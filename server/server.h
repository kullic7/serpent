#ifndef SERPENT_SERVER_H
#define SERPENT_SERVER_H

#include <events.h>
#include "registry.h"

// infrastructure

int setup_server_socket(const char *path);

void accept_connection(ClientRegistry *r, int listen_fd);
void accept_loop(ClientRegistry *r, int listen_fd, const _Atomic bool *accepting);

// handlers

void exec_action(const Action *act, EventQueue *q); // actions come via action queue and are handled in worker thread only
void handle_event(const Event *ev, ActionQueue *q); // events come via event queue and are handled in main thread only


// worker thread function

typedef struct {
    EventQueue *eq;
    ActionQueue *aq;
    const _Atomic bool *running;
} ActionThreadArgs;

typedef struct {
    //InputQueue *queue;
    const int client_fd;
    const _Atomic bool *running;
} RecvInputThreadArgs;

typedef struct {
    ClientRegistry *registry;
    int listen_fd;
    const _Atomic bool *accepting;
} AcceptLoopThreadArgs;

void *accept_loop_thread(void *arg);
void *action_thread(void *arg);
void *recv_input_thread(void *arg);


#endif //SERPENT_SERVER_H