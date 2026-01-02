#include "buttons.h"
#include "context.h"
#include <string.h>


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
    clear_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->main_menu);
    menu_push(&ctx->menus, &ctx->mode_select_menu);
}

void btn_connect_to_server(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;

    setup_input(ctx, "Server address (socket path): ");

    ctx->on_text_submit = on_socket_path_entered_when_joining;
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


// mode select menu
void btn_standard_mode(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    ctx->time_remaining = -1;
    menu_push(&ctx->menus, &ctx->world_select_menu);
}

void btn_time_mode(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;

    setup_input(ctx, "Time Limit (seconds): ");

    ctx->on_text_submit = on_time_entered;
}

// world select menu
void btn_easy_world_mode(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    ctx->obstacles_enabled = false;
    menu_push(&ctx->menus, &ctx->player_select_menu);

}

void btn_hard_world_mode(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    ctx->obstacles_enabled = true;
    menu_push(&ctx->menus, &ctx->load_menu);
}


// player select menu
void btn_single_player(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    ctx->game_mode = GAME_SINGLE;

    strncpy(ctx->server_path, "/tmp/serpent_single_player.sock", sizeof(ctx->server_path));

    clear_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->awaiting_menu);

    if (!spawn_connect_create_server(ctx)) {
        ctx->mode = CLIENT_MENU;
        clear_menus_stack(&ctx->menus);
        menu_push(&ctx->menus, &ctx->error_menu);
        return;
    }

    // TODO this is just for testing, should be after server confirmation
    ctx->mode = CLIENT_PLAYING;

}

void btn_multiplayer(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    ctx->game_mode = GAME_MULTI;

    setup_input(ctx, "Server address (socket path): ");

    ctx->on_text_submit = on_socket_path_entered_when_creating;

}


// load menu
void btn_random_world(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    ctx->random_world_enabled = true;
    menu_push(&ctx->menus, &ctx->player_select_menu);
}

void btn_load_from_file(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;

    setup_input(ctx, "Load file path: ");

    ctx->on_text_submit = on_input_file_entered;

}

// awaiting menu
void btn_cancel_awaiting(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    //send_msg(MSG_LEAVE); TODO notify server of leaving
    ctx->mode = CLIENT_MENU;
    clear_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->main_menu);
}