cmake_minimum_required(VERSION 3.20)
project(serpent C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(serpent-client client/main.c
        client/client.c
        client/renderer.c
        client/callbacks.c
        client/menu.c
        client/input.c
        common/timer.c
        common/logging.c
        common/protocol.c
        common/types.c
)

target_include_directories(serpent-client PRIVATE
        ${CMAKE_SOURCE_DIR}/client
        ${CMAKE_SOURCE_DIR}/common
)

add_executable(serpent-server server/main.c
        server/server.c
        server/game.c
        server/physics.c
        server/registry.c
        server/events.c
        common/timer.c
        common/logging.c
        common/protocol.c
        common/types.c
)

target_include_directories(serpent-server PRIVATE
        ${CMAKE_SOURCE_DIR}/server
        ${CMAKE_SOURCE_DIR}/common
)

add_custom_target(memcheck
        COMMAND valgrind
        --leak-check=full
        --track-origins=yes
        --trace-children=yes
        --log-file=valgrind.log
        $<TARGET_FILE:serpent-client>
        DEPENDS serpent-client serpent-server
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running Valgrind on serpent-client (with child processes)"
)
