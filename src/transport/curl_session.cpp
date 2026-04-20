#include "transport/curl_session.hpp"

#include <curl/curl.h>

#include <array>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

#include "guerrillamail/error.hpp"

namespace guerrillamail::transport {

namespace {

void check_curl_code(CURLcode code, const char* operation) {
    if (code != CURLE_OK) {
        throw guerrillamail::Error(
            guerrillamail::ErrorCode::internal,
            std::string(operation) + " failed: " + curl_easy_strerror(code)
        );
    }
}

void ensure_curl_global_init() {
    static std::once_flag init_flag;
    static CURLcode init_result = CURLE_OK;

    std::call_once(init_flag, []() {
        init_result = curl_global_init(CURL_GLOBAL_DEFAULT);
    });

    if (init_result != CURLE_OK) {
        throw guerrillamail::Error(
            guerrillamail::ErrorCode::internal,
            std::string("curl_global_init failed: ") + curl_easy_strerror(init_result)
        );
    }
}

std::string describe_curl_failure(CURLcode code, const char* error_buffer) {
    if (error_buffer != nullptr && error_buffer[0] != '\0') {
        return error_buffer;
    }

    return curl_easy_strerror(code);
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    const auto bytes = size * nmemb;
    body->append(ptr, bytes);
    return bytes;
}

long validated_timeout_ms(std::chrono::milliseconds timeout) {
    if (timeout.count() < 0) {
        throw guerrillamail::Error(
            guerrillamail::ErrorCode::invalid_argument,
            "timeout must not be negative"
        );
    }

    if (timeout.count() > (std::numeric_limits<long>::max)()) {
        throw guerrillamail::Error(
            guerrillamail::ErrorCode::invalid_argument,
            "timeout is too large for libcurl"
        );
    }

    return static_cast<long>(timeout.count());
}

void apply_session_options(CURL* handle, const SessionOptions& options) {
    check_curl_code(curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L), "CURLOPT_NOSIGNAL");
    check_curl_code(
        curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, validated_timeout_ms(options.timeout)),
        "CURLOPT_TIMEOUT_MS"
    );
    check_curl_code(
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, options.verify_tls ? 1L : 0L),
        "CURLOPT_SSL_VERIFYPEER"
    );
    check_curl_code(
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, options.verify_tls ? 2L : 0L),
        "CURLOPT_SSL_VERIFYHOST"
    );

    if (options.proxy.has_value()) {
        check_curl_code(curl_easy_setopt(handle, CURLOPT_PROXY, options.proxy->c_str()), "CURLOPT_PROXY");
    } else {
        check_curl_code(curl_easy_setopt(handle, CURLOPT_PROXY, ""), "CURLOPT_PROXY");
    }
}

std::string make_http_status_message(long status_code) {
    std::ostringstream stream;
    stream << "http request failed with status " << status_code;
    return stream.str();
}

} // namespace

struct CurlSession::Impl {
    explicit Impl(SessionOptions options_in) : options(std::move(options_in)) {
        ensure_curl_global_init();

        handle = curl_easy_init();
        if (handle == nullptr) {
            throw guerrillamail::Error(
                guerrillamail::ErrorCode::internal,
                "curl_easy_init failed"
            );
        }

        check_curl_code(curl_easy_setopt(handle, CURLOPT_COOKIEFILE, ""), "CURLOPT_COOKIEFILE");
        apply_session_options(handle, options);
    }

    ~Impl() {
        if (handle != nullptr) {
            curl_easy_cleanup(handle);
        }
    }

    CURL* handle = nullptr;
    SessionOptions options;
};

CurlSession::CurlSession(SessionOptions options) : impl_(new Impl(std::move(options))) {}

CurlSession::~CurlSession() {
    delete impl_;
}

CurlSession::CurlSession(CurlSession&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

CurlSession& CurlSession::operator=(CurlSession&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }

    return *this;
}

Response CurlSession::execute(const Request& request) {
    if (impl_ == nullptr || impl_->handle == nullptr) {
        throw guerrillamail::Error(guerrillamail::ErrorCode::internal, "curl session is not initialized");
    }

    if (request.url.empty()) {
        throw guerrillamail::Error(guerrillamail::ErrorCode::invalid_argument, "request url must not be empty");
    }

    auto* const handle = impl_->handle;
    apply_session_options(handle, impl_->options);

    std::string response_body;
    std::array<char, CURL_ERROR_SIZE> error_buffer{};

    check_curl_code(curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, error_buffer.data()), "CURLOPT_ERRORBUFFER");
    check_curl_code(curl_easy_setopt(handle, CURLOPT_URL, request.url.c_str()), "CURLOPT_URL");
    check_curl_code(curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &write_callback), "CURLOPT_WRITEFUNCTION");
    check_curl_code(curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response_body), "CURLOPT_WRITEDATA");

    curl_slist* header_list = nullptr;
    for (const auto& header : request.headers) {
        const auto combined = header.name + ": " + header.value;
        auto* const next = curl_slist_append(header_list, combined.c_str());
        if (next == nullptr) {
            curl_slist_free_all(header_list);
            throw guerrillamail::Error(
                guerrillamail::ErrorCode::internal,
                "curl_slist_append failed"
            );
        }
        header_list = next;
    }
    check_curl_code(curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header_list), "CURLOPT_HTTPHEADER");

    switch (request.method) {
    case HttpMethod::get:
        check_curl_code(curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L), "CURLOPT_HTTPGET");
        check_curl_code(curl_easy_setopt(handle, CURLOPT_POST, 0L), "CURLOPT_POST");
        check_curl_code(curl_easy_setopt(handle, CURLOPT_POSTFIELDS, nullptr), "CURLOPT_POSTFIELDS");
        check_curl_code(
            curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(0)),
            "CURLOPT_POSTFIELDSIZE_LARGE"
        );
        break;
    case HttpMethod::post:
        check_curl_code(curl_easy_setopt(handle, CURLOPT_HTTPGET, 0L), "CURLOPT_HTTPGET");
        check_curl_code(curl_easy_setopt(handle, CURLOPT_POST, 1L), "CURLOPT_POST");
        check_curl_code(curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request.body.c_str()), "CURLOPT_POSTFIELDS");
        check_curl_code(
            curl_easy_setopt(
                handle,
                CURLOPT_POSTFIELDSIZE_LARGE,
                static_cast<curl_off_t>(request.body.size())
            ),
            "CURLOPT_POSTFIELDSIZE_LARGE"
        );
        break;
    }

    const auto result = curl_easy_perform(handle);

    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, nullptr);
    curl_slist_free_all(header_list);

    if (result != CURLE_OK) {
        throw guerrillamail::Error(
            guerrillamail::ErrorCode::transport,
            describe_curl_failure(result, error_buffer.data())
        );
    }

    long status_code = 0;
    check_curl_code(curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status_code), "CURLINFO_RESPONSE_CODE");

    if (status_code < 200 || status_code >= 300) {
        throw guerrillamail::Error(
            guerrillamail::ErrorCode::http_status,
            make_http_status_message(status_code),
            status_code
        );
    }

    return Response{status_code, std::move(response_body)};
}

} // namespace guerrillamail::transport
