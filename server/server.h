#ifndef SERPENT_SERVER_H
#define SERPENT_SERVER_H

#include <events.h>
#include "protocol.h"
#include "registry.h"
#include "game.h"

typedef struct {
    EventQueue *eq;
    ActionQueue *aq;
    ClientRegistry *reg;
    const _Atomic bool *running;
} ActionThreadArgs;

typedef struct {
    EventQueue *eq;
    int client_fd;
    const _Atomic bool *running;
} RecvInputThreadArgs;

typedef struct {
    ClientRegistry *registry;
    int listen_fd;
    const _Atomic bool *accepting;
    RecvInputThreadArgs *recv_args;
} AcceptLoopThreadArgs;

typedef struct {
    EventQueue *eq;
    int player_id;
    int seconds;
} TimerThreadArgs;

// game
void game_run(bool timed_mode, bool single_player, GameState *game, EventQueue *eq, ActionQueue *aq);

// infrastructure

int setup_server_socket(const char *path);

int accept_connection(ClientRegistry *r, int listen_fd, RecvInputThreadArgs *args);
void accept_loop(ClientRegistry *r, int listen_fd, const _Atomic bool *accepting, RecvInputThreadArgs *recv_args);

// handlers

void exec_action(const Action *act, EventQueue *q, ClientRegistry *reg); // actions come via action queue and are handled in worker thread only
bool handle_event(const Event *ev, ActionQueue *q, GameState *state); // events come via event queue and are handled in main thread only

bool handle_end_event(bool timed_mode, bool single_player, const GameState *state, ActionQueue *aq);


int broadcast_game_state(ClientRegistry *reg, WorldState state);

// translation (handles input thread)
// message to event
int msg_to_event(const Message *msg, int client_fd, Event *ev);

// thread function

void *accept_loop_thread(void *arg);
void *action_thread(void *arg);
void *recv_input_thread(void *arg);
static void *resume_wait_thread(void *arg);
static void *end_wait_thread(void *arg);


#endif //SERPENT_SERVER_H