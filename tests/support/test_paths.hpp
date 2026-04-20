#pragma once

#include <filesystem>

namespace guerrillamail::tests::support {

std::filesystem::path project_root();
std::filesystem::path support_root();
std::filesystem::path fixtures_root();

} // namespace guerrillamail::tests::support
