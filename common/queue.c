#include "queue.h"

#include "queue.h"
#include <stdlib.h>

int tsqueue_init(TsQueue *q, size_t capacity) {
    q->items = (void **)malloc(sizeof(void *) * capacity);
    if (!q->items) return -1;

    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;

    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    return 0;
}

void tsqueue_destroy(TsQueue *q) {
    if (!q) return;
    free(q->items);
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

int tsqueue_push(TsQueue *q, void *item) {
    pthread_mutex_lock(&q->lock);
    while (q->count == q->capacity)
        pthread_cond_wait(&q->not_full, &q->lock);

    q->items[q->tail] = item;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

int tsqueue_pop(TsQueue *q, void **out_item) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0)
        pthread_cond_wait(&q->not_empty, &q->lock);

    *out_item = q->items[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

int tsqueue_try_push(TsQueue *q, void *item) {
    int ret = -1;
    pthread_mutex_lock(&q->lock);
    if (q->count < q->capacity) {
        q->items[q->tail] = item;
        q->tail = (q->tail + 1) % q->capacity;
        q->count++;
        pthread_cond_signal(&q->not_empty);
        ret = 0;
    }
    pthread_mutex_unlock(&q->lock);
    return ret;
}

int tsqueue_try_pop(TsQueue *q, void **out_item) {
    int ret = -1;
    pthread_mutex_lock(&q->lock);
    if (q->count > 0) {
        *out_item = q->items[q->head];
        q->head = (q->head + 1) % q->capacity;
        q->count--;
        pthread_cond_signal(&q->not_full);
        ret = 0;
    }
    pthread_mutex_unlock(&q->lock);
    return ret;
}