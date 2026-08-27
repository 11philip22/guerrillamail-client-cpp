#ifndef GUERRILLAMAIL_PROTOCOL_BOOTSTRAP_HPP
#define GUERRILLAMAIL_PROTOCOL_BOOTSTRAP_HPP

#include <string>
#include <string_view>

#include "transport/curl_session.hpp"

namespace guerrillamail::protocol::bootstrap {

std::string perform(transport::CurlSession& session, std::string_view base_url);

} // namespace guerrillamail::protocol::bootstrap

#endif // GUERRILLAMAIL_PROTOCOL_BOOTSTRAP_HPP
