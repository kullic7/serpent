#ifndef SERPENT_CONTEXT_H
#define SERPENT_CONTEXT_H

#include "menu.h"
#include "game.h"

//------------------------
/*
typedef struct {
    int socket_fd;
    char server_path[512];
    volatile bool running;
} NetworkState;

typedef struct {
    InputMode mode;
    char note[128];
    char buffer[128];
    size_t len;
    void (*on_submit)(void *ctx, const char *text);
} InputState;

typedef struct {
    MenuStack stack;
    Menu main_menu;
    Menu new_game_menu;
    Menu multiplayer_menu;
    Menu mode_select_menu;
    Menu world_select_menu;
    Menu connect_menu;
    Menu pause_menu;
    Menu game_over_menu;
} MenuState;

typedef struct {
    int score;
    GameRenderState render;
} GameState;

// single source of truth for client state
typedef struct {
    NetworkState net;
    InputState input;
    MenuState menus;
    GameState game;
    ClientMode mode;
} ClientContext;
*/

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
    INPUT_TEXT, // text mode
    INPUT_GAME, // game mode for keys/directions
    INPUT_DISPLAY, // just arbitrary display mode
} InputMode;

typedef enum {
    GAME_SINGLE,
    GAME_MULTI
} GameMode;


// single source of truth for client state
typedef struct {
    int socket_fd;  // socket file descriptor (unix domain sockets allow full duplex byte stream)
    char server_path[512]; // unix domain socket path
    volatile bool running;
    ClientMode mode;

    InputMode input_mode;
    char text_note[128];
    char text_buffer[128];
    size_t text_len;
    void (*on_text_submit)(void *ctx, const char *text);

    MenuStack menus;

    Menu main_menu;
    Menu new_game_menu;
    Menu multiplayer_menu;
    Menu mode_select_menu;
    Menu world_select_menu;
    Menu load_menu;
    Menu pause_menu;
    Menu game_over_menu;
    Menu awaiting_menu;
    Menu join_menu;

    GameRenderState game;
    int score;
    int time_remaining; // in seconds, -1 means no limit
    GameMode game_mode;

} ClientContext;


// helpers
void connect_to_server(ClientContext *ctx); // should modify just net subctx
void disconnect_from_server(ClientContext *ctx); // should modify just net subctx

void spawn_create_join_server(ClientContext *ctx); // should modify just net subctx

void spawn_server_process_if_needed(ClientContext *ctx); // should modify just net subctx

void load_from_file(ClientContext *ctx, const char *file_path); // pushes menu
void save_to_file(ClientContext *ctx, const char *file_path); // uses game sub
void random_world(ClientContext *ctx); // uses game sub

void on_time_entered(void *ctx_ptr, const char *text); // pushes menu
void on_socket_path_entered(void *ctx_ptr, const char *path); // uses modifies net and mode
void on_input_file_entered(void *ctx_ptr, const char *file_path); // uses game sub and pushes menu

// low level event(msg) callbacks
void on_game_over(void *ctx_ptr);
void on_game_state(void *ctx_ptr, const GameRenderState *state);
void on_game_ready(void *ctx_ptr);

#endif //SERPENT_CONTEXT_H