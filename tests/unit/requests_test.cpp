#include <algorithm>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "guerrillamail/error.hpp"
#include "protocol/requests.hpp"
#include "transport/curl_session.hpp"

namespace {

std::string header_value(
    const std::vector<guerrillamail::transport::Header>& headers,
    std::string_view name
) {
    const auto iterator = std::find_if(headers.begin(), headers.end(), [name](const auto& header) {
        return header.name == name;
    });

    REQUIRE(iterator != headers.end());
    return iterator->value;
}

} // namespace

TEST_CASE("ajax headers match rust-aligned defaults", "[requests]") {
    const auto headers = guerrillamail::protocol::requests::build_ajax_headers(
        "https://mail.example.test:8443/ajax.php",
        "token123",
        false
    );

    REQUIRE(header_value(headers, "Host") == "mail.example.test:8443");
    REQUIRE(
        header_value(headers, "User-Agent") == guerrillamail::protocol::requests::default_user_agent()
    );
    REQUIRE(header_value(headers, "Accept") == "application/json, text/javascript, */*; q=0.01");
    REQUIRE(header_value(headers, "Accept-Language") == "en-US,en;q=0.5");
    REQUIRE(header_value(headers, "Authorization") == "ApiToken token123");
    REQUIRE(header_value(headers, "X-Requested-With") == "XMLHttpRequest");
    REQUIRE(header_value(headers, "Origin") == "https://mail.example.test:8443");
    REQUIRE(header_value(headers, "Referer") == "https://mail.example.test:8443/");
    REQUIRE(header_value(headers, "Sec-Fetch-Dest") == "empty");
    REQUIRE(header_value(headers, "Sec-Fetch-Mode") == "cors");
    REQUIRE(header_value(headers, "Sec-Fetch-Site") == "same-origin");
    REQUIRE(header_value(headers, "Priority") == "u=0");
    REQUIRE(
        std::none_of(headers.begin(), headers.end(), [](const auto& header) {
            return header.name == "Content-Type";
        })
    );
}

TEST_CASE("ajax headers include content type for form posts", "[requests]") {
    const auto headers = guerrillamail::protocol::requests::build_ajax_headers(
        "https://www.guerrillamail.com/ajax.php",
        "token123",
        true
    );

    REQUIRE(
        header_value(headers, "Content-Type") == "application/x-www-form-urlencoded; charset=UTF-8"
    );
}

TEST_CASE("check_email probe url mirrors rust query shape", "[requests]") {
    const auto url = guerrillamail::protocol::requests::build_check_email_probe_url(
        "https://www.guerrillamail.com/ajax.php",
        "alias@example.com",
        "1700000000000"
    );

    REQUIRE(
        url ==
        "https://www.guerrillamail.com/ajax.php?f=check_email&seq=1&site=guerrillamail.com&in=alias&_=1700000000000"
    );
}

TEST_CASE("check_email probe url derives site from configured ajax host", "[requests]") {
    const auto url = guerrillamail::protocol::requests::build_check_email_probe_url(
        "https://mail.example.test:8443/ajax.php",
        "alias@example.com",
        "1700000000000"
    );

    REQUIRE(
        url ==
        "https://mail.example.test:8443/ajax.php?f=check_email&seq=1&site=mail.example.test&in=alias&_=1700000000000"
    );
}

TEST_CASE("check_email probe request builds a GET with rust-aligned headers", "[requests]") {
    const auto request = guerrillamail::protocol::requests::build_check_email_probe_request(
        "https://www.guerrillamail.com/ajax.php",
        "token123",
        "alias@example.com",
        "1700000000000"
    );

    REQUIRE(request.method == guerrillamail::transport::HttpMethod::get);
    REQUIRE(request.body.empty());
    REQUIRE(
        request.url ==
        "https://www.guerrillamail.com/ajax.php?f=check_email&seq=1&site=guerrillamail.com&in=alias&_=1700000000000"
    );
    REQUIRE(header_value(request.headers, "Authorization") == "ApiToken token123");
}

