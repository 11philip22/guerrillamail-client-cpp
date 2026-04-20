#pragma once

#include <optional>
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
    Error(ErrorCode code, std::string message, std::optional<long> http_status = std::nullopt);

    [[nodiscard]] ErrorCode code() const noexcept;
    [[nodiscard]] std::optional<long> http_status() const noexcept;

private:
    ErrorCode code_;
    std::optional<long> http_status_;
};

} // namespace guerrillamail
