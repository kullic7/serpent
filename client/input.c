#include "input.h"
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

/**
 * Reads keyboard input from the terminal and enqueues pressed keys.
 *
 * Switches the terminal to non-canonical, no-echo mode to receive
 * key presses immediately without waiting for ENTER.
 * Runs in a loop until the running flag becomes false.
 * Restores the original terminal settings before returning.
 *
 * @param queue Thread-safe queue used to store input keys.
 * @param running Atomic flag controlling the lifetime of the input loop.
 */
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
    const int rc = pthread_mutex_init(&q->lock, NULL);
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

/**
 * Enqueues a supported key into the client input queue.
 *
 * Accepts printable ASCII characters and selected control keys
 * (ENTER, TAB, ESC). Unsupported keys are ignored.
 * The queue is protected by a mutex and keys are dropped if the
 * queue is full.
 *
 * @param q   Pointer to the client input queue.
 * @param key Key value to enqueue.
 */
void enqueue_key(ClientInputQueue *q, const Key key) {
    if (!((key >= 32 && key <= 126) || key == '\n' || key == '\r' || key == '\t' || key == 27)) {
        return; /* ignore unsupported keys */
    }

    pthread_mutex_lock(&q->lock);
    if (q->count < sizeof(q->events) / sizeof(q->events[0])) {
        q->events[q->count++] = key;
    }
    pthread_mutex_unlock(&q->lock);
}

/**
 * Dequeues the oldest key from the client input queue.
 *
 * Retrieves and removes the first key in FIFO order.
 * The operation is non-blocking and returns false if the queue is empty.
 * Access to the queue is protected by a mutex.
 *
 * @param q   Pointer to the client input queue.
 * @param key Output parameter receiving the dequeued key.
 * @return true if a key was dequeued, false if the queue was empty.
 */
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

/**
 * Enqueues a message into the server input queue.
 *
 * This function is thread-safe. If the queue has reached its
 * maximum capacity, the calling thread blocks until space
 * becomes available.
 *
 * The function acquires the queue mutex before modifying the
 * internal buffer and uses a condition variable to wait when
 * the queue is full.
 *
 * @param q   Pointer to the server input queue
 * @param msg Message to be enqueued
 */
void enqueue_msg(ServerInputQueue *q, Message msg) {
    pthread_mutex_lock(&q->lock);

    while (q->count >= sizeof(q->events) / sizeof(q->events[0])) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }

    q->events[q->count++] = msg;

    pthread_mutex_unlock(&q->lock);
}

/**
 * Dequeues a message from the server input queue.
 *
 * This function is thread-safe and non-blocking. If the queue
 * is empty, it immediately returns false.
 *
 * On success, the oldest message (FIFO order) is copied into
 * the provided output parameter and removed from the queue.
 * If the queue was previously full, one waiting producer
 * thread is notified via the not_full condition variable.
 *
 * @param q   Pointer to the server input queue
 * @param msg Output parameter for the dequeued message
 * @return true if a message was dequeued, false if the queue was empty
 */
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


