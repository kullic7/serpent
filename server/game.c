#include <stdlib.h>
#include "config.h"
#include "game.h"
#include "server.h"
#include "logging.h"

void game_init(GameState *game, const int width, const int height, const int game_time,
               const bool obstacles_enabled, const bool random_world, const char *file_path) {
    game->width = width;
    game->height = height;

    game->wait_for_end_pending = false;

    game->players = NULL;
    game->player_count = 0;

    game->fruits = NULL;
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
            game_spawn_obstacles_random(game);
        }
        else {
            game_spawn_obstacles_from_file(game, file_path);
        }
    }
}

void game_destroy(const GameState *game) {
    if (game->players != NULL) {
        for (size_t i = 0; i < game->player_count; ++i) {
            free(game->players[i].snake.body);
        }
    }
    free(game->players);
    free(game->fruits);
    free(game->obstacles); // free is noop on NULL so its ok
}

void game_run(GameState *game, const bool timed_mode, const bool single_player, const bool easy_mode,
    EventQueue *eq, ActionQueue *aq, ClientRegistry *reg) {

    // timeout loop - waiting for at least one player to connect
    Timer timeout_timer;
    timer_reset(&timeout_timer);
    timer_set(&timeout_timer, 10); // 10 sec timeout for connection to avoid race condition with recv thread
    timer_start(&timeout_timer);
    while (true) {
        // handle events (only EVENT_CONNECTED is relevant)
        Event ev = {0};
        while (dequeue_event(eq, &ev)) {
            handle_event(&ev, aq, game);
        }

        if (game->player_count > 0) break; // at least one player connected
        if (timer_expired(&timeout_timer)) {
            log_server("timeout waiting for first player connection\n");

            broadcast_error(reg, "Timeout waiting for first player connection\n");
            return; // timeout waiting for first player
        }
    }

    // game loop
    timer_start(&game->timer);
    while (true) {

        game_broadcast_snapshot(game, aq);

        sleep_frame(GAME_TICK_TIME_MS);

        game_update(game, easy_mode, aq);

        // handle events
        Event ev = {0};
        bool end_game = false;
        while (dequeue_event(eq, &ev)) {
            end_game = handle_event(&ev, aq, game);
        }
        if (end_game) break;

        end_game = handle_end_event(timed_mode, single_player, game, aq);

        if (end_game) break;

    }

    broadcast_game_over(reg); // must be done here before shutdown so we are sure all clients get it (its blocking)
    log_server("game over broadcasted to clients\n");
}

void game_broadcast_snapshot(const GameState *game, ActionQueue *aq) {

    // for each client
    for (size_t i = 0; i < game->player_count; ++i) {
        Action act;
        act.type = ACT_SEND_GAME_STATE;

        ClientGameStateSnapshot *snapshot = malloc(sizeof(*snapshot));
        if (!snapshot) {
            // TODO handle OOM as needed
            continue;
        }

        snapshot->width = game->width;
        snapshot->height = game->height;

        snapshot->score = game->players[i].score;
        snapshot->player_time_elapsed = (int)timer_elapsed(&game->players[i].timer);
        snapshot->game_time_remaining = (int)timer_remaining(&game->timer);

        snapshot->snake_count = game->player_count;
        snapshot->snakes = malloc(snapshot->snake_count * sizeof(SnakeSnapshot));
        for (size_t j = 0; j < game->player_count; ++j) {
            const Player *p = &game->players[j];
            SnakeSnapshot *ss = &snapshot->snakes[j];
            ss->length = p->snake.length;
            ss->body = malloc(ss->length * sizeof(Position));
            for (size_t k = 0; k < ss->length; ++k) {
                ss->body[k] = p->snake.body[k];
            }
        }

        snapshot->fruit_count = game->fruit_count;
        snapshot->fruits = malloc(snapshot->fruit_count * sizeof(Fruit));
        for (size_t j = 0; j < game->fruit_count; ++j) {
            snapshot->fruits[j] = game->fruits[j];
        }

        snapshot->obstacle_count = game->obstacle_count;
        snapshot->obstacles = malloc(snapshot->obstacle_count * sizeof(Obstacle));
        for (size_t j = 0; j < game->obstacle_count; ++j) {
            snapshot->obstacles[j] = game->obstacles[j];
        }

        if (!snapshot->snakes || !snapshot->fruits || !snapshot->obstacles) {
            snapshot_destroy(snapshot);
            free(snapshot);
            continue;
        }

        act.u.game.state = snapshot;      // pointer, no copy
        act.u.game.player_id = game->players[i].id;

        enqueue_action(aq, act);

    }
}

int broadcast_game_over(ClientRegistry *reg) {

    int rc = 0;
    pthread_mutex_lock(&reg->lock);
    for (size_t i = 0; i < reg->count; ++i) {
        if (send_game_over(reg->clients[i]->socket_fd) < 0) {
            log_server("FAILED: to send game over to client\n");
            rc = -1;
            break;
        }
    }
    pthread_mutex_unlock(&reg->lock);

    return rc;
}

