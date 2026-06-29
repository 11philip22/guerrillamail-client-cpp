#ifndef GUERRILLAMAIL_CLIENT_HPP
#define GUERRILLAMAIL_CLIENT_HPP

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "guerrillamail/error.hpp"
#include "guerrillamail/types.hpp"

namespace guerrillamail {

struct ClientOptions {
    std::string base_url;
    std::string ajax_url;
    std::optional<std::string> site;
    std::chrono::milliseconds timeout{30000};
    std::optional<std::string> proxy;
    bool verify_tls = true;
};

class Client {
public:
    static Client create(const ClientOptions& options = {});

    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;

    std::string create_email(std::string_view alias = {}) const;
    std::vector<Message> get_messages(std::string_view email) const;
    EmailDetails fetch_email(std::string_view email, std::string_view mail_id) const;
    void delete_email(std::string_view email) const;
    std::vector<std::uint8_t> fetch_attachment(
        std::string_view email,
        std::string_view mail_id,
        const Attachment& attachment
    ) const;

private:
    struct Impl;

    explicit Client(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

} // namespace guerrillamail

#endif // GUERRILLAMAIL_CLIENT_HPP
