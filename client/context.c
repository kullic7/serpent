#define _POSIX_C_SOURCE 199309L
#include "context.h"
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <limits.h>

#include "client.h"

void on_time_entered(void *ctx_ptr, const char *text) {
    char *endptr = NULL;
    long seconds = strtol(text, &endptr, 10);
    if (endptr == text || *endptr != '\0' || seconds < 0 || seconds > INT_MAX) {
        // handle invalid input, e.g. set a default or show an error
        return;
    }
    ClientContext *ctx = ctx_ptr;
    ctx->time_remaining = (int)seconds; // TODO validation

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
        snprintf(ctx->error_menu.txt_fields[0].text, sizeof(ctx->error_menu.txt_fields[0].text),
             "Error. Could not connect to server.");
        clear_menus_stack(&ctx->menus);
        menu_push(&ctx->menus, &ctx->error_menu);
        return;
    }

    // TODO this is just for testing, should be after server confirmation
    ctx->mode = CLIENT_PLAYING;

    clear_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->awaiting_menu);

}


void on_input_file_entered(void *ctx_ptr, const char *path) {
    ClientContext *ctx = ctx_ptr;

    strncpy(ctx->file_path, path, sizeof(ctx->file_path));

    menu_push(&ctx->menus, &ctx->player_select_menu);
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

    // check if socket file not already taken
    if (wait_for_server_socket(ctx->server_path, 300)) {
        snprintf(ctx->error_menu.txt_fields[0].text, sizeof(ctx->error_menu.txt_fields[0].text),
             "Error. Socket address already taken. Use another.");
        return false;
    }

    if (!spawn_server_process(ctx)) {
        snprintf(ctx->error_menu.txt_fields[0].text, sizeof(ctx->error_menu.txt_fields[0].text),
             "Error. Spawning of server process failed.");
        return false;
    }

    // simplified readiness check for unix socket so we do not connect too early
    if (!wait_for_server_socket(ctx->server_path, 3000)) {
        snprintf(ctx->error_menu.txt_fields[0].text, sizeof(ctx->error_menu.txt_fields[0].text),
             "Error. Server did not create socket in time.");
        return false;
    }

    if (!connect_to_server(ctx)) {
        snprintf(ctx->error_menu.txt_fields[0].text, sizeof(ctx->error_menu.txt_fields[0].text),
             "Error. Could not connect to server.");
        return false;
    }

    return true;

}

bool spawn_server_process(ClientContext *ctx) {
    int pid = fork();
    if (pid == 0) {
        // child process: execute the server with appropriate arguments
        char time_arg[16];
        if (ctx->time_remaining >= 0) {
            snprintf(time_arg, sizeof(time_arg), "%d", ctx->time_remaining);
        } else {
            snprintf(time_arg, sizeof(time_arg), "-1");
        }

        execl("./serpent-server", "serpent-server",
            ctx->server_path,
            ctx->game_mode == GAME_SINGLE ? "1" : "0",
            time_arg,
            ctx->obstacles_enabled ? "1" : "0",
            ctx->obstacles_enabled && ctx->random_world_enabled ? "1" : "0",
            ctx->obstacles_enabled && !ctx->random_world_enabled ? ctx->file_path : "",
            NULL);
        // if execl returns, there was an error
        perror("execl failed");
        exit(1);
    } else if (pid < 0) {
        // fork failed
        perror("fork failed");
        return false; // indicate failure
    }

    return true; // parent process
}

void setup_input(ClientContext *ctx, const char *note) {
    ctx->input_mode = INPUT_TEXT;
    ctx->text_len = 0;
    strncpy(ctx->text_note, note, sizeof(ctx->text_note));
}

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

// server must create socket before this returns true
// we will wait for that ... simplified readiness check for unix socket
// blocking call
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
