#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include "guerrillamail/error.hpp"
#include "support/mock_http_server.hpp"
#include "transport/curl_session.hpp"

using guerrillamail::Error;
using guerrillamail::ErrorCode;
using guerrillamail::tests::support::MockHttpRequest;
using guerrillamail::tests::support::MockHttpResponse;
using guerrillamail::tests::support::MockHttpServer;
using guerrillamail::transport::CurlSession;
using guerrillamail::transport::Header;
using guerrillamail::transport::HttpMethod;
using guerrillamail::transport::Request;
using guerrillamail::transport::SessionOptions;

TEST_CASE("curl session returns status and body for success", "[transport]") {
    MockHttpServer server([](const MockHttpRequest&) {
        return MockHttpResponse{200, {}, "hello"};
    });

    CurlSession session;
    const auto response = session.execute(Request{HttpMethod::get, server.url("ok")});

    REQUIRE(response.status_code == 200);
    REQUIRE(response.body == "hello");
}

TEST_CASE("empty request url becomes invalid_argument", "[transport]") {
    CurlSession session;

    try {
        (void)session.execute(Request{});
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::invalid_argument);
    }
}

TEST_CASE("curl session sends custom headers", "[transport]") {
    MockHttpServer server([](const MockHttpRequest& request) {
        return MockHttpResponse{200, {}, request.header_value("X-Test").value_or("missing")};
    });

    CurlSession session;
    const auto response = session.execute(Request{
        HttpMethod::get,
        server.url("echo-header"),
        {Header{"X-Test", "demo"}}
    });

    REQUIRE(response.body == "demo");
}

TEST_CASE("curl session sends post method and body", "[transport]") {
    MockHttpServer server([](const MockHttpRequest& request) {
        return MockHttpResponse{200, {}, request.method + ":" + request.body};
    });

    CurlSession session;
    const auto response = session.execute(Request{
        HttpMethod::post,
        server.url("submit"),
        {Header{"Content-Type", "application/x-www-form-urlencoded"}},
        "alpha=beta"
    });

    REQUIRE(response.body == "POST:alpha=beta");
}

TEST_CASE("cookies persist across requests in one session", "[transport]") {
    MockHttpServer server([](const MockHttpRequest& request) {
        if (request.path == "/set-cookie") {
            return MockHttpResponse{200, {{"Set-Cookie", "sid=test123; Path=/"}}, "set"};
        }

        const auto cookie = request.header_value("Cookie").value_or("");
        if (cookie.find("sid=test123") != std::string::npos) {
            return MockHttpResponse{200, {}, "cookie-present"};
        }

        return MockHttpResponse{400, {}, "cookie-missing"};
    });

    CurlSession session;
    const auto first = session.execute(Request{HttpMethod::get, server.url("set-cookie")});
    const auto second = session.execute(Request{HttpMethod::get, server.url("check-cookie")});

    REQUIRE(first.body == "set");
    REQUIRE(second.body == "cookie-present");
}

TEST_CASE("non-2xx responses become http_status", "[transport]") {
    MockHttpServer server([](const MockHttpRequest&) {
        return MockHttpResponse{503, {}, "down"};
    });

    CurlSession session;

    try {
        (void)session.execute(Request{HttpMethod::get, server.url("status-503")});
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::http_status);
        REQUIRE(error.http_status().has_value());
        REQUIRE(error.http_status().value() == 503);
    }
}

TEST_CASE("timeout becomes transport", "[transport]") {
    MockHttpServer server([](const MockHttpRequest&) {
        return MockHttpResponse{200, {}, "slow", std::chrono::milliseconds(200)};
    });

    CurlSession session(SessionOptions{std::chrono::milliseconds(50)});

    try {
        (void)session.execute(Request{HttpMethod::get, server.url("slow")});
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::transport);
    }
}

TEST_CASE("negative timeout becomes invalid_argument", "[transport]") {
    try {
        CurlSession session(SessionOptions{std::chrono::milliseconds(-1)});
        (void)session;
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::invalid_argument);
    }
}

TEST_CASE("connection failure becomes transport", "[transport]") {
    CurlSession session;

    try {
        (void)session.execute(Request{HttpMethod::get, "http://127.0.0.1:1/unreachable"});
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::transport);
    }
}

TEST_CASE("proxy failure becomes transport", "[transport]") {
    MockHttpServer server([](const MockHttpRequest&) {
        return MockHttpResponse{200, {}, "ok"};
    });

    CurlSession session(SessionOptions{
        std::chrono::milliseconds(1000),
        std::string("http://127.0.0.1:1"),
        true,
    });

    try {
        (void)session.execute(Request{HttpMethod::get, server.url("proxy-target")});
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::transport);
    }
}

TEST_CASE("tls verification failure becomes transport", "[transport]") {
    MockHttpServer server([](const MockHttpRequest&) {
        return MockHttpResponse{200, {}, "plain-http"};
    });

    CurlSession session(SessionOptions{
        std::chrono::milliseconds(1000),
        std::nullopt,
        true,
    });

    try {
        (void)session.execute(Request{HttpMethod::get, server.url("tls-mismatch", "https")});
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::transport);
    }
}
