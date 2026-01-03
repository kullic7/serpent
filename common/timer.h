#ifndef SERPENT_TIMER_H
#define SERPENT_TIMER_H

#include <time.h>
#include <stdbool.h>


void sleep_frame(long sleep_time_ms);

typedef struct {
    struct timespec start_time;
    double duration_sec;
} Timer;

void timer_set(Timer *t, double duration_sec);

void timer_start(Timer *t);

void timer_reset(Timer *t);

double timer_elapsed(const Timer *t);

bool timer_expired(const Timer *t);

#endif //SERPENT_TIMER_H