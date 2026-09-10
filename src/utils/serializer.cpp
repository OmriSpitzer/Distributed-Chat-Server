/**
 * Length-prefixed TCP packet framing.
 *
 * Wire format: [4-byte big-endian payload size][serialized Packet bytes]
 * Payload: [u8 type][u64 BE timestamp][u32 BE responseCode]
 * then sender, receiver, room, message each as [u32 BE byte length][bytes].
 *
 * @date 07-09-2026
 */

#include "utils/serializer.h"
#include "utils/models/packet.h"
#include <cstdint>
#include <optional>
#include <string>

namespace {
void appendU8(std::string &out, std::uint8_t value) { out.push_back(static_cast<char>(value)); }

void appendU32BE(std::string &out, std::uint32_t value) {
  out.push_back(static_cast<char>((value >> 24) & 0xFF));
  out.push_back(static_cast<char>((value >> 16) & 0xFF));
  out.push_back(static_cast<char>((value >> 8) & 0xFF));
  out.push_back(static_cast<char>(value & 0xFF));
}

void appendU64BE(std::string &out, std::uint64_t value) {
  appendU32BE(out, static_cast<std::uint32_t>(value >> 32));
  appendU32BE(out, static_cast<std::uint32_t>(value & 0xFFFFFFFFu));
}

void appendLenPrefixed(std::string &out, const std::string &value) {
  appendU32BE(out, static_cast<std::uint32_t>(value.size()));
  out.append(value);
}

bool remainingAtLeast(const std::string &in, std::size_t offset, std::size_t needed) {
  return offset <= in.size() && in.size() - offset >= needed;
}

bool readU8(const std::string &in, std::size_t &offset, std::uint8_t &value) {
  if (!remainingAtLeast(in, offset, 1)) {
    return false;
  }
  value = static_cast<std::uint8_t>(static_cast<unsigned char>(in[offset]));
  offset += 1;
  return true;
}

bool readU32BE(const std::string &in, std::size_t &offset, std::uint32_t &value) {
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

bool readU64BE(const std::string &in, std::size_t &offset, std::uint64_t &value) {
  std::uint32_t high = 0;
  std::uint32_t low = 0;
  if (!readU32BE(in, offset, high) || !readU32BE(in, offset, low)) {
    return false;
  }
  value = (static_cast<std::uint64_t>(high) << 32) | low;
  return true;
}

bool readLenPrefixed(const std::string &in, std::size_t &offset, std::string &value) {
  std::uint32_t length = 0;
  if (!readU32BE(in, offset, length)) {
    return false;
  }
  if (!remainingAtLeast(in, offset, length)) {
    return false;
  }
  value.assign(in, offset, length);
  offset += length;
  return true;
}

bool isValidPacketType(Packet::PacketType type) {
  switch (type) {
  case Packet::PacketType::LOGIN:
  case Packet::PacketType::LOGOUT:
  case Packet::PacketType::MESSAGE:
  case Packet::PacketType::ROOM_JOIN:
  case Packet::PacketType::ROOM_LEAVE:
  case Packet::PacketType::DEFAULT:
  case Packet::PacketType::HEARTBEAT:
  case Packet::PacketType::REGISTER:
  case Packet::PacketType::GOSSIP_HELLO:
  case Packet::PacketType::GOSSIP_EVENT:
  case Packet::PacketType::GOSSIP_DIGEST:
  case Packet::PacketType::GOSSIP_PULL:
    return true;
  }
  return false;
}
} // namespace

// serialize a packet
std::string Serializer::serialize(const Packet &packet) {
  if (!isValidPacketType(packet.type)) {
    return "";
  }

  std::string payload;
  appendU8(payload, static_cast<std::uint8_t>(packet.type));
  appendU64BE(payload, packet.timestamp);
  appendU32BE(payload, static_cast<std::uint32_t>(packet.responseCode));
  appendLenPrefixed(payload, packet.sender);
  appendLenPrefixed(payload, packet.receiver);
  appendLenPrefixed(payload, packet.room);
  appendLenPrefixed(payload, packet.message);

  if (payload.size() > MAX_PAYLOAD_BYTES) {
    return "";
  }

  std::string framed;
  appendU32BE(framed, static_cast<std::uint32_t>(payload.size()));
  framed.append(payload);
  return framed;
}

// deserialize a packet
std::optional<Packet> Serializer::deserialize(const std::string &serializedPacket) {
  if (serializedPacket.size() < 4) {
    return std::nullopt;
  }

  std::size_t offset = 0;
  std::uint32_t payloadSize = 0;
  if (!readU32BE(serializedPacket, offset, payloadSize)) {
    return std::nullopt;
  }
  if (payloadSize == 0 || payloadSize > MAX_PAYLOAD_BYTES) {
    return std::nullopt;
  }
  if (serializedPacket.size() != 4 + payloadSize) {
    return std::nullopt;
  }

  std::uint8_t typeByte = 0;
  if (!readU8(serializedPacket, offset, typeByte)) {
    return std::nullopt;
  }
  const auto type = static_cast<Packet::PacketType>(typeByte);
  if (!isValidPacketType(type)) {
    return std::nullopt;
  }

  Packet packet;
  packet.type = type;
  if (!readU64BE(serializedPacket, offset, packet.timestamp)) {
    return std::nullopt;
  }
  std::uint32_t responseCode = 0;
  if (!readU32BE(serializedPacket, offset, responseCode)) {
    return std::nullopt;
  }
  packet.responseCode = static_cast<int>(responseCode);
  if (!readLenPrefixed(serializedPacket, offset, packet.sender)) {
    return std::nullopt;
  }
  if (!readLenPrefixed(serializedPacket, offset, packet.receiver)) {
    return std::nullopt;
  }
  if (!readLenPrefixed(serializedPacket, offset, packet.room)) {
    return std::nullopt;
  }
  if (!readLenPrefixed(serializedPacket, offset, packet.message)) {
    return std::nullopt;
  }
  if (offset != serializedPacket.size()) {
    return std::nullopt;
  }

  return packet;
}
