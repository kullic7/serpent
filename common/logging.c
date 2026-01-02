#include <stdio.h>
#include <time.h>
#include "logging.h"


void log_message(const char *message) {
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char timestamp[32];
    size_t written = 0;
    if (timeinfo) {
        written = strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    }
    if (written == 0) {
        snprintf(timestamp, sizeof(timestamp), "unknown time");
    }

    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        fprintf(log, "[%s] %s\n", timestamp, message);
        fclose(log);
    }
}