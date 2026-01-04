#include "server.h"
#include "logging.h"

#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <string.h>
#include <stdbool.h>
#include <poll.h>

#include "timer.h"


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

int accept_connection(ClientRegistry *r, const int listen_fd, RecvInputThreadArgs *args) {
    char buf[64];
    snprintf(buf, sizeof buf, "listening for connections on fd %d\n", listen_fd);
    log_server(buf);

    const int client_fd = accept(listen_fd, NULL, NULL);

    if (client_fd < 0) return -1; // handle error

    snprintf(buf, sizeof buf, "accepted connection from fd %d\n", client_fd);
    log_server(buf);

    pthread_t input_thread;
    args->client_fd = client_fd;

    int rc = pthread_create(&input_thread, NULL, recv_input_thread, args);
    if (rc != 0) {
        // handle error
        close(client_fd);
        return -1;
    }

    register_client(r, client_fd, input_thread);
    snprintf(buf, sizeof buf, "registered client fd %d\n", client_fd);
    log_server(buf);

    return 0;
}

void accept_loop(ClientRegistry *r, const int listen_fd, const _Atomic bool *accepting, RecvInputThreadArgs *recv_args) {
    // TODO signal for graceful shutdown
    log_server("entering accept loop \n");
    while (*accepting) {
        if (accept_connection(r, listen_fd, recv_args) == -1) {;
            log_server("error accepting connection \n");
            // continue accepting other connections
        }
    }
}

bool handle_event(const Event *ev, ActionQueue *q, GameState *game) {
    Action a = {0};
    switch (ev->type) {
        case EV_CONNECTED:
            game_add_fruit(game);
            game_add_player(game, ev->u.player_id);

            log_server("ev connected received\n");
            enqueue_action(q, (Action){ACT_SEND_READY, .u.player_id = ev->u.player_id});
            log_server("act send ready enqueued\n");
            break;
        case EV_LOADED:
            // world loaded, can start game
            //ctx->world_loaded = true; TODO
            break;
        case EV_INPUT:
            // update player input in game state ev->u.input
            game_update_player_direction(game, ev->u.input.player_id, ev->u.input.direction);
            log_server("ev input received\n");
            break;
        case EV_PAUSED:
            // client paused game
            game_pause_player(game, ev->u.player_id);
            log_server("ev paused received\n");
            break;
        case EV_RESUMED:
            // resume game  player is paused for 3 seconds
            game_schedule_resume_player(game, ev->u.player_id);
            log_server("ev resumed received\n");
            a.type = ACT_WAIT_PAUSED;
            a.u.wait = (ActArgPlayerWait){ ev->u.player_id, 3 };
            enqueue_action(q, a);
            log_server("act wait paused enqueued\n");
            break;
        case EV_WAITED_AFTER_RESUME:
            game_resume_player(game, ev->u.player_id);
            log_server("ev waited after resume received\n");
            break;
        case EV_DISCONNECTED:
            // remove player from game state ev->u.player_id
            game_remove_player(game, ev->u.player_id);
            log_server("ev disconnected received\n");
            a.type = ACT_UNREGISTER_PLAYER;
            a.u.player_id = ev->u.player_id;
            enqueue_action(q, a);
            log_server("act unregister player enqueued\n");
            break;
        case EV_WAITED_FOR_GAME_OVER:
            // game time elapsed we signal game loop to end
            log_server("ev waited for game over received\n");
            game->wait_for_end_pending = false;
            if (game->player_count <= 0) return true; // no players left after wait time
            break;
        case EV_ERROR:
            // handle error by sending error msg to player ev->u.player_id
            log_server("ev error received\n");
            a.type = ACT_SEND_ERROR;
            a.u.player_id = ev->u.player_id;
            enqueue_action(q, a);
            log_server("act send error enqueued\n");
            break;

        default:
            break;

    }
    return false;
}


bool handle_end_event(const bool timed_mode, const bool single_player, const GameState *state, ActionQueue *aq) {
    if (!timed_mode) {
        // no time limit -> standard mode
        if (state->player_count == 0) {
            if (single_player) {
                log_server("single player no players left -> shutdown\n");
                return true; // shutdown immediately
            }

            if (!state->wait_for_end_pending) {
                Action act;
                act.type = ACT_WAIT_FOR_END;
                act.u.end_in_seconds = 10; // wait 10 seconds before shutdown
                enqueue_action(aq, act);
                log_server("act wait for end enqueued due no players left\n");
                ((GameState *)state)->wait_for_end_pending = true;
            }
            return false;
        }
    }
    else {
        if ( timer_expired(&state->timer) ) {
            log_server("timer expired\n");
            return true; // time limit reached
        }
    }
    return 0;
}

