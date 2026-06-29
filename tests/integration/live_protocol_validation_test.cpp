#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "guerrillamail/client.hpp"
#include "guerrillamail/error.hpp"
#include "protocol/bootstrap.hpp"
#include "protocol/parsing.hpp"
#include "protocol/requests.hpp"
#include "transport/curl_session.hpp"

namespace {

constexpr std::string_view kLiveBaseUrl = "https://www.guerrillamail.com";
constexpr std::string_view kLiveAjaxUrl = "https://www.guerrillamail.com/ajax.php";

std::string make_timestamp() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

std::string make_probe_alias() {
    return "gmcppwol172" + make_timestamp();
}

bool has_message_list(const nlohmann::json& json) {
    return json.is_object() && json.contains("list") && json.at("list").is_array();
}

} // namespace

TEST_CASE("live bootstrap and ajax probe validate current GuerrillaMail behavior", "[integration][live]") {
    const auto* const live_tests = std::getenv("GUERRILLAMAIL_CPP_ENABLE_LIVE_TESTS");
    if (live_tests == nullptr || std::string_view(live_tests) != "1") {
        SKIP("set GUERRILLAMAIL_CPP_ENABLE_LIVE_TESTS=1 to run live GuerrillaMail validation");
    }

    guerrillamail::ClientOptions options;
    options.base_url = std::string(kLiveBaseUrl);
    options.ajax_url = std::string(kLiveAjaxUrl);

    REQUIRE_NOTHROW((void)guerrillamail::Client::create(options));

    guerrillamail::transport::CurlSession bootstrapped_session;
    const auto api_token = guerrillamail::protocol::bootstrap::perform(bootstrapped_session, kLiveBaseUrl);
    REQUIRE_FALSE(api_token.empty());

    const auto alias = make_probe_alias();
    const auto same_session_request = guerrillamail::protocol::requests::build_check_email_probe_request(
        kLiveAjaxUrl,
        api_token,
        alias,
        make_timestamp()
    );
    const auto same_session_body = bootstrapped_session.execute(same_session_request);
    const auto same_session_json = guerrillamail::protocol::parsing::parse_json(same_session_body);
    REQUIRE(has_message_list(same_session_json));

    bool fresh_session_succeeded = false;
    std::string fresh_session_outcome;

    try {
        guerrillamail::transport::CurlSession fresh_session;
        const auto fresh_session_request = guerrillamail::protocol::requests::build_check_email_probe_request(
            kLiveAjaxUrl,
            api_token,
            alias,
            make_timestamp()
        );
        const auto fresh_session_body = fresh_session.execute(fresh_session_request);
        const auto fresh_session_json = guerrillamail::protocol::parsing::parse_json(fresh_session_body);
        fresh_session_succeeded = has_message_list(fresh_session_json);
        fresh_session_outcome = fresh_session_succeeded
                                    ? "fresh-session probe succeeded without bootstrap cookies"
                                    : "fresh-session probe returned JSON without `list`: " + fresh_session_body;
    } catch (const guerrillamail::Error& error) {
        fresh_session_outcome = error.what();
    }

    INFO(fresh_session_outcome);
    REQUIRE_FALSE(fresh_session_outcome.empty());
}
