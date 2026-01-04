#ifndef SERPENT_GAME_H
#define SERPENT_GAME_H
#include <stdbool.h>
#include "game_types.h"

typedef struct {
    Player *players;
    size_t player_count;

    Fruit *fruits;
    size_t fruit_count;

    Obstacle *obstacles;
    size_t obstacle_count;

    int width;
    int height;

    Timer timer;

} GameState;

void game_init(GameState *game, int width, int height, int game_time, bool obstacles_enabled,
    bool random_world, const char *file_path);
void game_destroy(const GameState *game);

void game_add_player(GameState *game, int player_id);
void game_remove_player(GameState *game, int player_id);

void game_update(GameState *game);
void game_spawn_fruit(GameState *game);
void game_spawn_obstacles(GameState *game, const char *file_path);

GameRenderState game_snapshot(const GameState *game);


#endif //SERPENT_GAME_H