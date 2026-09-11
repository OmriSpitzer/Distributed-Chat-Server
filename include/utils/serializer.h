/**
 * Serializer header file class
 *
 * @date 11-09-2026
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

  // maximum payload bytes
  static constexpr std::uint32_t MAX_PAYLOAD_BYTES = 1024 * 1024;
};