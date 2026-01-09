#define _POSIX_C_SOURCE 199309L
#include "timer.h"
#include <time.h>

#include "logging.h"

void sleepn(const long nanoseconds) {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = nanoseconds;
    nanosleep(&ts, NULL);
}

void sleep_frame(const long sleep_time_ms) {
    static struct timespec last_frame = {0};
    struct timespec current_time, sleep_time;
    long elapsed_ms, sleep_ms;

    clock_gettime(CLOCK_MONOTONIC, &current_time);

    if (last_frame.tv_sec == 0 && last_frame.tv_nsec == 0) {
        last_frame = current_time;
        return;
    }

    elapsed_ms = (current_time.tv_sec - last_frame.tv_sec) * 1000 +
                 (current_time.tv_nsec - last_frame.tv_nsec) / 1000000;

    sleep_ms = sleep_time_ms - elapsed_ms;

    if (sleep_ms > 0) {
        sleep_time.tv_sec = sleep_ms / 1000;
        sleep_time.tv_nsec = (sleep_ms % 1000) * 1000000;
        nanosleep(&sleep_time, NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &last_frame);
}

static double timespec_diff_sec(const struct timespec *start,
                                const struct timespec *end) {
    time_t sec = end->tv_sec - start->tv_sec;
    long nsec = end->tv_nsec - start->tv_nsec;
    return (double)sec + (double)nsec / 1e9;
}

void timer_set(Timer *t, const double duration_sec) {
    t->duration_sec = duration_sec;
}

void timer_start(Timer *t) {
    clock_gettime(CLOCK_MONOTONIC, &t->start_time);
}

void timer_reset(Timer *t) {
    t->duration_sec = 0.0;
    t->start_time.tv_sec = 0;
    t->start_time.tv_nsec = 0;
}

double timer_remaining(const Timer *t) {
    if (t->duration_sec <= 0.0) return 0.0;
    const double elapsed = timer_elapsed(t);
    const double remaining = t->duration_sec - elapsed;
    return remaining > 0.0 ? remaining : 0.0;
}

double timer_elapsed(const Timer *t) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return timespec_diff_sec(&t->start_time, &now);
}

bool timer_expired(const Timer *t) {
    if (t->duration_sec <= 0.0) return true;
    return timer_elapsed(t) >= t->duration_sec;
}