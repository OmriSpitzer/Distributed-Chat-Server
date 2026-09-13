/**
 * Gossip event payload encode/decode
 *
 * @date 13-09-2026
 */

#include "utils/gossip_payload.h"
#include <cstdint>

namespace gossip_payload {
namespace {
void appendU32BE(std::string &out, std::uint32_t value) {
  out.push_back(static_cast<char>((value >> 24) & 0xFF));
  out.push_back(static_cast<char>((value >> 16) & 0xFF));
  out.push_back(static_cast<char>((value >> 8) & 0xFF));
  out.push_back(static_cast<char>(value & 0xFF));
}

void appendField(std::string &out, std::string_view value) {
  appendU32BE(out, static_cast<std::uint32_t>(value.size()));
  out.append(value.data(), value.size());
}

bool remainingAtLeast(std::string_view in, std::size_t offset, std::size_t needed) {
  return offset <= in.size() && in.size() - offset >= needed;
}

bool readU32BE(std::string_view in, std::size_t &offset, std::uint32_t &value) {
  if (!remainingAtLeast(in, offset, 4)) {
    return false;
  }
  const auto *bytes = reinterpret_cast<const unsigned char *>(in.data() + offset);
  value = (static_cast<std::uint32_t>(bytes[0]) << 24) |
          (static_cast<std::uint32_t>(bytes[1]) << 16) |
          (static_cast<std::uint32_t>(bytes[2]) << 8) | static_cast<std::uint32_t>(bytes[3]);
  offset += 4;
  return true;
}

bool readField(std::string_view in, std::size_t &offset, std::string &value) {
  std::uint32_t length = 0;
  if (!readU32BE(in, offset, length)) {
    return false;
  }
  if (!remainingAtLeast(in, offset, length)) {
    return false;
  }
  value.assign(in.data() + offset, length);
  offset += length;
  return true;
}

} // namespace

// encode five fields into a gossip event message body
std::string encode(std::string_view type, std::string_view eventId, std::string_view username,
                   std::string_view content, std::string_view field5) {
  std::string out;
  out.reserve(5 * 4 + type.size() + eventId.size() + username.size() + content.size() +
              field5.size());
  appendField(out, type);
  appendField(out, eventId);
  appendField(out, username);
  appendField(out, content);
  appendField(out, field5);
  return out;
}

// decode a gossip event message body; nullopt if truncated or trailing junk
std::optional<Fields> decode(std::string_view payload) {
  Fields fields;
  std::size_t offset = 0;
  if (!readField(payload, offset, fields.type) || !readField(payload, offset, fields.eventId) ||
      !readField(payload, offset, fields.username) || !readField(payload, offset, fields.content) ||
      !readField(payload, offset, fields.field5)) {
    return std::nullopt;
  }
  if (offset != payload.size()) {
    return std::nullopt;
  }
  return fields;
}

// extract eventId without requiring a full successful semantic apply
std::string eventId(std::string_view payload) {
  std::string type;
  std::string id;
  std::size_t offset = 0;
  if (!readField(payload, offset, type) || !readField(payload, offset, id)) {
    return {};
  }
  return id;
}

} // namespace gossip_payload
