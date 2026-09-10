cmake_minimum_required(VERSION 3.16)
project(DistributedChatServer C CXX)

set(CMAKE_CXX_STANDARD 17)

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# - - - - - - - - - - - - - - - - - Libraries - - - - - - - - - - - - - - - - -
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

# Library
add_library(utils
    src/utils/models/logger.cpp
    src/utils/models/log_message.cpp
    src/utils/models/message.cpp
    src/utils/models/user.cpp
    src/utils/models/room.cpp
    src/utils/models/packet.cpp
    src/utils/serializer.cpp
)
target_include_directories(utils PUBLIC include)
target_link_libraries(utils PUBLIC ws2_32)

find_package(Threads REQUIRED)

# Server library
add_library(server_lib
    src/server/database_manager.cpp
    src/server/user_manager.cpp
    src/server/room_manager.cpp
    src/server/message_manager.cpp
    src/server/thread_pool.cpp
    src/server/packet_processor.cpp
    src/server/client_session.cpp
    src/server/connection_manager.cpp
    src/server/heartbeat.cpp
    src/server/server.cpp
    src/server/gossip_manager.cpp
)

target_include_directories(server_lib PUBLIC include)
target_link_libraries(server_lib PUBLIC utils ws2_32)

add_executable(chat_server src/server/main.cpp)
target_link_libraries(chat_server PRIVATE server_lib)

# Client library
add_library(client_lib
    src/client/network.cpp
    src/client/console_ui.cpp
    src/client/packet_builder.cpp
    src/client/packet_handler.cpp
    src/client/client.cpp
)
target_include_directories(client_lib PUBLIC include)
target_link_libraries(client_lib PUBLIC utils)

add_executable(chat_client src/client/main.cpp)
target_link_libraries(chat_client PRIVATE client_lib)

# Authentication library
add_library(auth
    src/auth/authentication.cpp
)
target_link_libraries(auth PUBLIC utils)

target_include_directories(auth PUBLIC include)
target_link_libraries(server_lib PUBLIC utils auth ws2_32 Threads::Threads)
target_link_libraries(client_lib PUBLIC utils auth ws2_32 Threads::Threads)

# Database Library
add_library(sqlite3 STATIC src/database/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC src/database/sqlite)


# SQL schema / query registry: "c_name:relative/path.sql"
set(DB_SCHEMAS
  "init:src/database/init.sql"
  "get_user:src/database/queries/get_user.sql"
  "login_user:src/database/queries/login_user.sql"
  "create_user:src/database/queries/create_user.sql"
  "user_exists:src/database/queries/user_exists.sql"
  "save_message:src/database/queries/save_message.sql"
  "load_history:src/database/queries/load_history.sql"
  "set_membership:src/database/queries/set_membership.sql"
  "clear_membership:src/database/queries/clear_membership.sql"
)

set(_sql_schemas_h "#pragma once\n\n// Generated - do not edit. Source: DB_SCHEMAS in CMakeLists.txt\nnamespace db::sql {\n\n")
foreach(_entry IN LISTS DB_SCHEMAS)
  string(REPLACE ":" ";" _parts "${_entry}")
  list(GET _parts 0 _name)
  list(GET _parts 1 _rel)
  set(_abs "${CMAKE_SOURCE_DIR}/${_rel}")
  if(NOT EXISTS "${_abs}")
    message(FATAL_ERROR "DB schema file not found for '${_name}': ${_abs}")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_abs}")
  file(READ "${_abs}" _sql)
  string(APPEND _sql_schemas_h
    "// ${_rel}\n"
    "inline constexpr char ${_name}[] = R\"SQL(\n${_sql}\n)SQL\";\n\n")
  message(STATUS "DB schema '${_name}' <- ${_rel}")
endforeach()
string(APPEND _sql_schemas_h "} // namespace db::sql\n")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
file(WRITE "${CMAKE_BINARY_DIR}/generated/sql_schemas.h" "${_sql_schemas_h}")
target_include_directories(server_lib PRIVATE "${CMAKE_BINARY_DIR}/generated")

target_link_libraries(server_lib PUBLIC sqlite3)

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# ^ - ^ - ^ - ^ - ^ - ^ - ^ - ^ - ^ Libraries ^ - ^ - ^ - ^ - ^ - ^ - ^ - ^ - ^
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# - - - - - - - - - - - - - - - - - - Tests - - - - - - - - - - - - - - - - - -
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

# Tests
include(CTest)
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.8.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(message_test tests/class/message_test.cpp)
target_link_libraries(message_test PRIVATE utils Catch2::Catch2WithMain)

add_executable(log_message_test tests/class/log_message_test.cpp)
target_link_libraries(log_message_test PRIVATE utils Catch2::Catch2WithMain)

add_executable(logger_test tests/class/logger_test.cpp)
target_link_libraries(logger_test PRIVATE utils Catch2::Catch2WithMain)

add_executable(user_test tests/class/user_test.cpp)
target_link_libraries(user_test PRIVATE utils Catch2::Catch2WithMain)

add_executable(packet_test tests/class/packet_test.cpp)
target_link_libraries(packet_test PRIVATE utils Catch2::Catch2WithMain)

add_executable(room_test tests/class/room_test.cpp)
target_link_libraries(room_test PRIVATE utils Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(message_test)
catch_discover_tests(log_message_test)
catch_discover_tests(logger_test)
catch_discover_tests(user_test)
catch_discover_tests(packet_test)
catch_discover_tests(room_test)

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# ^ - ^ - ^ - ^ - ^ - ^ - ^ - ^ - ^ - Tests - ^ - ^ - ^ - ^ - ^ - ^ - ^ - ^ - ^
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -