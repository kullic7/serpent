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

int snapshot_deep_copy(ClientGameStateSnapshot *dst,
                       const ClientGameStateSnapshot *src) {
    *dst = *src; // copy scalars
    dst->snakes = NULL;
    dst->fruits = NULL;
    dst->obstacles = NULL;

    if (src->snake_count) {
        dst->snakes = malloc(src->snake_count * sizeof(SnakeSnapshot));
        if (!dst->snakes) goto fail;
        for (size_t i = 0; i < src->snake_count; ++i) {
            dst->snakes[i].length = src->snakes[i].length;
            dst->snakes[i].body = NULL;
            if (src->snakes[i].length) {
                dst->snakes[i].body =
                    malloc(src->snakes[i].length * sizeof(Position));
                if (!dst->snakes[i].body) goto fail;
                memcpy(dst->snakes[i].body,
                       src->snakes[i].body,
                       src->snakes[i].length * sizeof(Position));
            }
        }
    }

    if (src->fruit_count) {
        dst->fruits = malloc(src->fruit_count * sizeof(Fruit));
        if (!dst->fruits) goto fail;
        memcpy(dst->fruits, src->fruits,
               src->fruit_count * sizeof(Fruit));
    }

    if (src->obstacle_count) {
        dst->obstacles = malloc(src->obstacle_count * sizeof(Obstacle));
        if (!dst->obstacles) goto fail;
        memcpy(dst->obstacles, src->obstacles,
               src->obstacle_count * sizeof(Obstacle));
    }

    return 0;
    fail:
        snapshot_destroy(dst);
    return -1;
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