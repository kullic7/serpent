#ifndef SERPENT_GAME_H
#define SERPENT_GAME_H
#include <stdbool.h>
#include "types.h"
#include "events.h"
#include "registry.h"
#include "physics.h"

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

void game_run(GameState *game, bool timed_mode, bool single_player, bool easy_mode,
    EventQueue *eq, ActionQueue *aq, ClientRegistry *reg);

void game_init(GameState *game, int width, int height, int game_time, bool obstacles_enabled,
    bool random_world, const char *file_path);
void game_destroy(const GameState *game);
void game_update(GameState *game, bool easy_mode, ActionQueue *aq);

void game_broadcast_snapshot(const GameState *game, ActionQueue *aq);

void game_add_player(GameState *game, int player_id);
void game_grow_player(GameState *game, int player_id);
void game_remove_player(GameState *game, int player_id);
void game_update_player_direction(const GameState *game, int player_id, Direction dir);

void game_pause_player(const GameState *game, int player_id);
void game_schedule_resume_player(const GameState *game, int player_id);
void game_resume_player(const GameState *game, int player_id);

void game_add_fruit(GameState *game);
static void game_remove_fruit(GameState *game, size_t index);
void game_spawn_obstacles_random(GameState *game);
void game_spawn_obstacles_from_file(GameState *game, const char *file_path);

// broadcasting
int broadcast_game_over(ClientRegistry *reg);
int broadcast_error(ClientRegistry *reg, const char *error_msg);

#endif //SERPENT_GAME_H