TEST_CASE("check_email probe allows overriding only the query site value", "[requests]") {
    const auto request = guerrillamail::protocol::requests::build_check_email_probe_request(
        "https://mail.example.test:8443/ajax.php",
        "token123",
        "alias@example.com",
        "1700000000000",
        std::optional<std::string_view>("guerrillamail.com")
    );

    REQUIRE(
        request.url ==
        "https://mail.example.test:8443/ajax.php?f=check_email&seq=1&site=guerrillamail.com&in=alias&_=1700000000000"
    );
    REQUIRE(header_value(request.headers, "Host") == "mail.example.test:8443");
    REQUIRE(header_value(request.headers, "Origin") == "https://mail.example.test:8443");
    REQUIRE(header_value(request.headers, "Referer") == "https://mail.example.test:8443/");
}

TEST_CASE("check_email probe rejects an empty site override", "[requests]") {
    try {
        (void)guerrillamail::protocol::requests::build_check_email_probe_request(
            "https://mail.example.test:8443/ajax.php",
            "token123",
            "alias@example.com",
            "1700000000000",
            std::optional<std::string_view>("")
        );
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::invalid_argument);
    }
}

TEST_CASE("set_email_user request builds a POST with rust-aligned query body and headers", "[requests]") {
    const auto request = guerrillamail::protocol::requests::build_set_email_user_request(
        "https://mail.example.test:8443/ajax.php",
        "token123",
        "alias"
    );

    REQUIRE(request.method == guerrillamail::transport::HttpMethod::post);
    REQUIRE(request.url == "https://mail.example.test:8443/ajax.php?f=set_email_user");
    REQUIRE(
        request.body == "email_user=alias&lang=en&site=mail.example.test&in=%20Set%20cancel"
    );
    REQUIRE(header_value(request.headers, "Authorization") == "ApiToken token123");
    REQUIRE(
        header_value(request.headers, "Content-Type") == "application/x-www-form-urlencoded; charset=UTF-8"
    );
}

TEST_CASE("set_email_user request allows overriding only the form site value", "[requests]") {
    const auto request = guerrillamail::protocol::requests::build_set_email_user_request(
        "https://mail.example.test:8443/ajax.php",
        "token123",
        "alias",
        std::optional<std::string_view>("guerrillamail.com")
    );

    REQUIRE(
        request.body == "email_user=alias&lang=en&site=guerrillamail.com&in=%20Set%20cancel"
    );
    REQUIRE(header_value(request.headers, "Host") == "mail.example.test:8443");
    REQUIRE(header_value(request.headers, "Origin") == "https://mail.example.test:8443");
    REQUIRE(header_value(request.headers, "Referer") == "https://mail.example.test:8443/");
}

TEST_CASE("set_email_user request allows an empty alias intentionally", "[requests]") {
    const auto request = guerrillamail::protocol::requests::build_set_email_user_request(
        "https://www.guerrillamail.com/ajax.php",
        "token123",
        ""
    );

    REQUIRE(
        request.body == "email_user=&lang=en&site=guerrillamail.com&in=%20Set%20cancel"
    );
}

TEST_CASE("set_email_user request rejects an empty override", "[requests]") {
    try {
        (void)guerrillamail::protocol::requests::build_set_email_user_request(
            "https://mail.example.test:8443/ajax.php",
            "token123",
            "alias",
            std::optional<std::string_view>("")
        );
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::invalid_argument);
    }
}

TEST_CASE("extract_alias accepts alias-only input", "[requests]") {
    REQUIRE(guerrillamail::protocol::requests::extract_alias("alias") == "alias");
}

TEST_CASE("extract_alias returns the local-part of a full address", "[requests]") {
    REQUIRE(guerrillamail::protocol::requests::extract_alias("alias@example.com") == "alias");
}

TEST_CASE("extract_alias rejects empty alias input", "[requests]") {
    try {
        (void)guerrillamail::protocol::requests::extract_alias("@example.com");
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::invalid_argument);
    }
}

TEST_CASE("invalid ajax url becomes invalid_argument", "[requests]") {
    try {
        (void)guerrillamail::protocol::requests::build_ajax_headers("not-a-url", "token123", false);
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::invalid_argument);
    }
}

TEST_CASE("invalid ajax url probe becomes invalid_argument", "[requests]") {
    try {
        (void)guerrillamail::protocol::requests::build_check_email_probe_url(
            "not-a-url",
            "alias@example.com",
            "1700000000000"
        );
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::invalid_argument);
    }
}
