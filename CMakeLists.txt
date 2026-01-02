cmake_minimum_required(VERSION 3.20)
project(serpent C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(serpent-client client/client_main.c
        client/client.c
        client/input.c
        client/renderer.c
        client/menu.c
        client/context.c
        client/actions.c)

target_include_directories(serpent-client PRIVATE
        ${CMAKE_SOURCE_DIR}/client
        ${CMAKE_SOURCE_DIR}/common
)