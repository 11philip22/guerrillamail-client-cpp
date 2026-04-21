#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "guerrillamail/client.hpp"
#include "guerrillamail/error.hpp"
#include "guerrillamail/types.hpp"
#include "support/mock_http_server.hpp"

using guerrillamail::Client;
using guerrillamail::ClientOptions;
using guerrillamail::tests::support::MockHttpRequest;
using guerrillamail::tests::support::MockHttpResponse;
using guerrillamail::tests::support::MockHttpServer;

TEST_CASE("public API end-to-end flow works against mock server", "[integration][public-api]") {
    bool saw_bootstrap_cookie = false;
    bool saw_attachment_download = false;
    bool saw_delete = false;

    MockHttpServer server([&](const MockHttpRequest& request) {
        if (request.path == "/") {
            return MockHttpResponse{
                200,
                {{"Set-Cookie", "sid=test123; Path=/"}},
                "<script>api_token : 'token123'</script>"
            };
        }

        const auto cookie = request.header_value("Cookie").value_or("");
        saw_bootstrap_cookie = saw_bootstrap_cookie || cookie.find("sid=test123") != std::string::npos;

        if (request.path.find("/ajax.php?f=set_email_user") == 0) {
            return MockHttpResponse{200, {}, R"({"email_addr":"demo@sharklasers.com"})"};
        }

        if (request.path.find("/ajax.php?f=check_email") == 0) {
            return MockHttpResponse{200, {}, R"({"list":[{"mail_id":"mail-123","mail_from":"from@example.com","mail_subject":"subject","mail_excerpt":"excerpt","mail_timestamp":"1700000000"}]})"};
        }

        if (request.path.find("/ajax.php?f=fetch_email") == 0) {
            return MockHttpResponse{200, {}, R"({"mail_id":"mail-123","mail_from":"from@example.com","mail_subject":"subject","mail_body":"<p>body</p>","mail_timestamp":"1700000000","att":"1","sid_token":"sid123","att_info":[{"f":"file.bin","t":"application/octet-stream","p":"99"}]})"};
        }

        if (request.path.find("/inbox?") == 0) {
            saw_attachment_download = request.path.find("sid_token=sid123") != std::string::npos;
            return MockHttpResponse{200, {}, std::string("A\0B", 3)};
        }

        if (request.path.find("/ajax.php?f=forget_me") == 0) {
            saw_delete = true;
            return MockHttpResponse{204, {}, ""};
        }

        return MockHttpResponse{400, {}, "unexpected"};
    });

    ClientOptions options;
    options.base_url = server.url("/");
    options.ajax_url = server.url("/ajax.php");

    const auto client = Client::create(options);
    const auto email = client.create_email("demo");
    const auto messages = client.get_messages(email);
    REQUIRE(messages.size() == 1);

    const auto details = client.fetch_email(email, messages[0].mail_id);
    REQUIRE(details.mail_id == "mail-123");
    REQUIRE(details.attachments.size() == 1);

    const auto attachments = client.list_attachments(email, messages[0].mail_id);
    REQUIRE(attachments.size() == 1);
    REQUIRE(attachments[0].filename == "file.bin");
    REQUIRE(attachments[0].content_type_or_hint == std::optional<std::string>("application/octet-stream"));
    REQUIRE(attachments[0].part_id == "99");

    const auto bytes = client.fetch_attachment(email, messages[0].mail_id, attachments[0]);
    REQUIRE(bytes == std::vector<std::uint8_t>{'A', 0, 'B'});

    REQUIRE(client.delete_email(email));

    REQUIRE(saw_bootstrap_cookie);
    REQUIRE(saw_attachment_download);
    REQUIRE(saw_delete);
}
