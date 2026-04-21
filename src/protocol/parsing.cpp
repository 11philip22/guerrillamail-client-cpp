#include "protocol/parsing.hpp"

#include <string>

#include "guerrillamail/error.hpp"

namespace guerrillamail::protocol::parsing {

namespace {

[[noreturn]] void throw_response_parse(std::string message) {
    throw guerrillamail::Error(guerrillamail::ErrorCode::response_parse, std::move(message));
}

const nlohmann::json& require_member(const nlohmann::json& object, std::string_view key) {
    if (!object.is_object()) {
        throw_response_parse("expected JSON object");
    }

    const auto iterator = object.find(std::string(key));
    if (iterator == object.end()) {
        throw_response_parse("missing required member `" + std::string(key) + "`");
    }

    return *iterator;
}

} // namespace

nlohmann::json parse_json(std::string_view input) {
    try {
        return nlohmann::json::parse(input.begin(), input.end());
    } catch (const nlohmann::json::parse_error& error) {
        throw guerrillamail::Error(guerrillamail::ErrorCode::json_parse, error.what());
    }
}

std::string require_string_member(const nlohmann::json& object, std::string_view key) {
    const auto& value = require_member(object, key);
    if (!value.is_string()) {
        throw_response_parse("expected string member `" + std::string(key) + "`");
    }

    return value.get<std::string>();
}

const nlohmann::json& require_array_member(const nlohmann::json& object, std::string_view key) {
    const auto& value = require_member(object, key);
    if (!value.is_array()) {
        throw_response_parse("expected array member `" + std::string(key) + "`");
    }

    return value;
}

Message parse_message(const nlohmann::json& value) {
    if (!value.is_object()) {
        throw_response_parse("expected message object");
    }

    return Message{
        require_string_member(value, "mail_id"),
        require_string_member(value, "mail_from"),
        require_string_member(value, "mail_subject"),
        require_string_member(value, "mail_excerpt"),
        require_string_member(value, "mail_timestamp"),
    };
}

std::vector<Message> parse_message_list(const nlohmann::json& object) {
    const auto& list = require_array_member(object, "list");
    auto messages = std::vector<Message>{};
    messages.reserve(list.size());

    for (const auto& entry : list) {
        messages.push_back(parse_message(entry));
    }

    return messages;
}

} // namespace guerrillamail::protocol::parsing
