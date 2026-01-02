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
    int width;
    int height;
} Size;

typedef struct {
    Size size;
    Position pos;
} Box;

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
    Position *body; // dynamic array of positions
    size_t length;
} RenderSnake;


typedef struct {
    Position *body; // dynamic array of positions
    size_t length;
    size_t capacity;
    Direction direction;     // UP, DOWN, LEFT, RIGHT
    Direction next_direction;
    bool alive; // maybe it should be active instead
} Snake;

typedef struct {
    int id;
    Snake snake;
    size_t score;
    bool paused;
    Timer timer;
} Player;

typedef struct {
    Position pos;
    bool active;
} Fruit;

typedef struct {
    Position pos;
} Obstacle;

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

#endif //SERPENT_GAME_TYPES_H