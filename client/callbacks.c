#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <limits.h>
#include <stdio.h>
#include "callbacks.h"
#include "context.h"
#include "protocol.h"
#include "client.h"
#include "logging.h"

//gggggg
void btn_back_in_menu(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    menu_pop(&ctx->menus);
    Menu* m = menu_current(&ctx->menus);
    m->selected_index = 0; // reset selection on back
}

void btn_exit_to_main_menu(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;


    if (ctx->socket_fd >= 0) {
        log_client("sending leave message to server\n");
        if (send_leave(ctx->socket_fd) < 0) {
            log_client("FAILED: to send leave\n");
        } else log_client("send\n");

        disconnect_from_server(ctx);
    }

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

    log_client("sending resume message to server\n");
    if (send_resume(ctx->socket_fd) < 0) {
        log_client("FAILED: to send resume\n");
    } else log_client("send\n");

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

    strncpy(ctx->server_path, "serpent_single_player.sock", sizeof(ctx->server_path));

    clear_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->awaiting_menu);

    if (!spawn_connect_create_server(ctx)) {
        ctx->mode = CLIENT_MENU;
        clear_menus_stack(&ctx->menus);
        menu_push(&ctx->menus, &ctx->error_menu);
    }

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

    // send leave must precede disconnect
    log_client("send leave message to server\n");
    if (send_leave(ctx->socket_fd) < 0) {
        log_client("FAILED: to send leave\n");
    } else log_client("send\n");

    disconnect_from_server(ctx);

    ctx->mode = CLIENT_MENU;
    clear_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->main_menu);
}

// text input handling callbacks

void on_time_entered(void *ctx_ptr, const char *text) {
    ClientContext *ctx = ctx_ptr;

    char *endptr = NULL;
    const long seconds = strtol(text, &endptr, 10);

    if (endptr == text || *endptr != '\0' || seconds < 0 || seconds > INT_MAX) {
        snprintf(ctx->error_menu.txt_fields[0].text,
             sizeof(ctx->error_menu.txt_fields[0].text),
             "Invalid time. Enter a non\-negative integer.");
        clear_menus_stack(&ctx->menus);
        menu_push(&ctx->menus, &ctx->error_menu);
        return;
    }
    ctx->time_remaining = (int)seconds;

    menu_push(&ctx->menus, &ctx->world_select_menu);
}


void on_socket_path_entered_when_creating(void *ctx_ptr, const char *path) {
    ClientContext *ctx = ctx_ptr;
    strncpy(ctx->server_path, path, sizeof(ctx->server_path));

    if (!spawn_connect_create_server(ctx)) {
        ctx->mode = CLIENT_MENU;
        clear_menus_stack(&ctx->menus);
        menu_push(&ctx->menus, &ctx->error_menu);
        return;
    }

    clear_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->awaiting_menu);

}

void on_socket_path_entered_when_joining(void *ctx_ptr, const char *path) {
    ClientContext *ctx = ctx_ptr;
    strncpy(ctx->server_path, path, sizeof(ctx->server_path));

    if (!connect_to_server(ctx)) {
        ctx->mode = CLIENT_MENU;
        snprintf(ctx->error_menu.txt_fields[0].text, sizeof(ctx->error_menu.txt_fields[0].text),
             "Error. Could not connect to server.");
        clear_menus_stack(&ctx->menus);
        menu_push(&ctx->menus, &ctx->error_menu);
        return;
    }

    clear_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->awaiting_menu);

}


void on_input_file_entered(void *ctx_ptr, const char *path) {
    ClientContext *ctx = ctx_ptr;

    strncpy(ctx->file_path, path, sizeof(ctx->file_path));

    ctx->file_path[sizeof(ctx->file_path) - 1] = '\0';

    // client side check if file exists (possible because client and server are on same machine)
    if (access(ctx->file_path, F_OK) != 0) {
        snprintf(ctx->error_menu.txt_fields[0].text,
                 sizeof(ctx->error_menu.txt_fields[0].text),
                 "Error. File does not exist on machine.");
        clear_menus_stack(&ctx->menus);
        menu_push(&ctx->menus, &ctx->error_menu);
        return;
    }



    menu_push(&ctx->menus, &ctx->player_select_menu);
}