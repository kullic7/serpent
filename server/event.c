#include "event.h"

void handle_event(const Event *ev, ActionQueue *q) {
    Action a;
    switch (ev->type) {
        case EV_CONNECTED:
            // add_player(&game_state)
            //ctx->game.players[ctx->game.player_count++] = ev->u.player_id;

            a.type = ACT_SEND_GAME_OVER;
            a.u.player_id = ev->u.player_id;
            enqueue_action(q, a);
            break;
        case EV_LOADED:
            // world loaded, can start game
            //ctx->world_loaded = true;
            break;
        case EV_PAUSED:
            // client paused game
            // game_state handle it ev->u.player_id
            break;
        case EV_RESUMED:
            // resume game
            // game_state handle it ev->u.player_id
            break;
        case EV_DISCONNECTED:
            // remove player from game state ev->u.player_id
            a.type = ACT_UNREGISTER_PLAYER;
            a.u.player_id = ev->u.player_id;
            enqueue_action(q, a);
            break;
        case EV_ERROR:
            // handle error by sending error msg to player ev->u.player_id
            a.type = ACT_SEND_ERROR;
            a.u.player_id = ev->u.player_id;
            enqueue_action(q, a);
            break;

        default:
            break;
    }
}

void exec_action(const Action *act, EventQueue *q) {
    Event ev;
    switch (act->type) {
        case ACT_LOAD_WORLD:
            // load_world(); // blocking
            ev.type = EV_LOADED;
            ev.u.nodata = 0;
            enqueue_event(q, ev);
            break;
        case ACT_SEND_READY:
            // send ready message to client act->u.player_id
            //send_msg(act->u.player_id, MSG_READY, NULL, 0); // blocking
            break;
        case ACT_SEND_GAME_OVER:
            // send ready message to client act->u.player_id
            //send_msg(act->u.player_id, MSG_GAME_OVER, NULL, 0); // blocking
            break;
        case ACT_BROADCAST_SHUT_DOWN:
            // send shutdown message to all clients
            // broadcast_msg(MSG_SHUT_DOWN, NULL, 0);
            break;
        case ACT_BROADCAST_GAME_STATE:
            // send game state act->u.state to all clients
            // TODO serialize state
            // broadcast_msg(MSG_GAME_STATE, NULL, 0);
            break;
        case ACT_UNREGISTER_PLAYER:
            // remove player from game state act->u.player_id
            break;
        case ACT_SEND_ERROR:
            // send error message to client act->u.player_id
            //send_msg(act->u.player_id, MSG_ERROR, NULL, 0); // blocking
            break;
        default:
            break;
    }
}

void *action_thread(void *arg) {
    const ActionThreadArgs *args = arg;

    EventQueue *eq = args->eq;
    ActionQueue *aq = args->aq;
    const _Atomic bool *running = args->running;

    Action act;

    while (*running) {
        while (dequeue_action(aq, &act)) {
            exec_action(&act, eq);
        }
    }

    return NULL;
}