int broadcast_error(ClientRegistry *reg, const char *error_msg) {

    int rc = 0;
    pthread_mutex_lock(&reg->lock);
    for (size_t i = 0; i < reg->count; ++i) {
        if (send_error(reg->clients[i]->socket_fd, error_msg) < 0) {
            log_server("FAILED: to send error message to client\n");
            rc = -1;
            break;
        }
    }
    pthread_mutex_unlock(&reg->lock);

    return rc;
}

void game_add_player(GameState *game, const int player_id) {

    Player *new_players = realloc(game->players, (game->player_count + 1) * sizeof(Player));
    if (new_players == NULL) {
        // allocation failed, keep old `game->players` intact
        // optionally log or handle error here
        return;
    }
    game->players = new_players;

    Player *p = &game->players[game->player_count];

    p->id = player_id;
    p->score = 0;

    p->paused = false;
    p->resume_ev_pending = false;

    timer_reset(&p->timer);
    timer_set(&p->timer, 0); // start at 0

    // its snake
    p->snake.body = malloc(INITIAL_SNAKE_LENGTH * sizeof(Position));
    p->snake.length = INITIAL_SNAKE_LENGTH;
    p->snake.capacity = INITIAL_SNAKE_LENGTH;
    p->snake.direction = DIR_RIGHT;
    p->snake.next_direction = DIR_RIGHT;

    // initialize snake position: horizontally, centered, and away from walls/obstacles
    bool valid_snake_pos = false;
    while (!valid_snake_pos) {
        // choose head x so that the whole snake fits within \[0, width)
        const int max_head_x = game->width - 1;
        int min_head_x = INITIAL_SNAKE_LENGTH - 1;
        if (max_head_x <= min_head_x) {
            // fallback: clamp to valid range
            min_head_x = 0;
        }

        const int head_x = rand() % (max_head_x - min_head_x + 1) + min_head_x;
        const int head_y = rand() % game->height;

        valid_snake_pos = true;

        // check each segment for obstacle collision
        for (size_t i = 0; i < INITIAL_SNAKE_LENGTH && valid_snake_pos; ++i) {
            int seg_x = head_x - (int)i;
            int seg_y = head_y;

            // defensive bounds check
            if (seg_x < 0 || seg_x >= game->width || seg_y < 0 || seg_y >= game->height) {
                valid_snake_pos = false;
                break;
            }

            for (size_t j = 0; j < game->obstacle_count; ++j) {
                if (game->obstacles[j].pos.x == seg_x &&
                    game->obstacles[j].pos.y == seg_y) {
                    valid_snake_pos = false;
                    break;
                    }
            }
        }

        if (valid_snake_pos) {
            for (size_t i = 0; i < INITIAL_SNAKE_LENGTH; ++i) {
                p->snake.body[i].x = head_x - (int)i;
                p->snake.body[i].y = head_y;
            }
        }
    }

    game->player_count++;

    log_server("Player added to game\n");

}

void game_remove_player(GameState *game, const int player_id) {

    // find player index
    size_t idx = (size_t)-1;
    for (size_t i = 0; i < game->player_count; ++i) {
        if (game->players[i].id == player_id) {
            idx = i;
            break;
        }
    }

    if (idx != (size_t)-1) {
        free(game->players[idx].snake.body);

        for (size_t i = idx + 1; i < game->player_count; ++i) {
            game->players[i - 1] = game->players[i];
        }

        game->player_count--;

        if (game->player_count == 0) {
            free(game->players);
            game->players = NULL;
        } else {
            Player *tmp = realloc(game->players, game->player_count * sizeof(Player));
            if (tmp != NULL) {
                game->players = tmp;
            }
        }
    }

}

void game_update_player_direction(const GameState *game, const int player_id, const Direction dir) {
    for (size_t i = 0; i < game->player_count; ++i) {
        if (game->players[i].id == player_id) {
            // prevent reversing direction
            const Direction current_dir = game->players[i].snake.direction;
            if ((current_dir == DIR_UP && dir != DIR_DOWN) ||
                (current_dir == DIR_DOWN && dir != DIR_UP) ||
                (current_dir == DIR_LEFT && dir != DIR_RIGHT) ||
                (current_dir == DIR_RIGHT && dir != DIR_LEFT)) {
                game->players[i].snake.next_direction = dir;
            }
            break;
        }
    }
}

void game_pause_player(const GameState *game, const int player_id) {
    for (size_t i = 0; i < game->player_count; ++i) {
        if (game->players[i].id == player_id) {
            game->players[i].paused = true;
            game->players[i].resume_ev_pending = true;
            break;
        }
    }
}

void game_schedule_resume_player(const GameState *game, const int player_id) {
    for (size_t i = 0; i < game->player_count; ++i) {
        if (game->players[i].id == player_id) {
            game->players[i].resume_ev_pending = false;
            break;
        }
    }
}

void game_resume_player(const GameState *game, const int player_id) {
    for (size_t i = 0; i < game->player_count; ++i) {
        if (game->players[i].id == player_id && !game->players[i].resume_ev_pending) {
            game->players[i].paused = false;
            break;
        }
    }
}

