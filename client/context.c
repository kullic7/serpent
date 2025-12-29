#include "context.h"
#include <string.h>
#include <unistd.h>

#include "client.h"

void on_time_entered(void *ctx_ptr, const char *text) {
    const int seconds = strtol(text, NULL, 10);
    ClientContext *ctx = ctx_ptr;
    //ctx->game_config.time_limit = seconds; TODO

    menu_push(&ctx->menus, &ctx->world_select_menu);
}


void on_socket_path_entered(void *ctx_ptr, const char *path) {
    ClientContext *ctx = ctx_ptr;
    strncpy(ctx->server_path, path, sizeof(ctx->server_path));

    connect_to_server(ctx);

    //menu_push(&ctx->menus, &ctx->awaiting_menu);

    // TODO this is just for testing, should be after server confirmation
    ctx->mode = CLIENT_PLAYING;

}

void on_input_file_entered(void *ctx_ptr, const char *path) {
    ClientContext *ctx = ctx_ptr;

    // load world from file
    load_from_file(ctx, path);

    spawn_create_join_server(ctx);
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

void spawn_create_join_server(ClientContext *ctx) {
    spawn_server_process_if_needed(ctx);

    if (ctx->game_mode == GAME_SINGLE) {
        //connect_to_server(ctx); // creates connections and request join
        clear_menus_stack(&ctx->menus);
        menu_push(&ctx->menus, &ctx->awaiting_menu);
    }
    else {
        // for multiplayer we are in create "mode" so we just send create to server wo/ connect
        //send_msg(MSG_CREATE); // TODO send create to server
        menu_push(&ctx->menus, &ctx->join_menu);
    }
}

void spawn_server_process_if_needed(ClientContext *ctx) {
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
    }
}

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

void disconnect_from_server(ClientContext *ctx) {
    if (ctx->socket_fd >= 0) {
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }
}

void load_from_file(ClientContext *ctx, const char *file_path) {

}

void random_world(ClientContext *ctx) {

}