cmake_minimum_required(VERSION 3.16)
project(DistributedChatServer CXX)

set(CMAKE_CXX_STANDARD 17)

# Library
add_library(utils
    src/utils/models/logger.cpp
    src/utils/models/message.cpp
    src/utils/models/user.cpp
)
target_include_directories(utils PUBLIC include)

# Catch2
include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.4
)
FetchContent_MakeAvailable(Catch2)

# Tests
add_executable(logger_test tests/class/logger_test.cpp)
target_link_libraries(logger_test PRIVATE utils Catch2::Catch2WithMain)

add_executable(message_test tests/class/message_test.cpp)
target_link_libraries(message_test PRIVATE utils Catch2::Catch2WithMain)

add_executable(user_test tests/class/user_test.cpp)
target_link_libraries(user_test PRIVATE utils Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(logger_test)
catch_discover_tests(message_test)
catch_discover_tests(user_test)