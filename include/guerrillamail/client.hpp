#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "guerrillamail/error.hpp"
#include "guerrillamail/types.hpp"

namespace guerrillamail {

struct ClientOptions {
    std::string base_url;
    std::string ajax_url;
};

class Client {
public:
    static Client create(const ClientOptions& options = {});

    std::string create_email(std::string_view alias = {}) const;
    std::vector<Message> get_messages(std::string_view email) const;
    EmailDetails fetch_email(std::string_view email, std::string_view mail_id) const;
    void delete_email(std::string_view email) const;
    std::vector<Attachment> list_attachments(std::string_view email, std::string_view mail_id) const;
    std::vector<std::uint8_t> fetch_attachment(
        std::string_view email,
        std::string_view mail_id,
        const Attachment& attachment
    ) const;

private:
    struct Impl;

    explicit Client(std::shared_ptr<Impl> impl);

    std::shared_ptr<Impl> impl_;
};

} // namespace guerrillamail
