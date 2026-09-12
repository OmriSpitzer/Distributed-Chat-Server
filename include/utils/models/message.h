/**
 * Message header file class
 *
 * @date 11-09-2026
 */

#pragma once
#include "utils/models/user.h"
#include <ctime>
#include <ostream>
#include <string>
#include <string_view>

class Message {
public:
  // constructor
  Message(const User &from, const User &to, std::string_view message);

  // reconstruct a message loaded from the database (keeps stored id + timestamp)
  Message(const User &from, const User &to, std::string_view message, std::string id,
          std::time_t timestamp);

  // getters
  const User &getFrom() const;
  const User &getTo() const;
  const std::string &getContent() const;
  std::time_t getTimestamp() const;
  const std::string &getId() const;

  // print operator
  friend std::ostream &operator<<(std::ostream &out, const Message &msg);

  // equality operator
  bool operator==(const Message &other) const;
  bool operator!=(const Message &other) const;

  // comparison operators
  bool operator<(const Message &other) const;
  bool operator>(const Message &other) const;
  bool operator<=(const Message &other) const;
  bool operator>=(const Message &other) const;

private:
  User from;             // sender
  User to;               // receiver
  std::string content;   // message content
  std::time_t timestamp; // message timestamp
  std::string id;        // message id
};