static void *resume_wait_thread(void *arg) {
    TimerThreadArgs *args = arg;
    sleep(args->seconds);

    Event ev;
    ev.type = EV_WAITED_AFTER_RESUME;
    ev.u.player_id = args->player_id;
    enqueue_event(args->eq, ev);

    free(args);
    return NULL;
}

static void *end_wait_thread(void *arg) {
    TimerThreadArgs *args = arg;
    sleep(args->seconds);

    Event ev;
    ev.type = EV_WAITED_FOR_GAME_OVER;
    ev.u.nodata = 0;
    enqueue_event(args->eq, ev);

    free(args);
    return NULL;
}

void exec_action(const Action *act, EventQueue *q, ClientRegistry *reg) {
    Event ev;
    switch (act->type) {
        case ACT_LOAD_WORLD:
            // load_world(); // blocking TODO
            ev.type = EV_LOADED;
            ev.u.nodata = 0;
            enqueue_event(q, ev);
            log_server("act load world executed\n");
            log_server("event loaded enqueued\n");
            break;
        case ACT_SEND_READY:
            // send ready message to client act->u.player_id
            if (send_ready(act->u.player_id) < 0) log_server("FAILED\n");
            log_server("act send ready executed\n");
            break;
        case ACT_SEND_GAME_OVER:
            // send ready message to client act->u.player_id
            send_game_over(act->u.player_id);
            log_server("act send game over executed\n");
            break;
        case ACT_BROADCAST_GAME_STATE:
            // send game state act->u.state to all clients
            broadcast_game_state(reg, act->u.game);
            snapshot_destroy(act->u.game.state); // free dynamic arrays inside
            free(act->u.game.state); // free the struct itself
            log_server("act send broadcast game state executed\n");
            break;
        case ACT_UNREGISTER_PLAYER:
            // remove player from registry
            remove_client(reg, act->u.player_id);
            log_server("act unregister client executed\n");
            break;
        case ACT_WAIT_FOR_END: {
            TimerThreadArgs *args = malloc(sizeof(*args));
            if (!args) break; /* handle alloc failure */

            args->eq = q;
            args->player_id = 0;
            args->seconds = act->u.end_in_seconds;

            pthread_t tid;
            if (pthread_create(&tid, NULL, end_wait_thread, args) == 0) {
                pthread_detach(tid);
            } else {
                free(args);
            }
            log_server("act wait for end executed\n");
            break;
        }
        case ACT_WAIT_PAUSED: {
            TimerThreadArgs *args = malloc(sizeof(*args));
            if (!args) break; /* handle alloc failure */

            args->eq = q;
            args->player_id = act->u.wait.player_id;
            args->seconds = act->u.wait.seconds;

            pthread_t tid;
            if (pthread_create(&tid, NULL, resume_wait_thread, args) == 0) {
                pthread_detach(tid);
            } else {
                free(args);
            }
            log_server("act wait paused executed\n");
            break;
        }
        case ACT_SEND_ERROR:
            // send error message to client act->u.player_id TODO
            //send_msg(act->u.player_id, MSG_ERROR, NULL, 0); // blocking
            break;
        default:
            break;
    }
}

// translation
// message to event

int msg_to_event(const Message *msg, const int client_fd, Event *ev) {

    Direction dir;

    switch (msg->type) {
        case MSG_INPUT:

            msg_to_input(msg, &dir);

            ev->type = EV_INPUT;
            ev->u.input.player_id = client_fd;
            ev->u.input.direction = dir;
            break;
        case MSG_PAUSE:
            ev->type = EV_PAUSED;
            ev->u.player_id = client_fd;
            break;
        case MSG_RESUME:
            ev->type = EV_RESUMED;
            ev->u.player_id = client_fd;
            break;
        case MSG_LEAVE:
            ev->type = EV_DISCONNECTED;
            ev->u.player_id = client_fd;
            break;
        default: return -1; // unknown message type
    }
    return 0;
}


void game_run(const bool timed_mode, const bool single_player, GameState *game, EventQueue *eq, ActionQueue *aq) {

    while (true) {

        game_snapshot_each_client(game, aq);

        sleep_frame(GAME_TICK_TIME_MS);

        game_update(game);

        // handle events
        Event ev = {0};
        bool end_game = false;
        while (dequeue_event(eq, &ev)) {
            end_game = handle_event(&ev, aq, game);
        }
        if (end_game) break;

        end_game = handle_end_event(timed_mode, single_player, game, aq);

        if (end_game) break;

    }
}

