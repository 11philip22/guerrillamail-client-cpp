#include "support/test_paths.hpp"

namespace guerrillamail::tests::support {

std::filesystem::path project_root() {
    return std::filesystem::path(GUERRILLAMAIL_CPP_TEST_SOURCE_DIR).parent_path();
}

std::filesystem::path support_root() {
    return project_root() / "tests" / "support";
}

std::filesystem::path fixtures_root() {
    return support_root() / "fixtures";
}

} // namespace guerrillamail::tests::support
