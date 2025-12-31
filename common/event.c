#include "event.h"

/*
void enqueue_wait_for_socket(ClientContext *ctx, ActionQueue *aq) {
    Action act = {0};
    act.type = ACT_WAIT_FOR_SOCKET;
    strncpy(act.u.socket.path, ctx->socket_path,
            sizeof(act.u.socket.path) - 1);
    enqueue_action(aq, &act);
}

void enqueue_send_input(Direction dir, ActionQueue *aq) {
    Action act = {0};
    act.type = ACT_SEND_INPUT;
    act.u.input.direction = dir;
    enqueue_action(aq, &act);
}
*/