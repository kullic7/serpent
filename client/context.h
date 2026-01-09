#ifndef SERPENT_CONTEXT_H
#define SERPENT_CONTEXT_H

#include "menu.h"
#include "types.h"
#include "input.h"


typedef enum {
    CLIENT_MENU, // all menus except pause
    CLIENT_CONNECTING, // maybe redundant
    CLIENT_PLAYING,
    CLIENT_PAUSED, // pause menu
    CLIENT_EXIT,
    CLIENT_ERROR,
    CLIENT_GAME_OVER,
} ClientMode;

typedef enum {
    GAME_SINGLE,
    GAME_MULTI
} GameMode;


// single source of truth for client state
typedef struct {
    int socket_fd;  // socket file descriptor (unix domain sockets allow full duplex byte stream)
    char server_path[512]; // unix domain socket path
    pid_t server_pid; // if we spawned the server process

    volatile bool running;
    char error_message[512];
    ClientMode mode;

    // input state
    InputMode input_mode;
    char text_note[128];
    char text_buffer[128];
    size_t text_len;
    void (*on_text_submit)(void *ctx, const char *text);

    MenuStack menus;

    Menu main_menu;
    Menu mode_select_menu;
    Menu world_select_menu;
    Menu load_menu;
    Menu player_select_menu;
    Menu pause_menu;
    Menu game_over_menu;
    Menu awaiting_menu;
    Menu error_menu;

    // current game state/rendering
    ClientGameStateSnapshot game;

    // game configuration options
    int time_remaining; // in seconds, -1 means no limit
    GameMode game_mode;
    char file_path[128];
    bool obstacles_enabled; // for hard world
    bool random_world_enabled; // random | from file

} ClientContext;

#endif //SERPENT_CONTEXT_H