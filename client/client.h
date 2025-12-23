#ifndef SERPENT_CLIENT_H
#define SERPENT_CLIENT_H

#include "client/renderer.h"
#include "client/types.h"


void client_init(ClientContext *ctx);

void client_run(ClientContext *ctx, ClientInputQueue *input_queue);

void client_cleanup(ClientContext *ctx);

inline void recv_state(ClientContext *ctx) {};

inline void recv_msg(ClientContext *ctx) {};

inline void send_input(ClientContext *ctx, Key key) {};

inline void send_message(ClientContext *ctx, const void *data, size_t size) {};

#endif //SERPENT_CLIENT_H