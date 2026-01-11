#ifndef SERPENT_PHYSICS_H
#define SERPENT_PHYSICS_H

#include "types.h"

typedef struct {
    Position *body; // dynamic array of positions
    size_t length;
    size_t capacity;
    Direction direction;     // UP, DOWN, LEFT, RIGHT
    Direction next_direction;
} Snake;

typedef struct {
    int id;
    Snake snake;
    size_t score;
    bool paused;
    bool resume_ev_pending;
    Timer timer;
} Player;

bool player_player_collision(const Player *player, const Player *players, size_t num_players);
bool player_obstacle_collision(const Player *player, const Obstacle *obstacles, size_t num_obstacles);
int player_fruit_collision(const Player *player, const Fruit *fruits, size_t num_fruits);
bool player_wall_collision(const Player *player, int width, int height);

void move_player(Player *player);
void grow_player(Player *player);

#endif //SERPENT_PHYSICS_H