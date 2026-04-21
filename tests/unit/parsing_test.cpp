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

TEST_CASE("email details parse without attachments", "[parsing]") {
    const auto json = guerrillamail::protocol::parsing::parse_json(
        R"({"mail_id":"1","mail_from":"from@example.com","mail_subject":"subject","mail_body":"<p>body</p>","mail_timestamp":"1700000000"})"
    );
    const auto details = guerrillamail::protocol::parsing::parse_email_details(json);

    REQUIRE(details.mail_id == "1");
    REQUIRE(details.mail_from == "from@example.com");
    REQUIRE(details.mail_subject == "subject");
    REQUIRE(details.mail_body == "<p>body</p>");
    REQUIRE(details.mail_timestamp == "1700000000");
    REQUIRE(details.attachments.empty());
    REQUIRE_FALSE(details.attachment_count.has_value());
    REQUIRE_FALSE(details.sid_token.has_value());
}

TEST_CASE("email details parse attachment metadata and optional fields", "[parsing]") {
    const auto json = guerrillamail::protocol::parsing::parse_json(
        R"({"mail_id":"1","mail_from":"from@example.com","mail_subject":"subject","mail_body":"<p>body</p>","mail_timestamp":"1700000000","att":"1","sid_token":"sid123","att_info":[{"f":"file.txt","t":"text/plain","p":"99"}]})"
    );
    const auto details = guerrillamail::protocol::parsing::parse_email_details(json);

    REQUIRE(details.attachments.size() == 1);
    REQUIRE(details.attachments[0].filename == "file.txt");
    REQUIRE(details.attachments[0].content_type_or_hint == std::optional<std::string>("text/plain"));
    REQUIRE(details.attachments[0].part_id == "99");
    REQUIRE(details.attachment_count == std::optional<std::uint32_t>(1));
    REQUIRE(details.sid_token == std::optional<std::string>("sid123"));
}

TEST_CASE("email details parse numeric attachment count", "[parsing]") {
    const auto json = guerrillamail::protocol::parsing::parse_json(
        R"({"mail_id":"1","mail_from":"from@example.com","mail_subject":"subject","mail_body":"<p>body</p>","mail_timestamp":"1700000000","att":1})"
    );
    const auto details = guerrillamail::protocol::parsing::parse_email_details(json);

    REQUIRE(details.attachment_count == std::optional<std::uint32_t>(1));
}

TEST_CASE("malformed attachment count becomes response_parse", "[parsing]") {
    const auto json = guerrillamail::protocol::parsing::parse_json(
        R"({"mail_id":"1","mail_from":"from@example.com","mail_subject":"subject","mail_body":"<p>body</p>","mail_timestamp":"1700000000","att":"oops"})"
    );

    try {
        (void)guerrillamail::protocol::parsing::parse_email_details(json);
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::response_parse);
    }
}

TEST_CASE("partially numeric attachment count becomes response_parse", "[parsing]") {
    const auto json = guerrillamail::protocol::parsing::parse_json(
        R"({"mail_id":"1","mail_from":"from@example.com","mail_subject":"subject","mail_body":"<p>body</p>","mail_timestamp":"1700000000","att":"1junk"})"
    );

    try {
        (void)guerrillamail::protocol::parsing::parse_email_details(json);
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::response_parse);
    }
}

TEST_CASE("malformed attachment list becomes response_parse", "[parsing]") {
    const auto json = guerrillamail::protocol::parsing::parse_json(
        R"({"mail_id":"1","mail_from":"from@example.com","mail_subject":"subject","mail_body":"<p>body</p>","mail_timestamp":"1700000000","att_info":{}})"
    );

    try {
        (void)guerrillamail::protocol::parsing::parse_email_details(json);
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::response_parse);
    }
}
