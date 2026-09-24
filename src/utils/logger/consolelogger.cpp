/**
 * Console Logger class implementation file
 *
 * @brief Console logger that prints the log message to the console
 * @date 24-09-2026
 */
#include "utils/logger/consoleLogger.h"
#include "utils/logger/log_message.h"
#include <iostream>

// print the log message to the console
void ConsoleLogger::log(const LogMessage &message) {
  if (message.getType() == LogMessage::Type::ERROR) {
    std::cerr << message << '\n';
  } else {
    std::cout << message << '\n';
  }
}
