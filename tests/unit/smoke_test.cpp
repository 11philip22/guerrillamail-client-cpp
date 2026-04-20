#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include "guerrillamail/client.hpp"
#include "guerrillamail/error.hpp"
#include "support/test_paths.hpp"

TEST_CASE("error stores error code", "[smoke]") {
    const guerrillamail::Error error(guerrillamail::ErrorCode::internal, "boom");

    REQUIRE(error.code() == guerrillamail::ErrorCode::internal);
}

TEST_CASE("client create constructs placeholder client", "[smoke]") {
    const auto client = guerrillamail::Client::create();

    REQUIRE_THROWS_AS(client.create_email("demo"), guerrillamail::Error);
}

TEST_CASE("test support resolves repository paths", "[smoke]") {
    const auto root = guerrillamail::tests::support::project_root();

    REQUIRE(std::filesystem::exists(root / "AGENTS.md"));
    REQUIRE(root == guerrillamail::tests::support::project_root());
}
