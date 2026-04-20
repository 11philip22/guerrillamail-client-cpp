#include <string>

#include <catch2/catch_test_macros.hpp>

#include "guerrillamail/client.hpp"
#include "guerrillamail/error.hpp"
#include "protocol/bootstrap.hpp"
#include "support/mock_http_server.hpp"
#include "transport/curl_session.hpp"

using guerrillamail::Client;
using guerrillamail::ClientOptions;
using guerrillamail::Error;
using guerrillamail::ErrorCode;
using guerrillamail::tests::support::MockHttpRequest;
using guerrillamail::tests::support::MockHttpResponse;
using guerrillamail::tests::support::MockHttpServer;
using guerrillamail::transport::CurlSession;
using guerrillamail::transport::HttpMethod;
using guerrillamail::transport::Request;

TEST_CASE("client create bootstraps successfully against mock server", "[bootstrap][integration]") {
    bool saw_user_agent = false;
    bool saw_accept_language = false;

    MockHttpServer server([&](const MockHttpRequest& request) {
        if (request.path == "/") {
            saw_user_agent = request.header_value("User-Agent").has_value();
            saw_accept_language = request.header_value("Accept-Language").has_value();
            return MockHttpResponse{
                200,
                {{"Set-Cookie", "sid=test123; Path=/"}},
                "<script>api_token : 'token123'</script>"
            };
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);

    (void)client;
    REQUIRE(saw_user_agent);
    REQUIRE(saw_accept_language);
}

TEST_CASE("client create maps bootstrap http failure to http_status", "[bootstrap][integration]") {
    MockHttpServer server([](const MockHttpRequest&) {
        return MockHttpResponse{503, {}, "down"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    try {
        (void)Client::create(options);
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::http_status);
        REQUIRE(error.http_status().has_value());
        REQUIRE(error.http_status().value() == 503);
    }
}

TEST_CASE("client create maps bootstrap transport failure to transport", "[bootstrap][integration]") {
    ClientOptions options;
    options.base_url = "http://127.0.0.1:1/";
    options.ajax_url = "http://127.0.0.1:1/ajax.php";

    try {
        (void)Client::create(options);
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::transport);
    }
}

TEST_CASE("client create maps bootstrap token failures to token_parse", "[bootstrap][integration]") {
    MockHttpServer server([](const MockHttpRequest&) {
        return MockHttpResponse{200, {}, "<html>no token here</html>"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    try {
        (void)Client::create(options);
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::token_parse);
    }
}

TEST_CASE("bootstrap preserves cookies for follow-up request on same session", "[bootstrap][integration]") {
    MockHttpServer server([](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{
                200,
                {{"Set-Cookie", "sid=test123; Path=/"}},
                "<script>api_token : 'cookie-token'</script>"
            };
        }

        const auto cookie = request.header_value("Cookie").value_or("");
        if (cookie.find("sid=test123") != std::string::npos) {
            return MockHttpResponse{200, {}, "cookie-present"};
        }

        return MockHttpResponse{400, {}, "cookie-missing"};
    });

    CurlSession session;
    const auto bootstrap = guerrillamail::protocol::bootstrap::perform(session, server.url("/"));
    const auto response = session.execute(Request{HttpMethod::get, server.url("/check-cookie")});

    REQUIRE(bootstrap.api_token == "cookie-token");
    REQUIRE(response.body == "cookie-present");
}
