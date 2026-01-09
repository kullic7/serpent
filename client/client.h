#ifndef SERPENT_CLIENT_H
#define SERPENT_CLIENT_H

#include "input.h"
#include "context.h"

// client lifecycle
void client_init(ClientContext *ctx);
void client_run(ClientContext *ctx, ClientInputQueue *iq, ServerInputQueue *sq);
void client_cleanup(ClientContext *ctx);

// infrastructure
bool connect_to_server(ClientContext *ctx);
bool wait_for_server_socket(const char *path, int timeout_ms);
void disconnect_from_server(ClientContext *ctx);
bool spawn_server_process(ClientContext *ctx);
bool spawn_connect_create_server(ClientContext *ctx);
void poll_server_exit(ClientContext *ctx);
void handle_server_disconnect(ClientContext *ctx);

// menu initializers
static void init_menu_fields(Menu *menu, Button *buttons, size_t b_count,
    TextField *txt_fields, size_t t_count, ClientContext *ctx);

void init_main_menu(ClientContext *ctx);
void init_pause_menu(ClientContext *ctx);
void init_mode_select_menu(ClientContext *ctx);
void init_world_select_menu(ClientContext *ctx);
void init_load_menu(ClientContext *ctx);
void init_player_select_menu(ClientContext *ctx);
void init_game_over_menu(ClientContext *ctx);
void init_awaiting_menu(ClientContext *ctx);
void init_error_menu(ClientContext *ctx);

// input handlers
void handle_menu_key(Menu *menu, Key key);
void handle_game_key(ClientContext *ctx, Key key);
void handle_server_msg(ClientContext *ctx, Message msg);
void handle_text_input(ClientContext *ctx, Key key);

void setup_input(ClientContext *ctx, const char *note);

// thread functions

typedef struct {
    ClientInputQueue *queue;
    const _Atomic bool *running;
} InputThreadArgs;

typedef struct {
    const int *socket_fd;
    ServerInputQueue *queue;
    const _Atomic bool *running;
} ReceiveThreadArgs;

void *read_input_thread(void *arg);
void *recv_server_thread(void *arg);

#endif //SERPENT_CLIENT_H