/**
 * Gossip event payload encode/decode
 *
 * @brief Length-prefixed fields so content may contain any bytes
 * @date 13-09-2026
 *
 * Wire format: [4-byte big-endian payload size][serialized Fields bytes]
 * Fields: [4-byte big-endian length][string bytes]
 */

#include "utils/gossip_payload.h"
#include <cstdint>

namespace gossip_payload {
namespace {

// append a uint32_t to a string in big endian
void appendU32BE(std::string &out, std::uint32_t value) {
  out.push_back(static_cast<char>((value >> 24) & 0xFF));
  out.push_back(static_cast<char>((value >> 16) & 0xFF));
  out.push_back(static_cast<char>((value >> 8) & 0xFF));
  out.push_back(static_cast<char>(value & 0xFF));
}

// append a field to a string
void appendField(std::string &out, std::string_view value) {
  appendU32BE(out, static_cast<std::uint32_t>(value.size()));
  out.append(value.data(), value.size());
}

// check if there are at least the needed bytes remaining in the string
bool remainingAtLeast(std::string_view in, std::size_t offset, std::size_t needed) {
  return offset <= in.size() && in.size() - offset >= needed;
}

// read a uint32_t from a string in big endian
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

// read a field from a string
bool readField(std::string_view in, std::size_t &offset, std::string &value) {
  std::uint32_t length = 0;
  if (!readU32BE(in, offset, length)) {
    return false;
  }
  if (!remainingAtLeast(in, offset, length)) {
    return false;
  }

  // assign the value of the string to the length prefixed string
  value.assign(in.data() + offset, length);
  offset += length;
  return true;
}

} // namespace

// encode five fields into a gossip event message body
std::string encode(std::string_view type, std::string_view eventId, std::string_view username,
                   std::string_view content, std::string_view field5) {
  // create the output string
  std::string out;
  out.reserve(5 * 4 + type.size() + eventId.size() + username.size() + content.size() +
              field5.size());

  // append the fields to the output string
  appendField(out, type);
  appendField(out, eventId);
  appendField(out, username);
  appendField(out, content);
  appendField(out, field5);
  return out;
}

// decode a gossip event message body
std::optional<Fields> decode(std::string_view payload) {
  Fields fields;

  // read the fields from the payload
  std::size_t offset = 0;
  if (!readField(payload, offset, fields.type) || !readField(payload, offset, fields.eventId) ||
      !readField(payload, offset, fields.username) || !readField(payload, offset, fields.content) ||
      !readField(payload, offset, fields.field5)) {
    return std::nullopt;
  }

  // check if there is trailing junk
  if (offset != payload.size()) {
    return std::nullopt;
  }
  return fields;
}

// extract eventId from the payload
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
