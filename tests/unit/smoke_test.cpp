#include <filesystem>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "guerrillamail/client.hpp"
#include "guerrillamail/error.hpp"
#include "support/test_paths.hpp"

TEST_CASE("error stores error code", "[smoke]") {
    const guerrillamail::Error error(guerrillamail::ErrorCode::internal, "boom");

    REQUIRE(error.code() == guerrillamail::ErrorCode::internal);
}

TEST_CASE("client options expose bootstrap transport knobs", "[smoke]") {
    const guerrillamail::ClientOptions options;

    REQUIRE(options.base_url.empty());
    REQUIRE(options.ajax_url.empty());
    REQUIRE(options.timeout == std::chrono::milliseconds(30000));
    REQUIRE(options.verify_tls);
    REQUIRE_FALSE(options.proxy.has_value());
}

TEST_CASE("client is movable but not copyable", "[smoke]") {
    REQUIRE_FALSE(std::is_copy_constructible_v<guerrillamail::Client>);
    REQUIRE_FALSE(std::is_copy_assignable_v<guerrillamail::Client>);
    REQUIRE(std::is_move_constructible_v<guerrillamail::Client>);
    REQUIRE(std::is_move_assignable_v<guerrillamail::Client>);
}

TEST_CASE("test support resolves repository paths", "[smoke]") {
    const auto root = guerrillamail::tests::support::project_root();

    REQUIRE(std::filesystem::exists(root / "AGENTS.md"));
    REQUIRE(root == guerrillamail::tests::support::project_root());
}
