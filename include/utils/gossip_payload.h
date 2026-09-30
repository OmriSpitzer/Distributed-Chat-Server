/**
 * Gossip event payload encode/decode
 *
 * @brief Length-prefixed fields so content may contain any bytes
 * @date 13-09-2026
 */
#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace gossip_payload {

// fields for the gossip event payload
struct Fields {
  std::string type;
  std::string eventId;
  std::string username;
  std::string content;
  std::string field5; // timestamp, email, prevRoom, etc.
};

// encode five fields into a gossip event message body
std::string encode(std::string_view type, std::string_view eventId, std::string_view username,
                   std::string_view content, std::string_view field5);

// decode a gossip event message body
std::optional<Fields> decode(std::string_view payload);

// extract eventId from the payload
std::string eventId(std::string_view payload);

} // namespace gossip_payload
