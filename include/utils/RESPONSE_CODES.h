/**
 * Response Codes enum class
 *
 * @brief HTTP response codes for the server
 * @date 07-09-2026
 *
 * 200: OK
 * 400: BAD_REQUEST
 * 403: FORBIDDEN
 * 404: NOT_FOUND
 * 500: INTERNAL_SERVER_ERROR
 */

#pragma once

enum class RESPONSE_CODES {
  SUCCESS = 200,
  ERROR = 400,
  FORBIDDEN = 403,
  NOT_FOUND = 404,
  INTERNAL_SERVER_ERROR = 500,
};