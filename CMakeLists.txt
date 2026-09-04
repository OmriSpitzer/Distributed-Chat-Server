cmake_minimum_required(VERSION 3.16)
project(DistributedChatServer CXX)

set(CMAKE_CXX_STANDARD 17)

# Library
add_library(utils
    src/utils/models/logger.cpp
    src/utils/models/log_message.cpp
    src/utils/models/message.cpp
    src/utils/models/user.cpp
    src/utils/models/room.cpp
)
target_include_directories(utils PUBLIC include)

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
    src/auth/authentication.cpp
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

# Catch2 (URL archive avoids flaky git clones / partial checkouts on Windows)
include(FetchContent)
FetchContent_Declare(Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.4.tar.gz
    URL_HASH SHA256=b7754b711242c167d8f60b890695347f90a1ebc95949a045385114165d606dbb
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(Catch2)

# Tests
add_executable(logger_test tests/class/logger_test.cpp)
target_link_libraries(logger_test PRIVATE utils Catch2::Catch2WithMain)

add_executable(log_message_test tests/class/log_message_test.cpp)
target_link_libraries(log_message_test PRIVATE utils Catch2::Catch2WithMain)

add_executable(user_test tests/class/user_test.cpp)
target_link_libraries(user_test PRIVATE utils Catch2::Catch2WithMain)

add_executable(message_test tests/class/message_test.cpp)
target_link_libraries(message_test PRIVATE utils Catch2::Catch2WithMain)

add_executable(room_test tests/class/room_test.cpp)
target_link_libraries(room_test PRIVATE utils Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(logger_test)
catch_discover_tests(log_message_test)
catch_discover_tests(user_test)
catch_discover_tests(message_test)
catch_discover_tests(room_test)