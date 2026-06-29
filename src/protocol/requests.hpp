#ifndef GUERRILLAMAIL_PROTOCOL_REQUESTS_HPP
#define GUERRILLAMAIL_PROTOCOL_REQUESTS_HPP

#include <optional>
#include <string_view>

#include "transport/curl_session.hpp"

namespace guerrillamail::protocol::requests {

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
[[nodiscard]] transport::Request build_forget_me_request(
    std::string_view ajax_url,
    std::string_view api_token,
    std::string_view email,
    std::optional<std::string_view> site_override = std::nullopt
);
[[nodiscard]] transport::Request build_fetch_attachment_request(
    std::string_view base_url,
    std::string_view api_token,
    std::string_view mail_id,
    std::string_view part_id,
    std::optional<std::string_view> sid_token = std::nullopt
);

} // namespace guerrillamail::protocol::requests

#endif // GUERRILLAMAIL_PROTOCOL_REQUESTS_HPP
