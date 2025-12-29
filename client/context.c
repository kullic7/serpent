#define _POSIX_C_SOURCE 199309L
#include "context.h"
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "client.h"

void on_time_entered(void *ctx_ptr, const char *text) {
    const int seconds = strtol(text, NULL, 10);
    ClientContext *ctx = ctx_ptr;
    ctx->time_remaining = seconds; // TODO validation

    menu_push(&ctx->menus, &ctx->world_select_menu);
}


void on_socket_path_entered_when_creating(void *ctx_ptr, const char *path) {
    ClientContext *ctx = ctx_ptr;
    strncpy(ctx->server_path, path, sizeof(ctx->server_path));

    if (!spawn_connect_create_server(ctx)) {
        ctx->mode = CLIENT_MENU;
        clear_menus_stack(&ctx->menus);
        menu_push(&ctx->menus, &ctx->awaiting_menu); // todo add error menu
        return;
    }

    // TODO this is just for testing, should be after server confirmation
    ctx->mode = CLIENT_PLAYING;

    clear_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->awaiting_menu);

}

void on_socket_path_entered_when_joining(void *ctx_ptr, const char *path) {
    ClientContext *ctx = ctx_ptr;
    strncpy(ctx->server_path, path, sizeof(ctx->server_path));

    if (!connect_to_server(ctx)) {
        ctx->mode = CLIENT_MENU;
        clear_menus_stack(&ctx->menus);
        menu_push(&ctx->menus, &ctx->awaiting_menu); // todo add error menu
        return;
    }

    //send_msg(MSG_JOIN); TODO send join request to server

    // TODO this is just for testing, should be after server confirmation
    ctx->mode = CLIENT_PLAYING;

    clear_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->awaiting_menu);

}


void on_input_file_entered(void *ctx_ptr, const char *path) {
    ClientContext *ctx = ctx_ptr;

    // load world from file
    load_from_file(ctx, path);

    menu_push(&ctx->menus, &ctx->player_select_menu);

    //spawn_create_join_server(ctx);
}


void on_game_over(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;

    ctx->mode = CLIENT_GAME_OVER;

    snprintf(ctx->game_over_menu.txt_fields[0].text, sizeof(ctx->game_over_menu.txt_fields[0].text),
             "Game over. Your score is: %d", ctx->score);
}

void on_game_ready(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;

    ctx->mode = CLIENT_PLAYING;

}

// used by very first client to spawn server and connect to it
// single or multi player
bool spawn_connect_create_server(ClientContext *ctx) {
    if (!spawn_server_process(ctx)) {
        // error handling (menu, message, etc.)
        // show error menu TODO
        return false;
    }

    if (!wait_for_server_socket(ctx->server_path, 3000)) {
        // server failed to start
        // show error menu TODO
        return false;
    }

    if (!connect_to_server(ctx)) {
        // could not connect
        // show error menu  TODO
        return false;
    }

    // send_msg(MSG_CREATE);
    return true;

}

int spawn_server_process(ClientContext *ctx) {
    int pid = fork();
    if (pid == 0) {
        // Child process: execute the server
        execl("./serpent_server", "./serpent_server", NULL);
        // If execl returns, there was an error
        perror("execl failed");
        exit(1);
    } else if (pid < 0) {
        // Fork failed
        perror("fork failed");
        return 1; // Indicate failure
    }

    return 0; // Parent process
}

void setup_input(ClientContext *ctx, const char *note) {
    ctx->input_mode = INPUT_TEXT;
    ctx->text_len = 0;
    strncpy(ctx->text_note, note, sizeof(ctx->text_note));
}

/*
void connect_to_server(ClientContext *ctx) {
    // TODO: Implement actual Unix domain socket connection
    // struct sockaddr_un addr;
    // ctx->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    // addr.sun_family = AF_UNIX;
    // strncpy(addr.sun_path, ctx->server_path, sizeof(addr.sun_path) - 1);
    // connect(ctx->socket_fd, (struct sockaddr*)&addr, sizeof(addr));

    // todo error handling and check

    ctx->socket_fd = 1; // dummy socket fd for now

    // send_msg(MSG_JOIN); todo
}
*/

bool connect_to_server(ClientContext *ctx) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ctx->server_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return false;
    }

    ctx->socket_fd = fd;
    return true;
}

// server must bind to the socket path before this returns true
// we will wait for that
bool wait_for_server_socket(const char *path, int timeout_ms) {
    const int step_ms = 50;
    int waited = 0;

    struct stat st;

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = step_ms * 1000000L;

    while (waited < timeout_ms) {
        if (stat(path, &st) == 0) {
            return true; // socket exists
        }

        nanosleep(&ts, NULL);
        waited += step_ms;
    }

    return false;
}


void disconnect_from_server(ClientContext *ctx) {
    if (ctx->socket_fd >= 0) {
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }
}

void load_from_file(ClientContext *ctx, const char *file_path) {

}
