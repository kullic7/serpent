#ifndef SERPENT_TYPES_H
#define SERPENT_TYPES_H

#include <pthread.h>
#include "common/config.h"
#include "common/game_types.h"

typedef void (*ButtonAction)(void *user_data);

typedef size_t Key;

typedef struct {
    Key events[16];
    size_t count;
    pthread_mutex_t lock;
} ClientInputQueue;

typedef struct {
    Box box; // TODO probably simplification needed

    const char text[MENU_MAX_TEXT_LENGTH];

    ButtonAction on_press;
    void *user_data; // TODO pointer to client context usually

} Button;

typedef struct {
    Size size; // TODO not used currently

    size_t button_count;
    Button *buttons;

    size_t txt_fields_count;
    TextField *txt_fields;

    size_t selected_index;

} Menu;

// models a stack (hierarchy) of menus for navigation ... rendered is always the top menu
typedef struct {
    Menu *stack[MENU_STACK_MAX];
    size_t depth;
} MenuStack;

typedef struct {
    ClientInputQueue *queue;
    const _Atomic bool *running;
} InputThreadArgs;

typedef struct {
    int cols;
    int rows;
} TermSize;

typedef struct {
    int width;
    int height;

    size_t score;
    int time_remaining; // in seconds, -1 means no limit

    size_t snake_count;
    RenderSnake *snakes;

    size_t fruit_count;
    Position *fruits;

    size_t obstacle_count;
    Obstacle *obstacles;
} GameRenderState;

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
    GAME_NAME,
    GAME_DESCRIPTION,
} TextType;

// single source of truth for client state
typedef struct {
    int socket_fd;  // socket file descriptor (unix domain sockets allow full duplex byte stream)
    char server_path[512]; // unix domain socket path
    volatile bool running;
    ClientMode mode;

    int score;
    int time_remaining; // in seconds, -1 means no limit

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
    Menu connect_menu;
    Menu pause_menu;
    Menu game_over_menu;

    GameRenderState game;

} ClientContext;

#endif //SERPENT_TYPES_H