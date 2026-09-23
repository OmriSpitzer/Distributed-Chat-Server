/**
 * Serializer class
 *
 * @brief Serializes and deserializes packets
 * @date 11-09-2026
 *
 * Wire format: [4-byte big-endian payload size][serialized Packet bytes]
 *
 */

#include "utils/serializer.h"
#include "utils/models/packet.h"
#include <cstdint>
#include <optional>
#include <string>

namespace {
// append a uint8_t to a string
void appendU8(std::string &out, std::uint8_t value) { out.push_back(static_cast<char>(value)); }

// append a uint32_t to a string in big endian
void appendU32BE(std::string &out, std::uint32_t value) {
  out.push_back(static_cast<char>((value >> 24) & 0xFF));
  out.push_back(static_cast<char>((value >> 16) & 0xFF));
  out.push_back(static_cast<char>((value >> 8) & 0xFF));
  out.push_back(static_cast<char>(value & 0xFF));
}

// append a uint64_t to a string in big endian
void appendU64BE(std::string &out, std::uint64_t value) {
  appendU32BE(out, static_cast<std::uint32_t>(value >> 32));
  appendU32BE(out, static_cast<std::uint32_t>(value & 0xFFFFFFFFu));
}

// append a length prefixed string to a string
void appendLenPrefixed(std::string &out, const std::string &value) {
  appendU32BE(out, static_cast<std::uint32_t>(value.size()));
  out.append(value);
}

// check if there are at least needed bytes remaining in the string
bool remainingAtLeast(const std::string &in, std::size_t offset, std::size_t needed) {
  return offset <= in.size() && in.size() - offset >= needed;
}

// read a uint8_t from a string
bool readU8(const std::string &in, std::size_t &offset, std::uint8_t &value) {
  // check if there are at least 1 byte remaining in the string
  if (!remainingAtLeast(in, offset, 1)) {
    return false;
  }

  // read the uint8_t from the string
  value = static_cast<std::uint8_t>(static_cast<unsigned char>(in[offset]));
  offset += 1;
  return true;
}

// read a uint32_t from a string in big endian
bool readU32BE(const std::string &in, std::size_t &offset, std::uint32_t &value) {
  // check if there are at least 4 bytes remaining in the string
  if (!remainingAtLeast(in, offset, 4)) {
    return false;
  }

  // read the uint32_t from the string
  const auto *bytes = reinterpret_cast<const unsigned char *>(in.data() + offset);
  value = (static_cast<std::uint32_t>(bytes[0]) << 24) |
          (static_cast<std::uint32_t>(bytes[1]) << 16) |
          (static_cast<std::uint32_t>(bytes[2]) << 8) | static_cast<std::uint32_t>(bytes[3]);
  offset += 4;
  return true;
}

// read a uint64_t from a string in big endian
bool readU64BE(const std::string &in, std::size_t &offset, std::uint64_t &value) {
  std::uint32_t high = 0;
  std::uint32_t low = 0;
  if (!readU32BE(in, offset, high) || !readU32BE(in, offset, low)) {
    return false;
  }
  value = (static_cast<std::uint64_t>(high) << 32) | low;
  return true;
}

// read a length prefixed string from a string
bool readLenPrefixed(const std::string &in, std::size_t &offset, std::string &value) {
  std::uint32_t length = 0;

  // read the uint32_t from the string
  if (!readU32BE(in, offset, length)) {
    return false;
  }

  // check if there are at least the length bytes remaining in the string
  if (!remainingAtLeast(in, offset, length)) {
    return false;
  }

  // change the value of the string to the length prefixed string
  value.assign(in, offset, length);
  offset += length;
  return true;
}
} // namespace

// serialize a packet
std::string Serializer::serialize(const Packet &packet) {
  // check if the packet type is valid
  if (!Packet::isValidPacketType(packet.type)) {
    return "";
  }

  // create the payload
  std::string payload;

  // serialize the packet type, timestamp, and response code
  appendU8(payload, static_cast<std::uint8_t>(packet.type));
  appendU64BE(payload, packet.timestamp);
  appendU32BE(payload, static_cast<std::uint32_t>(packet.responseCode));

  // serialize the sender, receiver, room, and message
  appendLenPrefixed(payload, packet.sender);
  appendLenPrefixed(payload, packet.receiver);
  appendLenPrefixed(payload, packet.room);
  appendLenPrefixed(payload, packet.message);

  // check if the payload size is too large
  if (payload.size() > MAX_PAYLOAD_BYTES) {
    return "";
  }

  // create the framed packet by appending the payload size and the payload
  std::string framed;
  appendU32BE(framed, static_cast<std::uint32_t>(payload.size()));
  framed.append(payload);
  return framed;
}

// deserialize a packet
std::optional<Packet> Serializer::deserialize(const std::string &serializedPacket) {
  // check if the serialized packet is too small
  if (serializedPacket.size() < 4) {
    return std::nullopt;
  }

  // read the payload size
  std::size_t offset = 0;
  std::uint32_t payloadSize = 0;

  // read the payload size
  if (!readU32BE(serializedPacket, offset, payloadSize)) {
    return std::nullopt;
  }

  // check if the payload size is invalid
  if (payloadSize == 0 || payloadSize > MAX_PAYLOAD_BYTES) {
    return std::nullopt;
  }

  // check if the serialized packet is too large
  if (serializedPacket.size() != 4 + payloadSize) {
    return std::nullopt;
  }

  // read the packet type from the serialized packet
  std::uint8_t typeByte = 0;
  if (!readU8(serializedPacket, offset, typeByte)) {
    return std::nullopt;
  }

  // convert the packet type to the enum type
  const auto type = static_cast<Packet::PacketType>(typeByte);
  if (!Packet::isValidPacketType(type)) {
    return std::nullopt;
  }

  // create the packet
  Packet packet;

  // read the timestamp from the serialized packet
  packet.type = type;
  std::uint32_t responseCode = 0;

  if (!readU64BE(serializedPacket, offset, packet.timestamp) ||
      !readU32BE(serializedPacket, offset, responseCode)) {
    return std::nullopt;
  }

  // set the response code
  packet.responseCode = static_cast<int>(responseCode);

  // read the sender, receiver, room, and message from the serialized packet
  if (!readLenPrefixed(serializedPacket, offset, packet.sender) ||
      !readLenPrefixed(serializedPacket, offset, packet.receiver) ||
      !readLenPrefixed(serializedPacket, offset, packet.room) ||
      !readLenPrefixed(serializedPacket, offset, packet.message) ||
      offset != serializedPacket.size()) {
    return std::nullopt;
  }

  return packet;
}
