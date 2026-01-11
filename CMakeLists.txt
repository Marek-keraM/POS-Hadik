cmake_minimum_required(VERSION 3.29)
project(SemestralkaHadik C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

# Shared code (spoločné pre server aj klienta)
add_library(shared_code
        shared_code/ipc_messages.c
        shared_code/game_state.c
)

target_include_directories(shared_code PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/shared_code
)

# Server
add_executable(server
        server/server_main.c
)
target_link_libraries(server PRIVATE shared_code)
target_include_directories(server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server
)

# Client
add_executable(client
        client/client_main.c
)
target_link_libraries(client PRIVATE shared_code)
target_include_directories(client PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/client
)