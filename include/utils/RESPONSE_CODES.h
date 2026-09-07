/**
 * ResponseCodes class
 *
 * @brief Response codes for the server.
 * @date 07-09-2026
 */

#pragma once

enum class RESPONSE_CODES {
  SUCCESS = 200,
  ERROR = 400,
  NOT_FOUND = 404,
  INTERNAL_SERVER_ERROR = 500,
};