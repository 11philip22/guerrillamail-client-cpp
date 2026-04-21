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

TEST_CASE("client create_email reuses bootstrap session state for first ajax call", "[bootstrap][integration]") {
    bool saw_cookie = false;
    bool saw_authorization = false;
    bool saw_x_requested_with = false;
    bool saw_form_content_type = false;
    bool saw_post_method = false;
    bool saw_expected_query = false;
    bool saw_expected_body = false;

    MockHttpServer server([&](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{
                200,
                {{"Set-Cookie", "sid=test123; Path=/"}},
                "<script>api_token : 'token123'</script>"
            };
        }

        if (request.path.find("/ajax.php?f=set_email_user") == 0) {
            saw_post_method = request.method == "POST";
            saw_expected_query = request.path == "/ajax.php?f=set_email_user";
            saw_cookie = request.header_value("Cookie").value_or("").find("sid=test123") != std::string::npos;
            saw_authorization = request.header_value("Authorization").value_or("") == "ApiToken token123";
            saw_x_requested_with = request.header_value("X-Requested-With").value_or("") == "XMLHttpRequest";
            saw_form_content_type = request.header_value("Content-Type").value_or("") ==
                                    "application/x-www-form-urlencoded; charset=UTF-8";
            saw_expected_body = request.body.find("email_user=myalias") != std::string::npos &&
                                request.body.find("lang=en") != std::string::npos &&
                                request.body.find("site=127.0.0.1") != std::string::npos &&
                                request.body.find("in=%20Set%20cancel") != std::string::npos;

            return MockHttpResponse{200, {}, R"({"email_addr":"myalias@sharklasers.com"})"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);
    const auto email = client.create_email("myalias");

    REQUIRE(email == "myalias@sharklasers.com");
    REQUIRE(saw_post_method);
    REQUIRE(saw_expected_query);
    REQUIRE(saw_cookie);
    REQUIRE(saw_authorization);
    REQUIRE(saw_x_requested_with);
    REQUIRE(saw_form_content_type);
    REQUIRE(saw_expected_body);
}

TEST_CASE("client create_email allows an empty alias intentionally", "[bootstrap][integration]") {
    bool saw_empty_alias = false;

    MockHttpServer server([&](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{
                200,
                {{"Set-Cookie", "sid=test123; Path=/"}},
                "<script>api_token : 'token123'</script>"
            };
        }

        if (request.path.find("/ajax.php?f=set_email_user") == 0) {
            saw_empty_alias = request.body.find("email_user=&") != std::string::npos;
            return MockHttpResponse{200, {}, R"({"email_addr":"random@sharklasers.com"})"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);
    const auto email = client.create_email();

    REQUIRE(email == "random@sharklasers.com");
    REQUIRE(saw_empty_alias);
}

TEST_CASE("client create_email uses explicit site override without changing endpoint-derived headers", "[bootstrap][integration]") {
    bool saw_override_site = false;
    bool saw_host_header = false;
    bool saw_origin_header = false;
    bool saw_cookie = false;

    MockHttpServer server([&](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{
                200,
                {{"Set-Cookie", "sid=test123; Path=/"}},
                "<script>api_token : 'token123'</script>"
            };
        }

        if (request.path.find("/ajax.php?f=set_email_user") == 0) {
            saw_override_site = request.body.find("site=guerrillamail.com") != std::string::npos;
            saw_host_header = request.header_value("Host").value_or("").find("127.0.0.1:") == 0;
            saw_origin_header = request.header_value("Origin").value_or("").find("http://127.0.0.1:") == 0;
            saw_cookie = request.header_value("Cookie").value_or("").find("sid=test123") != std::string::npos;
            return MockHttpResponse{200, {}, R"({"email_addr":"myalias@sharklasers.com"})"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");
    options.site = "guerrillamail.com";

    const auto client = Client::create(options);
    const auto email = client.create_email("myalias");

    REQUIRE(email == "myalias@sharklasers.com");
    REQUIRE(saw_override_site);
    REQUIRE(saw_host_header);
    REQUIRE(saw_origin_header);
    REQUIRE(saw_cookie);
}

TEST_CASE("client create_email rejects aliases containing at-sign", "[bootstrap][integration]") {
    MockHttpServer server([](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{200, {}, "<script>api_token : 'token123'</script>"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);

    try {
        (void)client.create_email("bad@alias");
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::invalid_argument);
    }
}

TEST_CASE("client create_email rejects an empty explicit site override", "[bootstrap][integration]") {
    MockHttpServer server([](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{200, {}, "<script>api_token : 'token123'</script>"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");
    options.site = "";

    const auto client = Client::create(options);

    try {
        (void)client.create_email("myalias");
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::invalid_argument);
    }
}

TEST_CASE("client create_email maps missing email_addr to response_parse", "[bootstrap][integration]") {
    MockHttpServer server([](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{200, {}, "<script>api_token : 'token123'</script>"};
        }

        if (request.path.find("/ajax.php?f=set_email_user") == 0) {
            return MockHttpResponse{200, {}, R"({"ok":true})"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);

    try {
        (void)client.create_email("myalias");
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::response_parse);
    }
}

TEST_CASE("client create_email maps non-string email_addr to response_parse", "[bootstrap][integration]") {
    MockHttpServer server([](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{200, {}, "<script>api_token : 'token123'</script>"};
        }

        if (request.path.find("/ajax.php?f=set_email_user") == 0) {
            return MockHttpResponse{200, {}, R"({"email_addr":123})"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);

    try {
        (void)client.create_email("myalias");
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::response_parse);
    }
}
