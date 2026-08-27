#include "protocol/bootstrap.hpp"

#include <regex>
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

    const auto body = session.execute(transport::Request{
        transport::HttpMethod::get,
        std::string(base_url),
        build_bootstrap_headers(),
    });

    static const auto token_pattern = std::regex(R"(api_token\s*:\s*'([^']*)')");
    std::smatch match;

    if (!std::regex_search(body, match, token_pattern)) {
        throw guerrillamail::Error(
            guerrillamail::ErrorCode::token_parse,
            "api token not found in bootstrap HTML"
        );
    }
    if (match[1].length() == 0) {
        throw guerrillamail::Error(
            guerrillamail::ErrorCode::token_parse,
            "api token is empty"
        );
    }

    return match[1].str();
}

} // namespace guerrillamail::protocol::bootstrap
