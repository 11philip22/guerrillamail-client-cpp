#pragma once

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
    std::string_view timestamp
);
[[nodiscard]] transport::Request build_check_email_probe_request(
    std::string_view ajax_url,
    std::string_view api_token,
    std::string_view email,
    std::string_view timestamp
);

} // namespace guerrillamail::protocol::requests
