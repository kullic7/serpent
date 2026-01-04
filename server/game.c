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
    if (game->players != NULL) {
        for (size_t i = 0; i < game->player_count; ++i) {
            free(game->players[i].snake.body);
        }
    }
    free(game->players);
    free(game->fruits);
    free(game->obstacles); // free is noop on NULL so its ok
}

void game_snapshot_each_client(const GameState *game, ActionQueue *aq) {

    // for each client
    for (size_t i = 0; i < game->player_count; ++i) {
        Action act;
        act.type = ACT_BROADCAST_GAME_STATE;

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

        act.u.game.state = snapshot;      // pointer, no copy

        enqueue_action(aq, act);

    }
}

int broadcast_game_state(ClientRegistry *reg, const ActArgGameState game) {

    int rc = 0;
    pthread_mutex_lock(&reg->lock);
    for (size_t i = 0; i < reg->count; ++i) {
        if (send_state(reg->clients[i]->socket_fd, game.state) < 0) {
            log_server("failed to send game state to client\n");
            rc = -1;
            break;
        }
    }
    pthread_mutex_unlock(&reg->lock);

    return rc;
}

void game_add_player(GameState *game, const int player_id) {

    game->players = realloc(game->players, (game->player_count + 1) * sizeof(Player));
    // TODO handle realloc failure

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

    // initialize snake position
    for (size_t i = 0; i < INITIAL_SNAKE_LENGTH; ++i) {
        p->snake.body[i].x = 5 - i;
        p->snake.body[i].y = 5;
    }

    game->player_count++;

}

void game_remove_player(GameState *game, const int player_id) {

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
            // TODO: handle realloc failure if needed
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

void game_update(GameState *game) {
    // Update each player's snake position
    for (size_t i = 0; i < game->player_count; ++i) {
        Player *p = &game->players[i];
        if (p->paused) continue;

        // Update direction
        p->snake.direction = p->snake.next_direction;

        // move snake head
        Position new_head = p->snake.body[0];
        switch (p->snake.direction) {
            case DIR_UP:    new_head.y -= 1; break;
            case DIR_DOWN:  new_head.y += 1; break;
            case DIR_LEFT:  new_head.x -= 1; break;
            case DIR_RIGHT: new_head.x += 1; break;
        }

        // shift body segments to follow the previous segment
        for (size_t seg = p->snake.length - 1; seg > 0; --seg) {
            p->snake.body[seg] = p->snake.body[seg - 1];
        }

        p->snake.body[0] = new_head;

        // Check collisions with walls
        if (new_head.x < 0 || new_head.x >= game->width || new_head.y < 0 || new_head.y >= game->height) {
            // ACT send game over to p->player_id TODO
            // remove player
            // todo for now just wrap around (easy mode)
            if (new_head.x < 0) {
                new_head.x = game->width - 1;
            } else if (new_head.x >= game->width) {
                new_head.x = 0;
            }
            if (new_head.y < 0) {
                new_head.y = game->height - 1;
            } else if (new_head.y >= game->height) {
                new_head.y = 0;
            }
        }

        // check collisions with fruits

        /*
        for (size_t j = 0; j < game->fruit_count; ++j) {
            Fruit *f = &game->fruits[j];
            if (f->active && f->pos.x == new_head.x && f->pos.y == new_head.y) {
                // Eat fruit
                p->score += 10;
                f->active = false;
                // Grow snake
                p->snake.body = realloc(p->snake.body, (p->snake.length + 1) * sizeof(Position));
                p->snake.length += 1;
                break;
                // TODO remove fruit from game?
                // TODO spawn new fruit?

            }
        }*/
    }

    for (size_t i = game->fruit_count; i-- > 0; ) {
        if (!game->fruits[i].active) {
            game_remove_fruit(game, i);
        }
    }
}

void game_add_fruit(GameState *game) {
    if (game->fruits == NULL) {
        game->fruits = malloc(MAX_FRUITS * sizeof(Fruit));
        if (game->fruits == NULL) {
            // allocation failed, cannot add fruit
            return;
        }
        game->fruit_count = 0;
    }

    if (game->fruit_count >= MAX_FRUITS) return;

    Fruit f;
    f.pos.x = rand() % game->width;
    f.pos.y = rand() % game->height;
    f.active = true;

    game->fruits[game->fruit_count++] = f;
}

static void game_remove_fruit(GameState *game, const size_t index) {
    if (index >= game->fruit_count) return;

    for (size_t i = index + 1; i < game->fruit_count; ++i) {
        game->fruits[i - 1] = game->fruits[i];
    }
    game->fruit_count--;
}

void game_spawn_obstacles(GameState *game, const char *file_path) {
    // for simplicity, let's assume obstacles are placed randomly
    const size_t max_obstacles = 20;
    game->obstacles = malloc(max_obstacles * sizeof(Obstacle));
    game->obstacle_count = 0;

    for (size_t i = 0; i < max_obstacles; ++i) {
        Obstacle o;
        o.pos.x = rand() % game->width;
        o.pos.y = rand() % game->height;
        game->obstacles[game->obstacle_count++] = o;
    }
}

