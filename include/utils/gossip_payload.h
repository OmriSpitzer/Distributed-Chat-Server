/**
 * Gossip event payload encode/decode
 *
 * @brief Length-prefixed fields so content may contain any bytes (including '|').
 * Wire: five fields each as [u32 BE length][bytes]:
 *   type | eventId | username | content | field5
 * @date 13-09-2026
 */
#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace gossip_payload {

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

// decode a gossip event message body; nullopt if truncated or trailing junk
std::optional<Fields> decode(std::string_view payload);

// extract eventId without requiring a full successful semantic apply
std::string eventId(std::string_view payload);

} // namespace gossip_payload
