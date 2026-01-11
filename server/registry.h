#ifndef SERPENT_REGISTRY_H
#define SERPENT_REGISTRY_H

#include <pthread.h>

typedef struct Client {
    int socket_fd;  // acts as unique identifier for client
    int time_joined;
    pthread_t thread;
}   Client;

typedef struct {
    Client **clients; // dynamic array of pointers to clients (safer memory management when shared)
    size_t count;
    size_t capacity;
    pthread_mutex_t lock;
} ClientRegistry;

void registry_init(ClientRegistry *r);
void registry_destroy(ClientRegistry *r);
static void registry_grow(ClientRegistry *r);
void register_client(ClientRegistry *r, int client_fd, pthread_t thread); // main/accept thread responsibility
void remove_client(ClientRegistry *r, int id); // worker thread responsibility
static size_t find_client(ClientRegistry *r, int id);  // id is socket_fd here or some unique identifier

#endif //SERPENT_REGISTRY_H