#ifndef SERPENT_REGISTRY_H
#define SERPENT_REGISTRY_H

#include <pthread.h>
#include <stdbool.h>
#include "logging.h"

typedef struct Client {
    //int id;
    int socket_fd;  // acts as unique identifier for client
    int time_joined;
    pthread_t thread;
} Client;

typedef struct {
    Client **clients; // dynamic array of pointers to clients (safer memory management when shared)
    size_t count;
    size_t capacity;
    pthread_mutex_t lock;
} ClientRegistry;

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

void registry_init(ClientRegistry *r);
void registry_destroy(ClientRegistry *r);
static void registry_grow(ClientRegistry *r);
void register_client(ClientRegistry *r, int client_fd, pthread_t thread); // main/accept thread responsibility
void remove_client(ClientRegistry *r, int id); // worker thread responsibility
size_t find_client(ClientRegistry *r, int id);  // id is socket_fd here or some unique identifier

void *recv_input_thread(void *arg);

void accept_connection(ClientRegistry *r, int listen_fd);
void accept_loop(ClientRegistry *r, int listen_fd, const _Atomic bool *accepting);


#endif //SERPENT_REGISTRY_H