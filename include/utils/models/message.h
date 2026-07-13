/**
 * Message header file class
 *
 * @date 13-07-2026
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
  Message(User &from, User &to, std::string_view message);

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
  User &from;            // sender
  User &to;              // receiver
  std::string content;   // message content
  std::time_t timestamp; // message timestamp
  std::string id;        // message id
};
