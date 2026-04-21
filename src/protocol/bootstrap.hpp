#ifndef GUERRILLAMAIL_PROTOCOL_BOOTSTRAP_HPP
#define GUERRILLAMAIL_PROTOCOL_BOOTSTRAP_HPP

#include <string>
#include <string_view>

#include "transport/curl_session.hpp"

namespace guerrillamail::protocol::bootstrap {

struct BootstrapResult {
    std::string api_token;
};

BootstrapResult perform(transport::CurlSession& session, std::string_view base_url);
std::string extract_api_token(std::string_view html);

} // namespace guerrillamail::protocol::bootstrap

#endif // GUERRILLAMAIL_PROTOCOL_BOOTSTRAP_HPP
