#include "physics.h"
#include <stdlib.h>

bool player_player_collision(const Player *player, const Player *players, const size_t num_players) {
    for (size_t i = 0; i < num_players; ++i) {
        for (size_t j = 0; j < players[i].snake.length; ++j) {
            if (player->id == players[i].id && j == 0) {
                continue; // skip own head
            }
            if (player->snake.body[0].x == players[i].snake.body[j].x &&
                player->snake.body[0].y == players[i].snake.body[j].y) {
                return true;
            }
        }
    }
    return false;
}

bool player_obstacle_collision(const Player *player, const Obstacle *obstacles, const size_t num_obstacles) {
    for (size_t i = 0; i < num_obstacles; ++i) {
        if (player->snake.body[0].x == obstacles[i].pos.x &&
            player->snake.body[0].y == obstacles[i].pos.y) {
            return true;
        }
    }
    return false;
}

int player_fruit_collision(const Player *player, const Fruit *fruits, const size_t num_fruits) {
    for (size_t i = 0; i < num_fruits; ++i) {
        if (fruits[i].active &&
            player->snake.body[0].x == fruits[i].pos.x &&
            player->snake.body[0].y == fruits[i].pos.y) {
            return (int)i;
        }
    }
    return -1;
}

bool player_wall_collision(const Player *player, const int width, const int height) {
    if (player->snake.body[0].x < 0 || player->snake.body[0].x >= width ||
        player->snake.body[0].y < 0 || player->snake.body[0].y >= height) {
        return true;
    }
    return false;
}

void move_player(Player *player) {
    // Update direction
    player->snake.direction = player->snake.next_direction;

    // move snake head
    Position new_head = player->snake.body[0];
    switch (player->snake.direction) {
        case DIR_UP:    new_head.y -= 1; break;
        case DIR_DOWN:  new_head.y += 1; break;
        case DIR_LEFT:  new_head.x -= 1; break;
        case DIR_RIGHT: new_head.x += 1; break;
    }

    // shift body segments to follow the previous segment
    for (size_t seg = player->snake.length - 1; seg > 0; --seg) {
        player->snake.body[seg] = player->snake.body[seg - 1];
    }

    player->snake.body[0] = new_head;

}

void grow_player(Player *player) {
        const size_t new_length = player->snake.length + 1;
        Position *new_body = realloc(player->snake.body, new_length * sizeof(Position));
        if (new_body == NULL) {
            // handle allocation failure (log, disconnect player, etc.)
            return;
        }

        player->snake.body = new_body;

        // initialize the new tail segment to the previous tail position
        player->snake.body[new_length - 1] = player->snake.body[new_length - 2];

        player->snake.length = new_length;
}
