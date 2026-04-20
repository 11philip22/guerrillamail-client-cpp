#include "transport/curl_session.hpp"

#include <curl/curl.h>

#include <array>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

#include "guerrillamail/error.hpp"

namespace guerrillamail::transport {

namespace {

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

void apply_session_options(CURL* handle, const SessionOptions& options) {
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, static_cast<long>(options.timeout.count()));
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, options.verify_tls ? 1L : 0L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, options.verify_tls ? 2L : 0L);

    if (options.proxy.has_value()) {
        curl_easy_setopt(handle, CURLOPT_PROXY, options.proxy->c_str());
    } else {
        curl_easy_setopt(handle, CURLOPT_PROXY, "");
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

        curl_easy_setopt(handle, CURLOPT_COOKIEFILE, "");
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

    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, error_buffer.data());
    curl_easy_setopt(handle, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &write_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response_body);

    curl_slist* header_list = nullptr;
    for (const auto& header : request.headers) {
        const auto combined = header.name + ": " + header.value;
        header_list = curl_slist_append(header_list, combined.c_str());
    }
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header_list);

    switch (request.method) {
    case HttpMethod::get:
        curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(handle, CURLOPT_POST, 0L);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, nullptr);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(0));
        break;
    case HttpMethod::post:
        curl_easy_setopt(handle, CURLOPT_HTTPGET, 0L);
        curl_easy_setopt(handle, CURLOPT_POST, 1L);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request.body.c_str());
        curl_easy_setopt(
            handle,
            CURLOPT_POSTFIELDSIZE_LARGE,
            static_cast<curl_off_t>(request.body.size())
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
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status_code);

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
