#include <catch2/catch_test_macros.hpp>

#include "guerrillamail/error.hpp"
#include "protocol/bootstrap.hpp"

TEST_CASE("bootstrap token extraction succeeds for expected html", "[bootstrap]") {
    const auto token = guerrillamail::protocol::bootstrap::extract_api_token(
        "<script>var config = { api_token : 'abc123' };</script>"
    );

    REQUIRE(token == "abc123");
}

TEST_CASE("missing bootstrap token becomes token_parse", "[bootstrap]") {
    try {
        (void)guerrillamail::protocol::bootstrap::extract_api_token("<html>missing token</html>");
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::token_parse);
    }
}

TEST_CASE("empty bootstrap token becomes token_parse", "[bootstrap]") {
    try {
        (void)guerrillamail::protocol::bootstrap::extract_api_token(
            "<script>api_token : ''</script>"
        );
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::token_parse);
    }
}

TEST_CASE("unextractable bootstrap token shape becomes token_parse", "[bootstrap]") {
    try {
        (void)guerrillamail::protocol::bootstrap::extract_api_token(
            "<script>api_token : \"abc123\"</script>"
        );
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::token_parse);
    }
}
