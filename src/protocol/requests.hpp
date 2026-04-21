#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "transport/curl_session.hpp"

namespace guerrillamail::protocol::requests {

[[nodiscard]] std::string_view default_user_agent() noexcept;
[[nodiscard]] std::string extract_alias(std::string_view email);
[[nodiscard]] std::vector<transport::Header> build_ajax_headers(
    std::string_view ajax_url,
    std::string_view api_token,
    bool include_content_type
);
[[nodiscard]] std::string build_check_email_probe_url(
    std::string_view ajax_url,
    std::string_view email,
    std::string_view timestamp,
    std::optional<std::string_view> site_override = std::nullopt
);
[[nodiscard]] transport::Request build_check_email_probe_request(
    std::string_view ajax_url,
    std::string_view api_token,
    std::string_view email,
    std::string_view timestamp,
    std::optional<std::string_view> site_override = std::nullopt
);
[[nodiscard]] transport::Request build_set_email_user_request(
    std::string_view ajax_url,
    std::string_view api_token,
    std::string_view alias,
    std::optional<std::string_view> site_override = std::nullopt
);
[[nodiscard]] transport::Request build_fetch_email_request(
    std::string_view ajax_url,
    std::string_view api_token,
    std::string_view email,
    std::string_view mail_id,
    std::string_view timestamp,
    std::optional<std::string_view> site_override = std::nullopt
);

} // namespace guerrillamail::protocol::requests
