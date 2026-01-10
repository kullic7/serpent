#include "types.h"
#include <string.h>
#include <stdlib.h>

void snapshot_init(ClientGameStateSnapshot *st) {
    st->snakes = NULL;
    st->snake_count = 0;
    st->fruits = NULL;
    st->fruit_count = 0;
    st->obstacles = NULL;
    st->obstacle_count = 0;
}

void snapshot_destroy(ClientGameStateSnapshot *st) {
    if (st->snakes) {
        for (size_t i = 0; i < st->snake_count; ++i) {
            free(st->snakes[i].body);
        }
        free(st->snakes);
    }
    free(st->fruits);
    free(st->obstacles);
    snapshot_init(st);
}