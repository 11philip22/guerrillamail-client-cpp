#pragma once

#include <string_view>

#include <nlohmann/json.hpp>

namespace guerrillamail::protocol::parsing {

nlohmann::json parse_json(std::string_view input);
std::string require_string_member(const nlohmann::json& object, std::string_view key);

} // namespace guerrillamail::protocol::parsing
