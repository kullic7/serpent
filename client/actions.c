#include "actions.h"
#include "context.h"
#include "string.h"


void btn_back_in_menu(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    menu_pop(&ctx->menus);
    Menu* m = menu_current(&ctx->menus);
    m->selected_index = 0; // reset selection on back
}

void btn_exit_to_main_menu(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;

    /*
    if (ctx->socket_fd >= 0) {
        send_message(MSG_LEAVE, ctx, 0); // notify server of leaving
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }
    */

    ctx->mode = CLIENT_MENU;
    clear_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->main_menu);
}

// main menu
void btn_new_game(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    menu_push(&ctx->menus, &ctx->new_game_menu);
}

void btn_exit_game(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    ctx->running = false;
}

// pause menu
void btn_resume_game(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    menu_pop(&ctx->menus);

    //send_msg(MSG_RESUME); TODO notify server

    ctx->mode = CLIENT_PLAYING;
}


// new game menu
void btn_single_player(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    ctx->game_mode = GAME_SINGLE;
    menu_push(&ctx->menus, &ctx->mode_select_menu);
}

void btn_multiplayer(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    ctx->game_mode = GAME_MULTI;
    menu_push(&ctx->menus, &ctx->multiplayer_menu);
}

// mode select menu
void btn_standard_mode(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    //ctx->game_config.time_limit = 10; TODO set othr config options
    menu_push(&ctx->menus, &ctx->world_select_menu);
}

void btn_time_mode(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;

    ctx->input_mode = INPUT_TEXT;
    ctx->text_len = 0;
    strncpy(ctx->text_note, "Time Limit (seconds): ", sizeof(ctx->text_note));

    ctx->on_text_submit = on_time_entered;
}

// world select menu
void btn_easy_world_mode(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    //ctx->game_config.obstacles_enabled = false; TODO set other config options

    spawn_create_join_server(ctx);
}

void btn_hard_world_mode(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    //ctx->game_config.obstacles_enabled = true; TODO set other config options
    menu_push(&ctx->menus, &ctx->load_menu);
}

// multiplayer menu
void btn_connect_to_server(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;

    ctx->input_mode = INPUT_TEXT;
    ctx->text_len = 0;
    strncpy(ctx->text_note, "Server address (socket path): ", sizeof(ctx->text_note));

    ctx->on_text_submit = on_socket_path_entered;
}

void btn_create_game(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    menu_push(&ctx->menus, &ctx->mode_select_menu);
}

// load menu
void btn_random_world(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    random_world(ctx);

    spawn_create_join_server(ctx);
}

void btn_load_from_file(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;

    ctx->input_mode = INPUT_TEXT;
    ctx->text_len = 0;
    strncpy(ctx->text_note, "Load file path: ", sizeof(ctx->text_note));

    ctx->on_text_submit = on_input_file_entered;
}

// after multiplayer create game menu
void btn_join_game(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    ctx->mode = CLIENT_MENU;
    //send_msg(MSG_JOIN); TODO send join request to server
    clear_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->awaiting_menu);
}

// awaiting menu
void btn_cancel_awaiting(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    //send_msg(MSG_LEAVE); TODO notify server of leaving
    ctx->mode = CLIENT_MENU;
    clear_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->main_menu);
}