void game_update(GameState *game, const bool easy_mode, ActionQueue *aq) {
    // update each player's snake position
    for (size_t i = 0; i < game->player_count; ++i) {
        Player *p = &game->players[i];
        if (p->paused) continue;

        move_player(p);

        // check collisions
        if (player_player_collision(p, game->players, game->player_count) ||
            player_obstacle_collision(p, game->obstacles, game->obstacle_count)) {
            // ACT send game over to p->player_id
            enqueue_action(aq, (Action){ .type = ACT_SEND_GAME_OVER, .u.player_id = p->id });
            // remove player
            game_remove_player(game, p->id);
            continue; // skip further checks for this player
        }

        if (player_wall_collision(p, game->width, game->height)) {
            if (!easy_mode) {
                // ACT send game over to p->player_id
                enqueue_action(aq, (Action){ .type = ACT_SEND_GAME_OVER, .u.player_id = p->id });
                // remove player
                game_remove_player(game, p->id);
            } else {
                // wrap around (easy mode)
                Position *head = &p->snake.body[0];
                if (head->x < 0) {
                    head->x = game->width - 1;
                } else if (head->x >= game->width) {
                    head->x = 0;
                }
                if (head->y < 0) {
                    head->y = game->height - 1;
                } else if (head->y >= game->height) {
                    head->y = 0;
                }
            }
            continue; // skip further checks for this player
        }

        const int collided_fruit_idx = player_fruit_collision(p, game->fruits, game->fruit_count);
        if (collided_fruit_idx >= 0) {
            // eat and deactivate fruit
            Fruit *f = &game->fruits[collided_fruit_idx]; f->active = false; p->score += 1;

            // grow player
            grow_player(p);

            // add new fruit
            game_add_fruit(game);
        }
    }

    // remove inactive fruits
    for (size_t i = game->fruit_count; i-- > 0; ) {
        if (!game->fruits[i].active) {
            game_remove_fruit(game, i);
        }
    }
}

void game_add_fruit(GameState *game) {
    Fruit *new_fruits = realloc(game->fruits, (game->fruit_count + 1) * sizeof(Fruit));
    if (new_fruits == NULL) {
        // allocation failed, do not modify existing fruits
        return;
    }

    game->fruits = new_fruits;

    Fruit f = {0};

    // find valid position
    bool valid_pos = false;
    while (!valid_pos) {
        f.pos.x = 1 + rand() % (game->width  - 2);
        f.pos.y = 1 + rand() % (game->height - 2);
        f.active = true;

        valid_pos = true;

        // check collision with players
        for (size_t i = 0; i < game->player_count; ++i) {
            const Player *p = &game->players[i];

            if (player_fruit_collision(p, &f, 1) >= 0) {
                valid_pos = false;
                break;
            }
        }
        if (!valid_pos) continue;

        // check collision with obstacles
        for (size_t i = 0; i < game->obstacle_count; ++i) {
            if (game->obstacles[i].pos.x == f.pos.x &&
                game->obstacles[i].pos.y == f.pos.y) {
                valid_pos = false;
                break;
                }
        }
    }

    game->fruits[game->fruit_count++] = f;

    log_server("Fruit added to game\n");
}

static void game_remove_fruit(GameState *game, const size_t index) {
    if (index >= game->fruit_count) return;

    for (size_t i = index + 1; i < game->fruit_count; ++i) {
        game->fruits[i - 1] = game->fruits[i];
    }
    game->fruit_count--;
}

void game_spawn_obstacles_from_file(GameState *game, const char *file_path) {
    // for simplicity, let's assume obstacles are placed randomly
    // TODO we should impl from file interpretation
    //  for now we just call random
    game_spawn_obstacles_random(game);
}


void game_spawn_obstacles_random(GameState *game) {

    // choose num of obstacles at random based on board size
    const size_t max_possible = (size_t)(game->width * game->height / 120);
    const size_t max_obstacles = rand() % (max_possible > 0 ? max_possible : 1) + 1;

    game->obstacles = malloc(max_obstacles * sizeof(Obstacle));
    game->obstacle_count = 0;

    // easy algorithm: try to place obstacles randomly, not allowing any neighbours
    for (size_t i = 0; i < max_obstacles; ++i) {
        const int max_tries = 1000;
        bool placed = false;

        for (int t = 0; t < max_tries && !placed; ++t) {
            Obstacle o;
            o.pos.x = rand() % game->width;
            o.pos.y = rand() % game->height;

            // check 8 neighbors \+ self for any existing obstacle
            bool has_neighbor = false;
            for (size_t j = 0; j < game->obstacle_count && !has_neighbor; ++j) {
                const int dx = abs(game->obstacles[j].pos.x - o.pos.x);
                const int dy = abs(game->obstacles[j].pos.y - o.pos.y);
                if (dx <= 1 && dy <= 1) {
                    has_neighbor = true;
                }
            }

            if (!has_neighbor) {
                game->obstacles[game->obstacle_count++] = o;
                placed = true;
            }
        }

        // if we cannot place more without neighbors, stop early
        if (!placed) {
            break;
        }
    }
}
