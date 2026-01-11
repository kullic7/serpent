#ifndef SERPENT_GAME_TYPES_H
#define SERPENT_GAME_TYPES_H

#include <stdbool.h>
#include "config.h"
#include "timer.h"

typedef struct {
    int x;
    int y;
} Position;

typedef struct {
    const char text[MENU_MAX_TEXT_LENGTH];
} TextField;

typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

typedef struct {
    Position pos;
    bool active;
} Fruit;

typedef struct {
    Position pos;
} Obstacle;


// snapshot types for network and client rendering
typedef struct {
    Position *body;
    size_t length;
} SnakeSnapshot;

typedef struct {
    int width;
    int height;

    size_t score; // for particular player
    int player_time_elapsed; // for particular player in seconds (time since joining game) -> show in game over menu

    int game_time_remaining; // in seconds, global remaining time for timed mode, -1 means no limit

    size_t snake_count;
    SnakeSnapshot *snakes;

    size_t fruit_count;
    Fruit *fruits;

    size_t obstacle_count;
    Obstacle *obstacles;
} ClientGameStateSnapshot;

void snapshot_init(ClientGameStateSnapshot *st);
void snapshot_destroy(ClientGameStateSnapshot *st);

#endif //SERPENT_GAME_TYPES_H