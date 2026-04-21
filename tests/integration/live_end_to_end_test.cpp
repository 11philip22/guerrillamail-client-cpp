#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "guerrillamail/client.hpp"

namespace {

bool live_tests_enabled() {
    const auto* const raw_value = std::getenv("GUERRILLAMAIL_CPP_ENABLE_LIVE_TESTS");
    return raw_value != nullptr && std::string_view(raw_value) == "1";
}

void require_live_tests_enabled() {
    if (!live_tests_enabled()) {
        SKIP("set GUERRILLAMAIL_CPP_ENABLE_LIVE_TESTS=1 to run live GuerrillaMail validation");
    }
}

std::string make_alias() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return "gmcpplive" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

} // namespace

TEST_CASE("live create list and delete flow succeeds against GuerrillaMail", "[integration][live]") {
    require_live_tests_enabled();

    const auto alias = make_alias();
    const auto client = guerrillamail::Client::create();

    const auto email = client.create_email(alias);
    REQUIRE(email.find(alias + "@") == 0);

    const auto messages = client.get_messages(email);
    REQUIRE(messages.empty());

    REQUIRE(client.delete_email(email));
}
