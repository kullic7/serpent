#ifndef SERPENT_MENU_H
#define SERPENT_MENU_H

#include <stdlib.h>
#include "client/types.h"


void init_menus_stack(MenuStack *ms);
void clear_menus_stack(MenuStack *ms);

static void init_menu_fields(Menu *menu, Button *buttons, size_t b_count,
    TextField *txt_fields, size_t t_count, ClientContext *ctx);

void init_main_menu(Menu *menu, ClientContext *ctx);
void init_pause_menu(Menu *menu, ClientContext *ctx);
void init_new_game_menu(Menu *menu, ClientContext *ctx);

void init_multiplayer_menu(Menu *menu, ClientContext *ctx);

void init_mode_select_menu(Menu *menu, ClientContext *ctx);
void init_world_select_menu(Menu *menu, ClientContext *ctx);
void init_connect_menu(Menu *menu, ClientContext *ctx);
void init_game_over_menu(Menu *menu, ClientContext *ctx);

void menu_push(MenuStack *ms, Menu *menu);
void menu_pop(MenuStack *ms);
Menu *menu_current(MenuStack *ms);

// button actions/callbacks
void btn_new_game(void *ctx_ptr);
void btn_exit_game(void *ctx_ptr);

void btn_multiplayer(void *ctx_ptr);
void btn_create_game(void *ctx_ptr);
void btn_time_mode(void *ctx_ptr);
void btn_standard_mode(void *ctx_ptr);

void btn_world_select(void *ctx_ptr);
void btn_connect(void *ctx_ptr);
void btn_start_game(void *ctx_ptr);
void btn_resume_game(void *ctx_ptr);
void btn_exit_to_main_menu(void *ctx_ptr);
void btn_back_in_menu(void *ctx_ptr);

// helpers
void pause_game(ClientContext *ctx);
void connect_to_server(ClientContext *ctx);
void handle_text_input(ClientContext *ctx, Key key);
void on_time_entered(void *ctx_ptr, const char *text);
void on_socket_path_entered(void *ctx_ptr, const char *path);
void handle_game_over(void *ctx_ptr);


#endif //SERPENT_MENU_H