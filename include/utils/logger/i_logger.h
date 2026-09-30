/**
 * Logger interface header file
 *
 * @date 24-09-2026
 */
#pragma once
#include "utils/logger/log_message.h"

// logger interface — sinks receive the façade's LogMessage (shared timestamp/id)
class ILogger {
public:
  // destructor
  virtual ~ILogger() = default;

  // handle one log message
  virtual void log(const LogMessage &message) = 0;
};
