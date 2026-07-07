<div align="center">

<h1>guerrillamail-cpp</h1>

<p><strong>A small C++20 client for GuerrillaMail temporary inboxes.</strong></p>

<p>
  <a href="https://en.cppreference.com/w/cpp/20"><img src="https://img.shields.io/badge/C%2B%2B-20-F59E0B?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++20"></a>
  <a href="https://cmake.org/"><img src="https://img.shields.io/badge/CMake-3.22%2B-3B82F6?style=for-the-badge&logo=cmake&logoColor=white" alt="CMake 3.22+"></a>
  <a href="https://vcpkg.io/"><img src="https://img.shields.io/badge/vcpkg-managed-22C55E?style=for-the-badge" alt="vcpkg managed"></a>
  <a href="https://curl.se/libcurl/"><img src="https://img.shields.io/badge/libcurl-transport-0EA5E9?style=for-the-badge&logo=curl&logoColor=white" alt="libcurl transport"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-8B5CF6?style=for-the-badge" alt="MIT License"></a>
</p>

<p>
  <a href="#status">Status</a> |
  <a href="#features">Features</a> |
  <a href="#getting-started">Getting Started</a> |
  <a href="#usage">Usage</a> |
  <a href="#layout">Layout</a>
</p>

</div>

---

<p>Bootstrap a GuerrillaMail session, create disposable addresses, read messages, fetch message details, forget addresses, and download attachments through a synchronous C++ API.</p>

## Status

`guerrillamail-cpp` is an active C++ port of the Rust reference client in `guerillamail-rs`. The current implementation covers the core mailbox flow:

- bootstrap a session and parse the GuerrillaMail API token
- create a temporary email address
- list inbox messages
- fetch full message details
- list attachment metadata from fetched messages
- download attachment bytes
- forget an address in the current session

The public API is intentionally small and synchronous. Transport, JSON parsing, and error details stay behind project-owned headers.

## Features

- RAII client centered on `guerrillamail::Client`
- C++20 value types for messages, email details, and attachments
- `ClientOptions` for base URLs, AJAX URL, site, timeout, proxy, and TLS verification
- typed `guerrillamail::Error` exceptions with distinct error categories
- `libcurl` transport with session cookies
- `nlohmann/json` response parsing

## Getting Started

Clone with submodules so the pinned `vcpkg` checkout is available:

```powershell
git clone --recurse-submodules <repo-url>
cd guerrillamail-cpp
```

Configure and build with the bundled `vcpkg` toolchain:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="third_party/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Debug
```

Examples are enabled by default when this repository is the top-level CMake project. Disable them when embedding the library:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="third_party/vcpkg/scripts/buildsystems/vcpkg.cmake" -DGUERRILLAMAIL_CPP_BUILD_EXAMPLES=OFF
```

## Usage

```cpp
#include "guerrillamail/client.hpp"

int main() {
    auto client = guerrillamail::Client::create();

    const auto email = client.create_email();
    const auto messages = client.get_messages(email);

    if (!messages.empty()) {
        const auto details = client.fetch_email(email, messages.front().mail_id);

        for (const auto& attachment : details.attachments) {
            const auto bytes = client.fetch_attachment(email, details.mail_id, attachment);
            (void)bytes;
        }
    }

    client.delete_email(email);
}
```

When using this project from CMake, link the exported alias:

```cmake
target_link_libraries(your-target PRIVATE guerrillamail::guerrillamail-cpp)
```

## Example

The runnable demo lives in `examples/basic_flow.cpp`. It creates a temporary address, prints it, polls for messages for up to two minutes, fetches full message bodies, downloads attachments when present, and forgets the address before exiting.

```powershell
.\build\examples\Debug\guerrillamail-cpp-example-basic.exe
```

## Error Model

Public operations throw `guerrillamail::Error`, derived from `std::runtime_error`.

The error code keeps these cases distinguishable:

- `invalid_argument`
- `transport`
- `http_status`
- `token_parse`
- `response_parse`
- `json_parse`
- `internal`

## Layout

```text
include/guerrillamail/   public headers
src/                     client, protocol, parsing, and curl transport
examples/                basic end-to-end demo
third_party/vcpkg/       pinned dependency manager submodule
```
