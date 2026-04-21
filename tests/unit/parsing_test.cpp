#include <catch2/catch_test_macros.hpp>

#include "guerrillamail/error.hpp"
#include "protocol/parsing.hpp"

TEST_CASE("invalid json syntax becomes json_parse", "[parsing]") {
    try {
        (void)guerrillamail::protocol::parsing::parse_json("{");
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::json_parse);
    }
}

TEST_CASE("missing required member becomes response_parse", "[parsing]") {
    const auto json = guerrillamail::protocol::parsing::parse_json(R"({"ok":true})");

    try {
        (void)guerrillamail::protocol::parsing::require_string_member(json, "email_addr");
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::response_parse);
    }
}

TEST_CASE("wrong member type becomes response_parse", "[parsing]") {
    const auto json = guerrillamail::protocol::parsing::parse_json(R"({"email_addr":123})");

    try {
        (void)guerrillamail::protocol::parsing::require_string_member(json, "email_addr");
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::response_parse);
    }
}

TEST_CASE("valid message list parses into messages", "[parsing]") {
    const auto json = guerrillamail::protocol::parsing::parse_json(R"({"list":[{"mail_id":"1","mail_from":"from@example.com","mail_subject":"subject","mail_excerpt":"excerpt","mail_timestamp":"1700000000"}]})");
    const auto messages = guerrillamail::protocol::parsing::parse_message_list(json);

    REQUIRE(messages.size() == 1);
    REQUIRE(messages[0].mail_id == "1");
    REQUIRE(messages[0].mail_from == "from@example.com");
    REQUIRE(messages[0].mail_subject == "subject");
    REQUIRE(messages[0].mail_excerpt == "excerpt");
    REQUIRE(messages[0].mail_timestamp == "1700000000");
}

TEST_CASE("missing list becomes response_parse", "[parsing]") {
    const auto json = guerrillamail::protocol::parsing::parse_json(R"({"ok":true})");

    try {
        (void)guerrillamail::protocol::parsing::parse_message_list(json);
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::response_parse);
    }
}

TEST_CASE("non-array list becomes response_parse", "[parsing]") {
    const auto json = guerrillamail::protocol::parsing::parse_json(R"({"list":{}})");

    try {
        (void)guerrillamail::protocol::parsing::parse_message_list(json);
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::response_parse);
    }
}

TEST_CASE("malformed message entry becomes response_parse", "[parsing]") {
    const auto json = guerrillamail::protocol::parsing::parse_json(
        R"({"list":[{"mail_id":"1","mail_from":"from@example.com","mail_subject":"subject","mail_excerpt":123,"mail_timestamp":"1700000000"}]})"
    );

    try {
        (void)guerrillamail::protocol::parsing::parse_message_list(json);
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::response_parse);
    }
}
