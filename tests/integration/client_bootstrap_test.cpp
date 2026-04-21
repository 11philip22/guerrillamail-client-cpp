#include <string>

#include <catch2/catch_test_macros.hpp>

#include "guerrillamail/client.hpp"
#include "guerrillamail/error.hpp"
#include "protocol/bootstrap.hpp"
#include "support/mock_http_server.hpp"
#include "transport/curl_session.hpp"

using guerrillamail::Client;
using guerrillamail::ClientOptions;
using guerrillamail::Attachment;
using guerrillamail::Error;
using guerrillamail::ErrorCode;
using guerrillamail::tests::support::MockHttpRequest;
using guerrillamail::tests::support::MockHttpResponse;
using guerrillamail::tests::support::MockHttpServer;
using guerrillamail::transport::CurlSession;
using guerrillamail::transport::HttpMethod;
using guerrillamail::transport::Request;

namespace {

std::string host_with_port_from_url(std::string_view url) {
    const auto scheme_end = url.find("://");
    const auto host_start = scheme_end == std::string_view::npos ? 0 : scheme_end + 3;
    const auto path_start = url.find('/', host_start);
    return std::string(url.substr(host_start, path_start - host_start));
}

std::string origin_from_url(std::string_view url) {
    const auto scheme_end = url.find("://");
    const auto path_start = url.find('/', scheme_end == std::string_view::npos ? 0 : scheme_end + 3);
    if (path_start == std::string_view::npos) {
        return std::string(url);
    }

    return std::string(url.substr(0, path_start));
}

std::string site_from_url(std::string_view url) {
    const auto host_with_port = host_with_port_from_url(url);

    std::string host = host_with_port;
    if (!host.empty() && host.front() == '[') {
        const auto closing = host.find(']');
        if (closing != std::string::npos) {
            host = host.substr(1, closing - 1);
        }
    } else {
        const auto separator = host.find(':');
        if (separator != std::string::npos) {
            host = host.substr(0, separator);
        }
    }

    constexpr std::string_view kCommonWwwPrefix = "www.";
    if (host.starts_with(kCommonWwwPrefix) && host.size() > kCommonWwwPrefix.size()) {
        host.erase(0, kCommonWwwPrefix.size());
    }

    return host;
}

} // namespace

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

TEST_CASE("client get_messages reuses bootstrap session and extracts alias from full email", "[bootstrap][integration]") {
    bool saw_cookie = false;
    bool saw_authorization = false;
    bool saw_x_requested_with = false;
    bool saw_empty_body = false;
    bool saw_expected_query = false;

    MockHttpServer server([&](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{
                200,
                {{"Set-Cookie", "sid=test123; Path=/"}},
                "<script>api_token : 'token123'</script>"
            };
        }

        if (request.path.find("/ajax.php?f=check_email") == 0) {
            saw_cookie = request.header_value("Cookie").value_or("").find("sid=test123") != std::string::npos;
            saw_authorization = request.header_value("Authorization").value_or("") == "ApiToken token123";
            saw_x_requested_with = request.header_value("X-Requested-With").value_or("") == "XMLHttpRequest";
            saw_empty_body = request.body.empty();
            saw_expected_query = request.path.find("f=check_email") != std::string::npos &&
                                 request.path.find("seq=1") != std::string::npos &&
                                 request.path.find("site=127.0.0.1") != std::string::npos &&
                                 request.path.find("in=myalias") != std::string::npos &&
                                 request.path.find("_=") != std::string::npos;

            return MockHttpResponse{200, {}, R"({"list":[{"mail_id":"1","mail_from":"from@example.com","mail_subject":"subject","mail_excerpt":"excerpt","mail_timestamp":"1700000000"}]})"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);
    const auto messages = client.get_messages("myalias@example.com");

    REQUIRE(messages.size() == 1);
    REQUIRE(messages[0].mail_id == "1");
    REQUIRE(messages[0].mail_from == "from@example.com");
    REQUIRE(messages[0].mail_subject == "subject");
    REQUIRE(messages[0].mail_excerpt == "excerpt");
    REQUIRE(messages[0].mail_timestamp == "1700000000");
    REQUIRE(saw_cookie);
    REQUIRE(saw_authorization);
    REQUIRE(saw_x_requested_with);
    REQUIRE(saw_empty_body);
    REQUIRE(saw_expected_query);
}

TEST_CASE("client get_messages rejects empty alias input", "[bootstrap][integration]") {
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
        (void)client.get_messages("@example.com");
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::invalid_argument);
    }
}

