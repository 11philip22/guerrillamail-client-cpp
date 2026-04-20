#include "protocol/requests.hpp"

#include <curl/urlapi.h>

#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "guerrillamail/error.hpp"

namespace guerrillamail::protocol::requests {

namespace {

constexpr std::string_view kDefaultUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:131.0) Gecko/20100101 Firefox/131.0";
constexpr std::string_view kAjaxAccept = "application/json, text/javascript, */*; q=0.01";
constexpr std::string_view kAcceptLanguage = "en-US,en;q=0.5";
constexpr std::string_view kFormContentType = "application/x-www-form-urlencoded; charset=UTF-8";

class ScopedCurlUrl {
public:
    ScopedCurlUrl() : handle_(curl_url()) {
        if (handle_ == nullptr) {
            throw guerrillamail::Error(
                guerrillamail::ErrorCode::internal,
                "curl_url failed"
            );
        }
    }

    ~ScopedCurlUrl() {
        if (handle_ != nullptr) {
            curl_url_cleanup(handle_);
        }
    }

    ScopedCurlUrl(const ScopedCurlUrl&) = delete;
    ScopedCurlUrl& operator=(const ScopedCurlUrl&) = delete;

    [[nodiscard]] CURLU* get() const noexcept {
        return handle_;
    }

private:
    CURLU* handle_ = nullptr;
};

struct AjaxUrlMetadata {
    std::string site;
    std::string host;
    std::string origin;
    std::string referer;
};

[[noreturn]] void throw_invalid_argument(std::string message) {
    throw guerrillamail::Error(guerrillamail::ErrorCode::invalid_argument, std::move(message));
}

std::string get_url_part(CURLU* handle, CURLUPart part) {
    char* raw = nullptr;
    const auto result = curl_url_get(handle, part, &raw, 0);
    if (result != CURLUE_OK) {
        throw_invalid_argument("ajax_url is not a valid absolute URL");
    }

    std::string value(raw);
    curl_free(raw);
    return value;
}

std::string derive_site_from_host(std::string_view host) {
    constexpr std::string_view kCommonWwwPrefix = "www.";
    if (host.starts_with(kCommonWwwPrefix) && host.size() > kCommonWwwPrefix.size()) {
        return std::string(host.substr(kCommonWwwPrefix.size()));
    }

    return std::string(host);
}

AjaxUrlMetadata parse_ajax_url(std::string_view ajax_url) {
    if (ajax_url.empty()) {
        throw_invalid_argument("ajax_url must not be empty");
    }

    const auto ajax_url_string = std::string(ajax_url);
    ScopedCurlUrl url;
    if (curl_url_set(url.get(), CURLUPART_URL, ajax_url_string.c_str(), 0) != CURLUE_OK) {
        throw_invalid_argument("ajax_url is not a valid absolute URL");
    }

    const auto scheme = get_url_part(url.get(), CURLUPART_SCHEME);
    const auto host = get_url_part(url.get(), CURLUPART_HOST);

    char* raw_port = nullptr;
    const auto port_result = curl_url_get(url.get(), CURLUPART_PORT, &raw_port, 0);

    std::string host_with_port = host;
    if (port_result == CURLUE_OK) {
        host_with_port += ":";
        host_with_port += raw_port;
        curl_free(raw_port);
    } else if (port_result != CURLUE_NO_PORT) {
        throw_invalid_argument("ajax_url is not a valid absolute URL");
    }

    const auto origin = scheme + "://" + host_with_port;
    return AjaxUrlMetadata{derive_site_from_host(host), host_with_port, origin, origin + "/"};
}

bool is_unreserved(unsigned char character) {
    return std::isalnum(character) != 0 || character == '-' || character == '.' || character == '_' ||
           character == '~';
}

std::string percent_encode(std::string_view input) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;

    for (const auto character : input) {
        const auto byte = static_cast<unsigned char>(character);
        if (is_unreserved(byte)) {
            encoded << character;
            continue;
        }

        encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }

    return encoded.str();
}

std::string build_query_string(const std::vector<std::pair<std::string_view, std::string>>& params) {
    std::ostringstream query;

    for (std::size_t index = 0; index < params.size(); ++index) {
        if (index != 0) {
            query << '&';
        }

        query << percent_encode(params[index].first) << '=' << percent_encode(params[index].second);
    }

    return query.str();
}

} // namespace

std::string_view default_user_agent() noexcept {
    return kDefaultUserAgent;
}

std::string extract_alias(std::string_view email) {
    const auto separator = email.find('@');
    const auto alias = email.substr(0, separator);
    if (alias.empty()) {
        throw_invalid_argument("email alias must not be empty");
    }

    return std::string(alias);
}

std::vector<transport::Header> build_ajax_headers(
    std::string_view ajax_url,
    std::string_view api_token,
    bool include_content_type
) {
    if (api_token.empty()) {
        throw_invalid_argument("api_token must not be empty");
    }

    const auto metadata = parse_ajax_url(ajax_url);

    auto headers = std::vector<transport::Header>{
        transport::Header{"Host", metadata.host},
        transport::Header{"User-Agent", std::string(default_user_agent())},
        transport::Header{"Accept", std::string(kAjaxAccept)},
        transport::Header{"Accept-Language", std::string(kAcceptLanguage)},
        transport::Header{"Authorization", "ApiToken " + std::string(api_token)},
        transport::Header{"X-Requested-With", "XMLHttpRequest"},
        transport::Header{"Origin", metadata.origin},
        transport::Header{"Referer", metadata.referer},
        transport::Header{"Sec-Fetch-Dest", "empty"},
        transport::Header{"Sec-Fetch-Mode", "cors"},
        transport::Header{"Sec-Fetch-Site", "same-origin"},
        transport::Header{"Priority", "u=0"},
    };

    if (include_content_type) {
        headers.insert(
            headers.begin() + 4,
            transport::Header{"Content-Type", std::string(kFormContentType)}
        );
    }

    return headers;
}

std::string build_check_email_probe_url(
    std::string_view ajax_url,
    std::string_view email,
    std::string_view timestamp
) {
    if (ajax_url.empty()) {
        throw_invalid_argument("ajax_url must not be empty");
    }
    if (timestamp.empty()) {
        throw_invalid_argument("timestamp must not be empty");
    }

    const auto metadata = parse_ajax_url(ajax_url);
    const auto alias = extract_alias(email);
    const auto query = build_query_string({
        {"f", "check_email"},
        {"seq", "1"},
        {"site", metadata.site},
        {"in", alias},
        {"_", std::string(timestamp)},
    });

    auto url = std::string(ajax_url);
    url += (url.find('?') == std::string::npos) ? '?' : '&';
    url += query;
    return url;
}

transport::Request build_check_email_probe_request(
    std::string_view ajax_url,
    std::string_view api_token,
    std::string_view email,
    std::string_view timestamp
) {
    return transport::Request{
        transport::HttpMethod::get,
        build_check_email_probe_url(ajax_url, email, timestamp),
        build_ajax_headers(ajax_url, api_token, false),
        {},
    };
}

} // namespace guerrillamail::protocol::requests
