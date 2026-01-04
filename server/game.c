#include <stdlib.h>
#include "config.h"
#include "game.h"
#include "server.h"

void game_init(GameState *game, const int width, const int height, const int game_time,
               const bool obstacles_enabled, const bool random_world, const char *file_path) {
    game->width = width;
    game->height = height;

    game->players = NULL;
    game->player_count = 0;

    game->fruits = malloc(MAX_FRUITS * sizeof(Fruit));
    game->fruit_count = 0;

    game->obstacles = NULL;
    game->obstacle_count = 0;

    Timer timer;
    timer_reset(&timer);
    game->timer = timer;

    if (game_time >= 0) {
        timer_set(&game->timer, game_time);
    }

    if (obstacles_enabled) {

        if (random_world) {
            game_spawn_obstacles(game, file_path);
        }
        else { // TODO we skip loading from file for now
            game_spawn_obstacles(game, file_path);
        }
    }
}

void game_destroy(const GameState *game) {
    free(game->players);
    free(game->fruits);
    free(game->obstacles); // free is noop on NULL so its ok
}

void game_add_player(GameState *game, const int player_id) {

}

void game_remove_player(GameState *game, const int player_id) {

}

void game_update(GameState *game) {

}

void game_spawn_fruit(GameState *game) {
}

void game_spawn_obstacles(GameState *game, const char *file_path) {
}

