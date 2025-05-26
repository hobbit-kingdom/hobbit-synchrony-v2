cmake_minimum_required(VERSION 3.10)
project(ClientServerApp)

# Define the path to the common utilities
set(COMMON_UTILS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common/NetworkUtilities)

# Add an interface library for common utilities
# This is a modern CMake way to handle shared code/settings
add_library(NetworkUtils INTERFACE)
target_sources(NetworkUtils INTERFACE
    ${COMMON_UTILS_DIR}/Message.cpp
    ${COMMON_UTILS_DIR}/WinCrypto.cpp
)
target_include_directories(NetworkUtils INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR} # Allows including "common/NetworkUtilities/header.h"
)

# For WinCrypto, we need to link against Ws2_32 and Advapi32
if(WIN32)
    target_link_libraries(NetworkUtils INTERFACE Ws2_32 Advapi32)
endif()


# Server executable
add_executable(game_server src/server/server.cpp)
target_link_libraries(game_server PRIVATE NetworkUtils)
# Ensure server can find headers from common
target_include_directories(game_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})


# Client executable
add_executable(game_client src/client/client.cpp)
target_link_libraries(game_client PRIVATE NetworkUtils)
# Ensure client can find headers from common
target_include_directories(game_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
