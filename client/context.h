#ifndef SERPENT_CONTEXT_H
#define SERPENT_CONTEXT_H

#include "menu.h"
#include "game_types.h"

//------------------------


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
    GameRenderState game;
    int score;

    // game configuration options
    int time_remaining; // in seconds, -1 means no limit
    GameMode game_mode;
    char file_path[128];
    bool obstacles_enabled; // for hard world
    bool random_world_enabled; // random | from file

} ClientContext;

// --------------
/*
typedef struct {
    int socket_fd;
    char server_path[512];
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
    Menu mode_select_menu;
    Menu world_select_menu;
    Menu load_menu;
    Menu pause_menu;
    Menu game_over_menu;
    Menu awaiting_menu;
} MenuState;

typedef struct {
    int score;
    GameRenderState render;
} GameState;

typedef struct {
    volatile bool running;
    GameMode mode;
    int time_remaining; // in seconds, -1 means no limit
    char file_path[128];
} GameConfig;

// single source of truth for client state
typedef struct {
    NetworkState net;
    InputState input;
    MenuState menus;
    GameState game;
    ClientMode mode;
    GameConfig config;
} ClientContext2;
*/

// helpers
bool connect_to_server(ClientContext *ctx); // should modify just net subctx
bool wait_for_server_socket(const char *path, int timeout_ms);
void disconnect_from_server(ClientContext *ctx); // should modify just net subctx

bool spawn_connect_create_server(ClientContext *ctx); // should modify just net subctx

bool spawn_server_process(ClientContext *ctx); // should modify just net subctx

void setup_input(ClientContext *ctx, const char *note);

void on_time_entered(void *ctx_ptr, const char *text); // pushes menu
void on_socket_path_entered_when_creating(void *ctx_ptr, const char *path); // uses modifies net and mode
void on_socket_path_entered_when_joining(void *ctx_ptr, const char *path); // uses modifies net and mode
void on_input_file_entered(void *ctx_ptr, const char *file_path); // uses game sub and pushes menu

// low level event(msg) callbacks
void on_game_over(void *ctx_ptr);
void on_game_state(void *ctx_ptr, const GameRenderState *state);

#endif //SERPENT_CONTEXT_H