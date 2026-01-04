#ifndef SERPENT_CLIENT_H
#define SERPENT_CLIENT_H

#include "input.h"
#include "context.h"

void client_init(ClientContext *ctx);

void client_run(ClientContext *ctx, ClientInputQueue *iq, ServerInputQueue *sq);

void client_cleanup(ClientContext *ctx);

static void init_menu_fields(Menu *menu, Button *buttons, size_t b_count,
    TextField *txt_fields, size_t t_count, ClientContext *ctx);

void init_main_menu(Menu *menu, ClientContext *ctx);
void init_pause_menu(Menu *menu, ClientContext *ctx);

void init_mode_select_menu(Menu *menu, ClientContext *ctx);
void init_world_select_menu(Menu *menu, ClientContext *ctx);
void init_load_menu(Menu *menu, ClientContext *ctx);
void init_player_select_menu(Menu *menu, ClientContext *ctx);

void init_game_over_menu(Menu *menu, ClientContext *ctx);

void init_awaiting_menu(Menu *menu, ClientContext *ctx);
void init_error_menu(Menu *menu, ClientContext *ctx);

// input handlers
void handle_menu_key(Menu *menu, Key key); // should modify only menu subctx
void handle_game_key(Key key, ClientContext *ctx); // should map key to direction and send to server ... menu, Key, mode
void handle_server_msg(ClientContext *ctx, Message msg); // should modify game sub context
void handle_text_input(ClientContext *ctx, Key key); // should modify oly input sub context

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