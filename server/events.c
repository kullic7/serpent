#include "events.h"
#include <pthread.h>

void event_queue_init(EventQueue *q) {
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

void event_queue_destroy(EventQueue *q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_full);
}

void enqueue_event(EventQueue *q, const Event ev) {
    pthread_mutex_lock(&q->lock);
    while (q->count >= MAX_EVENTS) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    q->events[q->count++] = ev;
    pthread_mutex_unlock(&q->lock);
}

bool dequeue_event(EventQueue *q, Event *ev) {
    pthread_mutex_lock(&q->lock);
    if (q->count == 0) {
        pthread_mutex_unlock(&q->lock);
        return false;
    }
    *ev = q->events[0];

    // shift remaining events
    for (size_t i = 1; i < q->count; ++i) {
        q->events[i - 1] = q->events[i];
    }
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return true;
}


void action_queue_init(ActionQueue *q) {
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

void action_queue_destroy(ActionQueue *q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_full);
}

void enqueue_action(ActionQueue *q, const Action act) {
    pthread_mutex_lock(&q->lock);
    while (q->count >= MAX_ACTIONS) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    q->actions[q->count++] = act;
    pthread_mutex_unlock(&q->lock);
}

bool dequeue_action(ActionQueue *q, Action *act) {
    pthread_mutex_lock(&q->lock);
    if (q->count == 0) {
        pthread_mutex_unlock(&q->lock);
        return false;
    }
    *act = q->actions[0];

    // shift remaining actions
    for (size_t i = 1; i < q->count; ++i) {
        q->actions[i - 1] = q->actions[i];
    }
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return true;
}