#include "registry.h"
#include <stdlib.h>
#include <unistd.h>

void registry_init(ClientRegistry *r) {
    r->capacity = 8;
    r->count = 0;
    r->clients = malloc(r->capacity * sizeof(Client *));
    pthread_mutex_init(&r->lock, NULL);
}

void registry_destroy(ClientRegistry *r) {
    for (size_t i = 0; i < r->count; ++i) {
        Client *c = r->clients[i];
        close(c->socket_fd);
        pthread_join(c->thread, NULL);
        free(c);
    }
    free(r->clients);
    pthread_mutex_destroy(&r->lock);
}

// implicitly assumes lock is held when called
static void registry_grow(ClientRegistry *r) {
    const size_t new_capacity = r->capacity * 2;
    Client **new_clients = realloc(r->clients, new_capacity * sizeof(Client *));
    if (new_clients == NULL) {
        return;
    }
    r->capacity = new_capacity;
    r->clients = new_clients;
}


void register_client(ClientRegistry *r, const int client_fd, const pthread_t thread) {
    Client *c = malloc(sizeof(Client));
    c->socket_fd = client_fd;
    c->thread = thread;
    // TODO assign and time_joined

    pthread_mutex_lock(&r->lock);

    if (r->count == r->capacity)
        registry_grow(r);

    r->clients[r->count++] = c;

    pthread_mutex_unlock(&r->lock);
}

// implicitly assumes lock is held when called
static size_t find_client(ClientRegistry *r, const int id) {
    for (size_t i = 0; i < r->count; ++i) {
        if (r->clients[i]->socket_fd == id) {
            return i;
        }
    }
    return (size_t)-1; // not found
}

void remove_client(ClientRegistry *r, const int id) {
    pthread_mutex_lock(&r->lock);

    const size_t idx = find_client(r, id);
    if (idx == (size_t)-1) {
        pthread_mutex_unlock(&r->lock);
        return;
    }

    Client *removed = r->clients[idx];

    r->clients[idx] = r->clients[r->count - 1];
    r->count--;

    pthread_mutex_unlock(&r->lock);

    close(removed->socket_fd);
    pthread_join(removed->thread, NULL);
    free(removed);
}



