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
