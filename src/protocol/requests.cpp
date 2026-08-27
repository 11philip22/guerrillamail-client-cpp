#include "protocol/requests.hpp"

#include <curl/curl.h>
#include <curl/urlapi.h>

#include <optional>
#include <limits>
#include <memory>
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

using CurlUrlPtr = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>;
using CurlStringPtr = std::unique_ptr<char, decltype(&curl_free)>;

struct AjaxUrlMetadata {
    std::string site;
    std::string host;
    std::string origin;
    std::string referer;
};

[[noreturn]] void throw_invalid_argument(std::string message) {
    throw guerrillamail::Error(guerrillamail::ErrorCode::invalid_argument, std::move(message));
}

CurlUrlPtr make_curl_url() {
    auto url = CurlUrlPtr(curl_url(), curl_url_cleanup);
    if (url == nullptr) {
        throw guerrillamail::Error(
            guerrillamail::ErrorCode::internal,
            "curl_url failed"
        );
    }
    return url;
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
    const auto url = make_curl_url();
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

std::string percent_encode(std::string_view input) {
    if (input.empty()) {
        return {};
    }
    if (input.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        throw_invalid_argument("query parameter is too large to encode");
    }

    auto escaped = CurlStringPtr(
        curl_easy_escape(nullptr, input.data(), static_cast<int>(input.size())),
        curl_free
    );
    if (escaped == nullptr) {
        throw guerrillamail::Error(
            guerrillamail::ErrorCode::internal,
            "curl_easy_escape failed"
        );
    }

    return escaped.get();
}

std::string build_query_string(const std::vector<std::pair<std::string_view, std::string>>& params) {
    std::string query;
    for (const auto& [key, value] : params) {
        if (!query.empty()) {
            query.push_back('&');
        }
        query.append(percent_encode(key)).append("=").append(percent_encode(value));
    }
    return query;
}

std::string append_query(std::string_view base_url, const std::vector<std::pair<std::string_view, std::string>>& params) {
    auto url = std::string(base_url);
    url += (url.find('?') == std::string::npos) ? '?' : '&';
    url += build_query_string(params);
    return url;
}

std::string trim_trailing_slash(std::string value) {
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }

    return value;
}

std::string resolve_site_form_value(
    const AjaxUrlMetadata& metadata,
    std::optional<std::string_view> site_override
) {
    if (site_override.has_value()) {
        if (site_override->empty()) {
            throw_invalid_argument("site override must not be empty");
        }

        return std::string(*site_override);
    }

    return metadata.site;
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
    const AjaxUrlMetadata& metadata,
    std::string_view api_token,
    bool include_content_type
) {
    if (api_token.empty()) {
        throw_invalid_argument("api_token must not be empty");
    }

    auto headers = std::vector<transport::Header>{
        transport::Header{"Host", metadata.host},
        transport::Header{"User-Agent", std::string(kDefaultUserAgent)},
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

} // namespace

transport::Request build_check_email_probe_request(
    std::string_view ajax_url,
    std::string_view api_token,
    std::string_view email,
    std::string_view timestamp,
    std::optional<std::string_view> site_override
) {
    if (timestamp.empty()) {
        throw_invalid_argument("timestamp must not be empty");
    }

    const auto metadata = parse_ajax_url(ajax_url);
    const auto alias = extract_alias(email);

    return transport::Request{
        transport::HttpMethod::get,
        append_query(ajax_url, {
            {"f", "check_email"},
            {"seq", "1"},
            {"site", resolve_site_form_value(metadata, site_override)},
            {"in", alias},
            {"_", std::string(timestamp)},
        }),
        build_ajax_headers(metadata, api_token, false),
        {},
    };
}

transport::Request build_set_email_user_request(
    std::string_view ajax_url,
    std::string_view api_token,
    std::string_view alias,
    std::optional<std::string_view> site_override
) {
    if (ajax_url.empty()) {
        throw_invalid_argument("ajax_url must not be empty");
    }

    const auto metadata = parse_ajax_url(ajax_url);

    return transport::Request{
        transport::HttpMethod::post,
        append_query(ajax_url, {{"f", "set_email_user"}}),
        build_ajax_headers(metadata, api_token, true),
        build_query_string({
            {"email_user", std::string(alias)},
            {"lang", "en"},
            // Keep the form `site` aligned with the configured endpoint by default, but allow an
            // explicit compatibility override without changing Host/Origin/Referer metadata.
            {"site", resolve_site_form_value(metadata, site_override)},
            {"in", " Set cancel"},
        }),
    };
}

transport::Request build_fetch_email_request(
    std::string_view ajax_url,
    std::string_view api_token,
    std::string_view email,
    std::string_view mail_id,
    std::string_view timestamp,
    std::optional<std::string_view> site_override
) {
    if (mail_id.empty()) {
        throw_invalid_argument("mail_id must not be empty");
    }
    if (timestamp.empty()) {
        throw_invalid_argument("timestamp must not be empty");
    }

    const auto metadata = parse_ajax_url(ajax_url);
    const auto alias = extract_alias(email);

    return transport::Request{
        transport::HttpMethod::get,
        append_query(ajax_url, {
            {"f", "fetch_email"},
            {"email_id", std::string(mail_id)},
            {"site", resolve_site_form_value(metadata, site_override)},
            {"in", alias},
            {"_", std::string(timestamp)},
        }),
        build_ajax_headers(metadata, api_token, false),
        {},
    };
}

transport::Request build_forget_me_request(
    std::string_view ajax_url,
    std::string_view api_token,
    std::string_view email,
    std::optional<std::string_view> site_override
) {
    if (ajax_url.empty()) {
        throw_invalid_argument("ajax_url must not be empty");
    }

    const auto metadata = parse_ajax_url(ajax_url);
    const auto alias = extract_alias(email);

    return transport::Request{
        transport::HttpMethod::post,
        append_query(ajax_url, {{"f", "forget_me"}}),
        build_ajax_headers(metadata, api_token, true),
        build_query_string({
            {"site", resolve_site_form_value(metadata, site_override)},
            {"in", alias},
        }),
    };
}

transport::Request build_fetch_attachment_request(
    std::string_view base_url,
    std::string_view api_token,
    std::string_view mail_id,
    std::string_view part_id,
    std::optional<std::string_view> sid_token
) {
    if (base_url.empty()) {
        throw_invalid_argument("base_url must not be empty");
    }
    if (mail_id.empty()) {
        throw_invalid_argument("mail_id must not be empty");
    }
    if (part_id.empty()) {
        throw_invalid_argument("part_id must not be empty");
    }

    auto inbox_url = trim_trailing_slash(std::string(base_url));
    inbox_url += "/inbox";
    inbox_url += '?';

    std::vector<std::pair<std::string_view, std::string>> query = {
        {"get_att", ""},
        {"lang", "en"},
        {"email_id", std::string(mail_id)},
        {"part_id", std::string(part_id)},
    };

    if (sid_token.has_value() && !sid_token->empty()) {
        query.emplace_back("sid_token", std::string(*sid_token));
    }

    inbox_url += build_query_string(query);
    const auto metadata = parse_ajax_url(base_url);

    return transport::Request{
        transport::HttpMethod::get,
        std::move(inbox_url),
        build_ajax_headers(metadata, api_token, true),
        {},
    };
}

} // namespace guerrillamail::protocol::requests
