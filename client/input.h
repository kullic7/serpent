#ifndef SERPENT_INPUT_H
#define SERPENT_INPUT_H

#include "game_types.h"
#include <pthread.h>
#include "protocol.h"

typedef size_t Key;
typedef struct ClientInputQueue {
    Key events[MAX_KEY_EVENTS];
    size_t count;
    pthread_mutex_t lock;
} ClientInputQueue;

typedef struct ServerInputQueue {
    Message events[MAX_MESSAGES];
    size_t count;
    pthread_mutex_t lock;
    pthread_cond_t not_full;
} ServerInputQueue;

void client_input_queue_init(ClientInputQueue *q);
void client_input_queue_destroy(ClientInputQueue *q);

void server_input_queue_init(ServerInputQueue *q);
void server_input_queue_destroy(ServerInputQueue *q);

void enqueue_key(ClientInputQueue *q, Key key);
bool dequeue_key(ClientInputQueue *q, Key *key);
void client_input_queue_flush(ClientInputQueue *q);

void enqueue_msg(ServerInputQueue *q, Message msg);
bool dequeue_msg(ServerInputQueue *q, Message *msg);

void read_keyboard_input(ClientInputQueue *queue, const _Atomic bool *running);
inline void recv_msg(ServerInputQueue *q, Message *msg) {}; // receive message from server socket

#endif //SERPENT_INPUT_H