#include "registry.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>

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

size_t find_client(ClientRegistry *r, const int id) {
    for (size_t i = 0; i < r->count; ++i) {
        if (r->clients[i]->socket_fd == id) {
            return i;
        }
    }
    return (size_t)-1; // not found
}

void remove_client(ClientRegistry *r, const int id) {
    const size_t idx = find_client(r, id);

    Client *removed = r->clients[idx];

    close(removed->socket_fd);
    pthread_join(removed->thread, NULL);

    r->clients[idx] = r->clients[r->count - 1]; // TODO this is undesirable for order, maybe shift instead
    r->count--;

    free(removed);
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


