#ifndef GUERRILLAMAIL_TYPES_HPP
#define GUERRILLAMAIL_TYPES_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace guerrillamail {

struct Message {
    std::string mail_id;
    std::string mail_from;
    std::string mail_subject;
    std::string mail_excerpt;
    std::string mail_timestamp;
};

struct Attachment {
    // Intentionally mirrors upstream attachment metadata fields returned by fetch-email responses
    // instead of exposing a derived download-oriented `{name, url, size}` placeholder shape.
    std::string filename;
    std::optional<std::string> content_type_or_hint;
    std::string part_id;
};

struct EmailDetails {
    std::string mail_id;
    std::string mail_from;
    std::string mail_subject;
    std::string mail_body;
    std::string mail_timestamp;
    std::vector<Attachment> attachments;
    std::optional<std::uint32_t> attachment_count;
    std::optional<std::string> sid_token;
};

} // namespace guerrillamail

#endif // GUERRILLAMAIL_TYPES_HPP
