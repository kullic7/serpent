#ifndef SERPENT_QUEUE_H
#define SERPENT_QUEUE_H

#include <pthread.h>
#include <stddef.h>

typedef struct {
    void **items;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} TsQueue;

/* Generic queue API */
int tsqueue_init(TsQueue *q, size_t capacity);
void tsqueue_destroy(TsQueue *q);

/* Blocks if queue is full */
int tsqueue_push(TsQueue *q, void *item);

/* Blocks if queue is empty */
int tsqueue_pop(TsQueue *q, void **out_item);

/* Non-blocking versions, return 0 on success, non-zero on empty/full */
int tsqueue_try_push(TsQueue *q, void *item);
int tsqueue_try_pop(TsQueue *q, void **out_item);


#endif //SERPENT_QUEUE_H