// thread functions

/*
void *accept_loop_thread(void *arg) {
    const AcceptLoopThreadArgs *args = arg;

    ClientRegistry *registry = args->registry;
    const int listen_fd = args->listen_fd;
    const _Atomic bool *accepting = args->accepting;
    RecvInputThreadArgs *recv_args = args->recv_args; // dummy, will be set in accept_connection

    accept_loop(registry, listen_fd, accepting, recv_args);

    log_server("THREAD: ACCEPT completed\n");

    return NULL;
}
*/

void *accept_loop_thread(void *arg) {
    const AcceptLoopThreadArgs *args = arg;

    ClientRegistry *registry = args->registry;
    const int listen_fd = args->listen_fd;
    const _Atomic bool *accepting = args->accepting;
    RecvInputThreadArgs *recv_args = args->recv_args; // dummy, will be set in accept_connection

    struct pollfd pfd;
    pfd.fd = listen_fd;
    pfd.events = POLLIN;

    log_server("THREAD: ACCEPT using poll\n");

    while (*accepting) {
        int rc = poll(&pfd, 1, 100); // 100 ms timeout to re-check *accepting
        if (rc < 0) {
            log_server("FAILED: poll on listen_fd\n");
            break;
        } else if (rc == 0) {
            // timeout, re-check *accepting
            continue;
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            log_server("poll: listen socket error/hangup\n");
            break;
        }

        if (pfd.revents & POLLIN) {
            if (accept_connection(registry, listen_fd, recv_args) == -1) {
                log_server("error accepting connection listen fd likely closed\n");
                break;
                // continue; keep polling for new connections
            }
        }
    }

    log_server("THREAD: ACCEPT completed\n");

    return NULL;
}

void *action_thread(void *arg) {
    const ActionThreadArgs *args = arg;

    EventQueue *eq = args->eq;
    ActionQueue *aq = args->aq;
    ClientRegistry *reg = args->reg;
    const _Atomic bool *running = args->running;

    Action act;

    while (*running) {
        while (dequeue_action(aq, &act)) {
            exec_action(&act, eq, reg);
        }
    }
    log_server("THREAD: ACTION completed\n");

    return NULL;
}

/*
void *recv_input_thread(void *arg) {
    const RecvInputThreadArgs *args = arg;

    EventQueue *eq = args->eq;
    const int client_fd = args->client_fd;
    const _Atomic bool *running = args->running;

    // first signal that client connected
    enqueue_event(eq, (Event){ .type = EV_CONNECTED, .u.player_id = client_fd });

    while (*running) {
        // blocking receive message from client_fd
        Message msg = {0};
        Event ev = {0};
        if (recv_message(client_fd, &msg) < 0) log_server("FAILED: recv message\n");

        // translate to event
        if (msg_to_event(&msg, client_fd, &ev) == 0) {
            // enqueue to event queue
            enqueue_event(eq, ev); // by value so its safe wrt scope
        }

        message_destroy(&msg); // free payload if any
    }
    log_server("THREAD: RECEIVE completed\n");

    return NULL;
}
*/

void *recv_input_thread(void *arg) {
    const RecvInputThreadArgs *args = arg;

    EventQueue *eq = args->eq;
    const int client_fd = args->client_fd;
    const _Atomic bool *running = args->running;

    struct pollfd pfd;
    pfd.fd = client_fd;
    pfd.events = POLLIN;

    // first signal that client connected
    enqueue_event(eq, (Event){ .type = EV_CONNECTED, .u.player_id = client_fd });

    while (*running) {
        int rc = poll(&pfd, 1, 100); // 100 ms timeout
        if (rc < 0) {
            log_server("FAILED: poll\n");
            break;
        } else if (rc == 0) {
            // timeout, just continue to re-check *running
            continue;
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            log_server("poll: socket error/hangup\n");
            break;
        }

        if (pfd.revents & POLLIN) {
            Message msg = (Message){0};
            Event ev = (Event){0};

            if (recv_message(client_fd, &msg) < 0) {
                log_server("FAILED: recv message or client closed\n");
                message_destroy(&msg);
                enqueue_event(eq, (Event){ .type = EV_DISCONNECTED, .u.player_id = client_fd });
                break;
            }

            if (msg_to_event(&msg, client_fd, &ev) == 0) {
                enqueue_event(eq, ev); // by value so it is safe
            }

            message_destroy(&msg); // free payload if any
        }
    }

    log_server("THREAD: RECEIVE completed\n");

    return NULL;
}