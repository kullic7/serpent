#ifndef SERPENT_INPUT_H
#define SERPENT_INPUT_H

#include "client.h"

void read_input(ClientInputQueue *queue, const _Atomic bool *running);

void handle_menu_key(Menu *menu, Key key);
void handle_game_key(Key key, ClientContext *ctx); // should map key to direction and send to server

void enqueue_input(ClientInputQueue *queue, Key key);
bool dequeue_input(ClientInputQueue *queue, Key *key);

#endif //SERPENT_INPUT_H