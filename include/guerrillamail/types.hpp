#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace guerrillamail {

struct Message {
    std::string mail_id;
    std::string mail_from;
    std::string mail_subject;
};

struct Attachment {
    std::string name;
    std::string url;
    std::size_t size = 0;
};

struct EmailDetails {
    std::string mail_id;
    std::string mail_from;
    std::string mail_subject;
    std::string mail_body;
    std::vector<Attachment> attachments;
};

} // namespace guerrillamail
