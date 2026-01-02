#include "server.h"
#include "logging.h"

#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/un.h>
#include <string.h>
#include <stdbool.h>


int setup_server_socket(const char *path) {
    const int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        log_server("error creating server socket\n");
        return -1; // handle error
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    unlink(path); // remove existing socket file if any

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_server("error binding server socket\n");
        close(listen_fd);
        return -1; // handle error
    }

    if (listen(listen_fd, 16) < 0) {
        log_server("error listening on server socket\n");
        close(listen_fd);
        return -1; // handle error
    }

    return listen_fd;
}

void accept_connection(ClientRegistry *r, const int listen_fd) {
    char buf[64];
    snprintf(buf, sizeof buf, "listening for connections on fd %d\n", listen_fd);
    log_server(buf);

    const int client_fd = accept(listen_fd, NULL, NULL);

    snprintf(buf, sizeof buf, "accepted connection from fd %d\n", client_fd);
    log_server(buf);

    if (client_fd < 0) return; // handle error

    pthread_t input_thread;
    RecvInputThreadArgs args = {0};
    int rc = pthread_create(&input_thread, NULL, recv_input_thread, &args);
    if (rc != 0) {
        // handle error
        close(client_fd);
    }

    register_client(r, client_fd, input_thread);
    snprintf(buf, sizeof buf, "registered client fd %d\n", client_fd);
    log_server(buf);
}

void accept_loop(ClientRegistry *r, const int listen_fd, const _Atomic bool *accepting) {
    // TODO signal for graceful shutdown
    log_server("entering accept loop \n");
    while (*accepting) {
        accept_connection(r, listen_fd);
    }
}

void handle_event(const Event *ev, ActionQueue *q) {
    Action a;
    switch (ev->type) {
        case EV_CONNECTED:
            // add_player(&game_state)
            //ctx->game.players[ctx->game.player_count++] = ev->u.player_id;

            a.type = ACT_SEND_GAME_OVER;
            a.u.player_id = ev->u.player_id;
            enqueue_action(q, a);
            break;
        case EV_LOADED:
            // world loaded, can start game
            //ctx->world_loaded = true;
            break;
        case EV_PAUSED:
            // client paused game
            // game_state handle it ev->u.player_id
            break;
        case EV_RESUMED:
            // resume game
            // game_state handle it ev->u.player_id
            break;
        case EV_DISCONNECTED:
            // remove player from game state ev->u.player_id
            a.type = ACT_UNREGISTER_PLAYER;
            a.u.player_id = ev->u.player_id;
            enqueue_action(q, a);
            break;
        case EV_ERROR:
            // handle error by sending error msg to player ev->u.player_id
            a.type = ACT_SEND_ERROR;
            a.u.player_id = ev->u.player_id;
            enqueue_action(q, a);
            break;

        default:
            break;
    }
}

void exec_action(const Action *act, EventQueue *q) {
    Event ev;
    switch (act->type) {
        case ACT_LOAD_WORLD:
            // load_world(); // blocking
            ev.type = EV_LOADED;
            ev.u.nodata = 0;
            enqueue_event(q, ev);
            break;
        case ACT_SEND_READY:
            // send ready message to client act->u.player_id
            //send_msg(act->u.player_id, MSG_READY, NULL, 0); // blocking
            break;
        case ACT_SEND_GAME_OVER:
            // send ready message to client act->u.player_id
            //send_msg(act->u.player_id, MSG_GAME_OVER, NULL, 0); // blocking
            break;
        case ACT_BROADCAST_SHUT_DOWN:
            // send shutdown message to all clients
            // broadcast_msg(MSG_SHUT_DOWN, NULL, 0);
            break;
        case ACT_BROADCAST_GAME_STATE:
            // send game state act->u.state to all clients
            // TODO serialize state
            // broadcast_msg(MSG_GAME_STATE, NULL, 0);
            break;
        case ACT_UNREGISTER_PLAYER:
            // remove player from game state act->u.player_id
            break;
        case ACT_SEND_ERROR:
            // send error message to client act->u.player_id
            //send_msg(act->u.player_id, MSG_ERROR, NULL, 0); // blocking
            break;
        default:
            break;
    }
}

// thread functions

void *accept_loop_thread(void *arg) {
    const AcceptLoopThreadArgs *args = arg;

    ClientRegistry *registry = args->registry;
    const int listen_fd = args->listen_fd;
    const _Atomic bool *accepting = args->accepting;

    accept_loop(registry, listen_fd, accepting);

    return NULL;
}

void *action_thread(void *arg) {
    const ActionThreadArgs *args = arg;

    EventQueue *eq = args->eq;
    ActionQueue *aq = args->aq;
    const _Atomic bool *running = args->running;

    Action act;

    while (*running) {
        while (dequeue_action(aq, &act)) {
            exec_action(&act, eq);
        }
    }

    return NULL;
}

void *recv_input_thread(void *arg) {
    const RecvInputThreadArgs *args = arg;

    //InputQueue *queue = args->queue;
    const _Atomic bool *running = args->running;

    //recv_all(queue, running);
    // translate to Events
    // enqueue to server's input queue

    return NULL;
}