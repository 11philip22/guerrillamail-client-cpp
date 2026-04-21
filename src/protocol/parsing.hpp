#pragma once

#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "guerrillamail/types.hpp"

namespace guerrillamail::protocol::parsing {

nlohmann::json parse_json(std::string_view input);
std::string require_string_member(const nlohmann::json& object, std::string_view key);
const nlohmann::json& require_array_member(const nlohmann::json& object, std::string_view key);
Message parse_message(const nlohmann::json& value);
std::vector<Message> parse_message_list(const nlohmann::json& object);

} // namespace guerrillamail::protocol::parsing
