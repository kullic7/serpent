#ifndef SERPENT_TYPES_H
#define SERPENT_TYPES_H

#include <pthread.h>
#include "common/config.h"
#include "common/game_types.h"

typedef struct {
    int width;
    int height;

    size_t score;
    int time_remaining; // in seconds, -1 means no limit

    size_t snake_count;
    RenderSnake *snakes;

    size_t fruit_count;
    Position *fruits;

    size_t obstacle_count;
    Obstacle *obstacles;
} GameRenderState;


#endif //SERPENT_TYPES_H