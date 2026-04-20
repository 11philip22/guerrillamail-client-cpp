#pragma once

#include <stdexcept>
#include <string>

namespace guerrillamail {

enum class ErrorCode {
    invalid_argument,
    transport,
    http_status,
    token_parse,
    response_parse,
    json_parse,
    internal,
};

class Error : public std::runtime_error {
public:
    Error(ErrorCode code, std::string message);

    [[nodiscard]] ErrorCode code() const noexcept;

private:
    ErrorCode code_;
};

} // namespace guerrillamail
