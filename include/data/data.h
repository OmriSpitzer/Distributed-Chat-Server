/**
 * Data header file class
 *
 * @date 07-09-2026
 */
#pragma once
#include <map>
#include <string>
#include <vector>

class Data {
public:
  static std::vector<std::map<std::string, std::string>> users;    // stub users
  static std::vector<std::map<std::string, std::string>> rooms;    // stub rooms
  static std::vector<std::map<std::string, std::string>> messages; // stub messages
};
