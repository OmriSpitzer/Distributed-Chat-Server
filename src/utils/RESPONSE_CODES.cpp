/**
 * ResponseCodes class
 *
 * @brief Response codes for the server.
 * @date 07-09-2026
 */

#include "utils/RESPONSE_CODES.h"
#include <map>
#include <string>

class ResponseCodes {
public:
  // map of the response codes by the code
  const map<int, std::string> RESPONSE_CODES = {
      {200, "SUCCESS"},
      {400, "ERROR"},
      {404, "NOT_FOUND"},
      {500, "INTERNAL_SERVER_ERROR"},
  };

  // map of the response codes by the name
  const map<std::string, int> RESPONSE_CODES_BY_NAME = {
      {"SUCCESS", 200},
      {"ERROR", 400},
      {"NOT_FOUND", 404},
      {"INTERNAL_SERVER_ERROR", 500},
  };

  // get the code by the name
  int get(const std::string &name) { return ResponseCodes::RESPONSE_CODES_BY_NAME[name]; }

  // get the name by the code
  std::string gets(int code) { return ResponseCodes::RESPONSE_CODES[code]; }
};