#include "guerrillamail/error.hpp"

#include <optional>
#include <utility>

namespace guerrillamail {

Error::Error(ErrorCode code, std::string message, std::optional<long> http_status)
    : std::runtime_error(std::move(message)), code_(code), http_status_(http_status) {}

ErrorCode Error::code() const noexcept {
    return code_;
}

std::optional<long> Error::http_status() const noexcept {
    return http_status_;
}

} // namespace guerrillamail
