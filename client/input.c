#include "input.h"
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>


void read_keyboard_input(ClientInputQueue *queue, const _Atomic bool *running) {
    struct termios old_tio, new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    while (*running) {
        const int ch = getchar();
        if (ch == EOF) break;
        enqueue_key(queue, (Key)ch); // enqueue the key by value
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
}

void client_input_queue_init(ClientInputQueue *q) {
    q->count = 0;
    // events[] is left with indeterminate contents; that's fine as long as
    // you only read the first `count` entries.
    int rc = pthread_mutex_init(&q->lock, NULL);
    if (rc != 0) {
        // handle error
    }
}

void client_input_queue_destroy(ClientInputQueue *q) {
    pthread_mutex_destroy(&q->lock);
}

void server_input_queue_init(ServerInputQueue *q) {
    q->count = 0;
    int rc = pthread_mutex_init(&q->lock, NULL);
    if (rc != 0) {
        // handle error
    }
    rc = pthread_cond_init(&q->not_full, NULL);
    if (rc != 0) {
        // handle error
    }
}

void server_input_queue_destroy(ServerInputQueue *q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_full);
}

void enqueue_key(ClientInputQueue *q, Key key) {
    if (!((key >= 32 && key <= 126) || key == '\n' || key == '\r' || key == '\t' || key == 27)) {
        return; /* ignore unsupported keys */
    }

    pthread_mutex_lock(&q->lock);
    if (q->count < sizeof(q->events) / sizeof(q->events[0])) {
        q->events[q->count++] = key;
    }
    pthread_mutex_unlock(&q->lock);
}

bool dequeue_key(ClientInputQueue *q, Key *key) {
    pthread_mutex_lock(&q->lock);
    if (q->count == 0) {
        pthread_mutex_unlock(&q->lock);
        return false;
    }
    *key = q->events[0];

    // shift remaining events
    for (size_t i = 1; i < q->count; ++i) {
        q->events[i - 1] = q->events[i];
    }
    q->count--;
    pthread_mutex_unlock(&q->lock);
    return true;
}

void client_input_queue_flush(ClientInputQueue *q) {
    pthread_mutex_lock(&q->lock);
    q->count = 0;
    pthread_mutex_unlock(&q->lock);
}


void enqueue_msg(ServerInputQueue *q, Message msg) {
    pthread_mutex_lock(&q->lock);

    while (q->count >= sizeof(q->events) / sizeof(q->events[0])) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }

    q->events[q->count++] = msg;

    pthread_mutex_unlock(&q->lock);
}

bool dequeue_msg(ServerInputQueue *q, Message *msg) {
    pthread_mutex_lock(&q->lock);
    if (q->count == 0) {
        pthread_mutex_unlock(&q->lock);
        return false;
    }

    *msg = q->events[0];

    for (size_t i = 1; i < q->count; ++i) {
        q->events[i - 1] = q->events[i];
    }
    q->count--;

    pthread_cond_signal(&q->not_full);

    pthread_mutex_unlock(&q->lock);

    return true;
}


