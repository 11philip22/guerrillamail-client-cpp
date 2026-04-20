#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace guerrillamail::tests::support {

struct MockHttpRequest {
    std::string method;
    std::string path;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;

    [[nodiscard]] std::optional<std::string> header_value(std::string_view name) const;
};

struct MockHttpResponse {
    int status_code = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    std::chrono::milliseconds delay{0};
};

class MockHttpServer {
public:
    using Handler = std::function<MockHttpResponse(const MockHttpRequest&)>;

    explicit MockHttpServer(Handler handler);
    ~MockHttpServer();

    MockHttpServer(const MockHttpServer&) = delete;
    MockHttpServer& operator=(const MockHttpServer&) = delete;

    [[nodiscard]] std::string url(std::string_view path = "/", std::string_view scheme = "http") const;

private:
    struct Impl;
    Impl* impl_;
};

} // namespace guerrillamail::tests::support
