#include "protocol/bootstrap.hpp"

#include <string>
#include <vector>

#include "guerrillamail/error.hpp"

namespace guerrillamail::protocol::bootstrap {

namespace {

constexpr const char* kDefaultUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:131.0) Gecko/20100101 Firefox/131.0";

std::vector<transport::Header> build_bootstrap_headers() {
    return {
        transport::Header{"User-Agent", kDefaultUserAgent},
        transport::Header{"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"},
        transport::Header{"Accept-Language", "en-US,en;q=0.5"},
        transport::Header{"Upgrade-Insecure-Requests", "1"},
    };
}

} // namespace

std::string perform(transport::CurlSession& session, std::string_view base_url) {
    if (base_url.empty()) {
        throw guerrillamail::Error(
            guerrillamail::ErrorCode::invalid_argument,
            "base_url must not be empty"
        );
    }

    const auto response = session.execute(transport::Request{
        transport::HttpMethod::get,
        std::string(base_url),
        build_bootstrap_headers(),
    });

    return extract_api_token(response.body);
}

std::string extract_api_token(std::string_view html) {
    constexpr std::string_view marker = "api_token";

    for (auto key = html.find(marker); key != std::string_view::npos; key = html.find(marker, key + marker.size())) {
        auto value = key + marker.size();
        while (value < html.size() && (html[value] == ' ' || html[value] == '\t' || html[value] == '\n' || html[value] == '\r')) {
            ++value;
        }
        if (value == html.size() || html[value] != ':') {
            continue;
        }

        ++value;
        while (value < html.size() && (html[value] == ' ' || html[value] == '\t' || html[value] == '\n' || html[value] == '\r')) {
            ++value;
        }
        if (value == html.size() || html[value] != '\'') {
            continue;
        }

        const auto end = html.find('\'', value + 1);
        if (end == std::string_view::npos) {
            continue;
        }
        if (end == value + 1) {
            throw guerrillamail::Error(
                guerrillamail::ErrorCode::token_parse,
                "api token is empty"
            );
        }

        return std::string(html.substr(value + 1, end - value - 1));
    }

    throw guerrillamail::Error(
        guerrillamail::ErrorCode::token_parse,
        "api token not found in bootstrap HTML"
    );
}

} // namespace guerrillamail::protocol::bootstrap
