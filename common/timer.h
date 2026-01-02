#ifndef SERPENT_TIMER_H
#define SERPENT_TIMER_H

#include <time.h>


void sleep_frame(long sleep_time_ms);

typedef struct {
    struct timespec start_time;
    double duration_sec; /* how long the timer should run */
} Timer;

/* Initialize timer with a given duration in seconds */
void timer_set(Timer *t, double duration_sec);

/* Start (or restart) the timer from now */
void timer_start(Timer *t);

/* Reset timer to zero duration and clear start time */
void timer_reset(Timer *t);

/* How many seconds have elapsed since start */
double timer_elapsed(const Timer *t);

/* Has the timer reached its duration? */
int timer_expired(const Timer *t);

#endif //SERPENT_TIMER_H