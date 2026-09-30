/**
 * Console Logger class header file
 *
 * @date 24-09-2026
 */
#pragma once
#include "utils/logger/i_logger.h"
#include "utils/logger/log_message.h"

class ConsoleLogger : public ILogger {
public:
  // destructor
  ~ConsoleLogger() override = default;

  // log a message to stdout/stderr
  void log(const LogMessage &message) override;
};
