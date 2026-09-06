/**
 * Configuration file for the server
 *
 * @date 06-09-2026
 */
#pragma once
#include <cstdint>
#include <string>

namespace config {

// server host
inline const std::string SERVER_HOST = "127.0.0.1";

// TCP port the server listens on
inline constexpr std::uint16_t PORT = 5555;

// number of worker threads in the thread pool
inline constexpr std::size_t THREAD_COUNT = 4;

// path to the text database file
inline const std::string DB_PATH = "data/db.txt";

} // namespace config