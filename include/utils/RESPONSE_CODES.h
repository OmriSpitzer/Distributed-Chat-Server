/**
 * ResponseCodes class
 *
 * @brief Response codes for the server.
 * @date 07-09-2026
 */

#pragma once
#include <map>
#include <string>

class ResponseCodes {
public:
  // map of the response codes by the code
  static const map<int, std::string> RESPONSE_CODES;

  // map of the response codes by the name
  static const map<std::string, int> RESPONSE_CODES_BY_NAME;

  // get the code by the name
  static int get(const std::string &name);

  // get the name by the code
  static std::string gets(int code);
};