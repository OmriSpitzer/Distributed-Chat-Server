/**
 * Length-prefixed TCP packet framing shared by client and server.
 *
 * Wire format: [4-byte big-endian payload size][serialized Packet bytes]
 *
 * @date 06-09-2026
 */
#pragma once
#include "utils/models/packet.h"
#include <cstdint>
#include <optional>
#include <string>

class Serializer {
public:
  // serialize a packet
  static std::string serialize(const Packet &packet);

  // deserialize a packet
  static std::optional<Packet> deserialize(const std::string &serializedPacket);

private:
  static constexpr std::uint32_t MAX_PAYLOAD_BYTES = 1024 * 1024;
};