#include "guerrillamail/error.hpp"

#include <utility>

namespace guerrillamail {

Error::Error(ErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

ErrorCode Error::code() const noexcept {
    return code_;
}

} // namespace guerrillamail
