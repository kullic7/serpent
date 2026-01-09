#ifndef SERPENT_GAME_H
#define SERPENT_GAME_H
#include <stdbool.h>
#include "types.h"
#include "events.h"
#include "registry.h"

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
    bool wait_for_end_pending;

} GameState;

void game_init(GameState *game, int width, int height, int game_time, bool obstacles_enabled,
    bool random_world, const char *file_path);
void game_destroy(const GameState *game);

void game_snapshot_each_client(const GameState *game, ActionQueue *aq);
int broadcast_game_state(ClientRegistry *reg, ActArgGameState game);
int broadcast_game_over(ClientRegistry *reg);

void game_add_player(GameState *game, int player_id);
void game_remove_player(GameState *game, int player_id);
static void game_remove_fruit(GameState *game, size_t index);

void game_update_player_direction(const GameState *game, int player_id, Direction dir);
void game_pause_player(const GameState *game, int player_id);
void game_schedule_resume_player(const GameState *game, int player_id);
void game_resume_player(const GameState *game, int player_id);

void game_update(GameState *game);
void game_add_fruit(GameState *game);
void game_spawn_obstacles(GameState *game, const char *file_path);

#endif //SERPENT_GAME_H