TEST_CASE("client get_messages maps missing list to response_parse", "[bootstrap][integration]") {
    MockHttpServer server([](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{200, {}, "<script>api_token : 'token123'</script>"};
        }

        if (request.path.find("/ajax.php?f=check_email") == 0) {
            return MockHttpResponse{200, {}, R"({"ok":true})"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);

    try {
        (void)client.get_messages("myalias@example.com");
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::response_parse);
    }
}

TEST_CASE("client get_messages uses explicit site override without changing endpoint-derived headers", "[bootstrap][integration]") {
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

        if (request.path.find("/ajax.php?f=check_email") == 0) {
            saw_override_site = request.path.find("site=guerrillamail.com") != std::string::npos;
            saw_host_header = request.header_value("Host").value_or("").find("127.0.0.1:") == 0;
            saw_origin_header = request.header_value("Origin").value_or("").find("http://127.0.0.1:") == 0;
            saw_cookie = request.header_value("Cookie").value_or("").find("sid=test123") != std::string::npos;
            return MockHttpResponse{200, {}, R"({"list":[{"mail_id":"1","mail_from":"from@example.com","mail_subject":"subject","mail_excerpt":"excerpt","mail_timestamp":"1700000000"}]})"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");
    options.site = "guerrillamail.com";

    const auto client = Client::create(options);
    const auto messages = client.get_messages("myalias@example.com");

    REQUIRE(messages.size() == 1);
    REQUIRE(saw_override_site);
    REQUIRE(saw_host_header);
    REQUIRE(saw_origin_header);
    REQUIRE(saw_cookie);
}

TEST_CASE("client fetch_email parses details and attachment metadata", "[bootstrap][integration]") {
    bool saw_cookie = false;
    bool saw_authorization = false;
    bool saw_x_requested_with = false;
    bool saw_empty_body = false;
    bool saw_expected_query = false;

    MockHttpServer server([&](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{
                200,
                {{"Set-Cookie", "sid=test123; Path=/"}},
                "<script>api_token : 'token123'</script>"
            };
        }

        if (request.path.find("/ajax.php?f=fetch_email") == 0) {
            saw_cookie = request.header_value("Cookie").value_or("").find("sid=test123") != std::string::npos;
            saw_authorization = request.header_value("Authorization").value_or("") == "ApiToken token123";
            saw_x_requested_with = request.header_value("X-Requested-With").value_or("") == "XMLHttpRequest";
            saw_empty_body = request.body.empty();
            saw_expected_query = request.path.find("f=fetch_email") != std::string::npos &&
                                 request.path.find("email_id=mail-123") != std::string::npos &&
                                 request.path.find("site=127.0.0.1") != std::string::npos &&
                                 request.path.find("in=myalias") != std::string::npos &&
                                 request.path.find("_=") != std::string::npos &&
                                 request.path.find("seq=") == std::string::npos;

            return MockHttpResponse{200, {}, R"({"mail_id":"mail-123","mail_from":"from@example.com","mail_subject":"subject","mail_body":"<p>body</p>","mail_timestamp":"1700000000","att":"1","sid_token":"sid123","att_info":[{"f":"file.txt","t":"text/plain","p":"99"}]})"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);
    const auto details = client.fetch_email("myalias@example.com", "mail-123");

    REQUIRE(details.mail_id == "mail-123");
    REQUIRE(details.mail_from == "from@example.com");
    REQUIRE(details.mail_subject == "subject");
    REQUIRE(details.mail_body == "<p>body</p>");
    REQUIRE(details.mail_timestamp == "1700000000");
    REQUIRE(details.attachment_count == std::optional<std::uint32_t>(1));
    REQUIRE(details.sid_token == std::optional<std::string>("sid123"));
    REQUIRE(details.attachments.size() == 1);
    REQUIRE(details.attachments[0].filename == "file.txt");
    REQUIRE(details.attachments[0].content_type_or_hint == std::optional<std::string>("text/plain"));
    REQUIRE(details.attachments[0].part_id == "99");
    REQUIRE(saw_cookie);
    REQUIRE(saw_authorization);
    REQUIRE(saw_x_requested_with);
    REQUIRE(saw_empty_body);
    REQUIRE(saw_expected_query);
}

TEST_CASE("client list_attachments returns parsed attachment metadata", "[bootstrap][integration]") {
    MockHttpServer server([](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{200, {{"Set-Cookie", "sid=test123; Path=/"}}, "<script>api_token : 'token123'</script>"};
        }

        if (request.path.find("/ajax.php?f=fetch_email") == 0) {
            return MockHttpResponse{200, {}, R"({"mail_id":"mail-123","mail_from":"from@example.com","mail_subject":"subject","mail_body":"<p>body</p>","mail_timestamp":"1700000000","att_info":[{"f":"file.txt","t":"text/plain","p":"99"}]})"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);
    const auto attachments = client.list_attachments("myalias@example.com", "mail-123");

    REQUIRE(attachments.size() == 1);
    REQUIRE(attachments[0].filename == "file.txt");
    REQUIRE(attachments[0].content_type_or_hint == std::optional<std::string>("text/plain"));
    REQUIRE(attachments[0].part_id == "99");
}

TEST_CASE("client fetch_email uses explicit site override without changing endpoint-derived headers", "[bootstrap][integration]") {
    bool saw_override_site = false;
    bool saw_host_header = false;
    bool saw_origin_header = false;
    bool saw_cookie = false;

    MockHttpServer server([&](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{200, {{"Set-Cookie", "sid=test123; Path=/"}}, "<script>api_token : 'token123'</script>"};
        }

        if (request.path.find("/ajax.php?f=fetch_email") == 0) {
            saw_override_site = request.path.find("site=guerrillamail.com") != std::string::npos;
            saw_host_header = request.header_value("Host").value_or("").find("127.0.0.1:") == 0;
            saw_origin_header = request.header_value("Origin").value_or("").find("http://127.0.0.1:") == 0;
            saw_cookie = request.header_value("Cookie").value_or("").find("sid=test123") != std::string::npos;
            return MockHttpResponse{200, {}, R"({"mail_id":"mail-123","mail_from":"from@example.com","mail_subject":"subject","mail_body":"<p>body</p>","mail_timestamp":"1700000000"})"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");
    options.site = "guerrillamail.com";

    const auto client = Client::create(options);
    const auto details = client.fetch_email("myalias@example.com", "mail-123");

    REQUIRE(details.mail_id == "mail-123");
    REQUIRE(saw_override_site);
    REQUIRE(saw_host_header);
    REQUIRE(saw_origin_header);
    REQUIRE(saw_cookie);
}

TEST_CASE("client fetch_email rejects empty mail id", "[bootstrap][integration]") {
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
        (void)client.fetch_email("myalias@example.com", "");
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::invalid_argument);
    }
}

TEST_CASE("client delete_email best-effort session cleanup reuses bootstrap session", "[bootstrap][integration]") {
    bool saw_cookie = false;
    bool saw_authorization = false;
    bool saw_x_requested_with = false;
    bool saw_post_method = false;
    bool saw_expected_query = false;
    std::string observed_body;

    MockHttpServer server([&](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{
                200,
                {{"Set-Cookie", "sid=test123; Path=/"}},
                "<script>api_token : 'token123'</script>"
            };
        }

        if (request.path.find("/ajax.php?f=forget_me") == 0) {
            saw_post_method = request.method == "POST";
            saw_expected_query = request.path == "/ajax.php?f=forget_me";
            saw_cookie = request.header_value("Cookie").value_or("").find("sid=test123") != std::string::npos;
            saw_authorization = request.header_value("Authorization").value_or("") == "ApiToken token123";
            saw_x_requested_with = request.header_value("X-Requested-With").value_or("") == "XMLHttpRequest";
            observed_body = request.body;
            return MockHttpResponse{204, {}, ""};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto expected_site = site_from_url(options.ajax_url);

    const auto client = Client::create(options);

    REQUIRE(client.delete_email("myalias@example.com"));
    REQUIRE(saw_post_method);
    REQUIRE(saw_expected_query);
    REQUIRE(saw_cookie);
    REQUIRE(saw_authorization);
    REQUIRE(saw_x_requested_with);
    REQUIRE(observed_body == "site=" + expected_site + "&in=myalias");
}

TEST_CASE("client delete_email uses explicit site override without changing endpoint-derived headers", "[bootstrap][integration]") {
    bool saw_cookie = false;
    std::string observed_body;
    std::string observed_host;
    std::string observed_origin;

    MockHttpServer server([&](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{200, {{"Set-Cookie", "sid=test123; Path=/"}}, "<script>api_token : 'token123'</script>"};
        }

        if (request.path.find("/ajax.php?f=forget_me") == 0) {
            observed_body = request.body;
            observed_host = request.header_value("Host").value_or("");
            observed_origin = request.header_value("Origin").value_or("");
            saw_cookie = request.header_value("Cookie").value_or("").find("sid=test123") != std::string::npos;
            return MockHttpResponse{200, {}, "ok"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");
    options.site = "guerrillamail.com";

    const auto expected_host = host_with_port_from_url(options.ajax_url);
    const auto expected_origin = origin_from_url(options.ajax_url);

    const auto client = Client::create(options);
    REQUIRE(client.delete_email("myalias@example.com"));
    REQUIRE(observed_body == "site=guerrillamail.com&in=myalias");
    REQUIRE(observed_host == expected_host);
    REQUIRE(observed_origin == expected_origin);
    REQUIRE(saw_cookie);
}

TEST_CASE("client delete_email rejects empty alias input", "[bootstrap][integration]") {
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
        client.delete_email("@example.com");
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::invalid_argument);
    }
}

TEST_CASE("client delete_email maps non-2xx to http_status", "[bootstrap][integration]") {
    MockHttpServer server([](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{200, {}, "<script>api_token : 'token123'</script>"};
        }

        if (request.path.find("/ajax.php?f=forget_me") == 0) {
            return MockHttpResponse{503, {}, "down"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);

    try {
        client.delete_email("myalias@example.com");
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::http_status);
        REQUIRE(error.http_status() == std::optional<long>(503));
    }
}

TEST_CASE("client fetch_attachment downloads binary body with sid_token when present", "[bootstrap][integration]") {
    bool saw_fetch_email = false;
    bool saw_download_query = false;
    bool saw_cookie = false;

    MockHttpServer server([&](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{200, {{"Set-Cookie", "sid=test123; Path=/"}}, "<script>api_token : 'token123'</script>"};
        }

        if (request.path.find("/ajax.php?f=fetch_email") == 0) {
            saw_fetch_email = true;
            return MockHttpResponse{200, {}, R"({"mail_id":"mail-123","mail_from":"from@example.com","mail_subject":"subject","mail_body":"<p>body</p>","mail_timestamp":"1700000000","sid_token":"sid123","att_info":[{"f":"file.bin","t":"application/octet-stream","p":"99"}]})"};
        }

        if (request.path.find("/inbox?") == 0) {
            saw_cookie = request.header_value("Cookie").value_or("").find("sid=test123") != std::string::npos;
            saw_download_query = request.path.find("get_att=") != std::string::npos &&
                                 request.path.find("lang=en") != std::string::npos &&
                                 request.path.find("email_id=mail-123") != std::string::npos &&
                                 request.path.find("part_id=99") != std::string::npos &&
                                 request.path.find("sid_token=sid123") != std::string::npos &&
                                 request.path.find("site=") == std::string::npos;

            return MockHttpResponse{200, {}, std::string("A\0B", 3)};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);
    const auto attachment = Attachment{"file.bin", std::optional<std::string>("application/octet-stream"), "99"};
    const auto bytes = client.fetch_attachment("myalias@example.com", "mail-123", attachment);

    REQUIRE(saw_fetch_email);
    REQUIRE(saw_download_query);
    REQUIRE(saw_cookie);
    REQUIRE(bytes == std::vector<std::uint8_t>{'A', 0, 'B'});
}

TEST_CASE("client fetch_attachment omits sid_token when absent", "[bootstrap][integration]" ) {
    bool saw_download_query = false;

    MockHttpServer server([&](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{200, {}, "<script>api_token : 'token123'</script>"};
        }

        if (request.path.find("/ajax.php?f=fetch_email") == 0) {
            return MockHttpResponse{200, {}, R"({"mail_id":"mail-123","mail_from":"from@example.com","mail_subject":"subject","mail_body":"<p>body</p>","mail_timestamp":"1700000000","att_info":[{"f":"file.bin","p":"99"}]})"};
        }

        if (request.path.find("/inbox?") == 0) {
            saw_download_query = request.path.find("sid_token=") == std::string::npos;
            return MockHttpResponse{200, {}, "hello"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);
    const auto attachment = Attachment{"file.bin", std::nullopt, "99"};
    const auto bytes = client.fetch_attachment("myalias@example.com", "mail-123", attachment);

    REQUIRE(saw_download_query);
    REQUIRE(bytes == std::vector<std::uint8_t>{'h', 'e', 'l', 'l', 'o'});
}

TEST_CASE("client fetch_attachment rejects missing part id as invalid_argument", "[bootstrap][integration]") {
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
    const auto attachment = Attachment{"file.bin", std::nullopt, ""};

    try {
        (void)client.fetch_attachment("myalias@example.com", "mail-123", attachment);
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::invalid_argument);
    }
}

TEST_CASE("client fetch_attachment maps non-2xx download to http_status", "[bootstrap][integration]") {
    MockHttpServer server([](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{200, {}, "<script>api_token : 'token123'</script>"};
        }

        if (request.path.find("/ajax.php?f=fetch_email") == 0) {
            return MockHttpResponse{200, {}, R"({"mail_id":"mail-123","mail_from":"from@example.com","mail_subject":"subject","mail_body":"<p>body</p>","mail_timestamp":"1700000000","att_info":[{"f":"file.bin","p":"99"}]})"};
        }

        if (request.path.find("/inbox?") == 0) {
            return MockHttpResponse{503, {}, "down"};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);
    const auto attachment = Attachment{"file.bin", std::nullopt, "99"};

    try {
        (void)client.fetch_attachment("myalias@example.com", "mail-123", attachment);
        FAIL("expected exception");
    } catch (const Error& error) {
        REQUIRE(error.code() == ErrorCode::http_status);
        REQUIRE(error.http_status() == std::optional<long>(503));
    }
}
