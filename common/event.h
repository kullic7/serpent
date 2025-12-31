#ifndef SERPENT_EVENT_H
#define SERPENT_EVENT_H

/*
void *action_thread(void *arg) {
    ActionQueue *aq = arg;
    Action act;

    while (dequeue_action(aq, &act)) {
        switch (act.type) {
            case ACT_WAIT_FOR_SOCKET:
                wait_for_socket(act.u.socket.path);
                break;
            case ACT_LOAD_WORLD:
                load_world_by_id(act.u.world.world_id);
                break;
            case ACT_SEND_INPUT:
                send_input_to_server(act.u.input.direction);
                break;

        }
    }
    return NULL;
}
*/

typedef enum {
    ACT_CONNECT, // client calls connect_to_server
    ACT_WAIT_FOR_SOCKET,
    ACT_LOAD_WORLD,
    ACT_SEND_INPUT,
    /* ... */
} EventType;

typedef struct {
    char path[108];
} ActionSocketPath;

typedef struct {
    int socket_fd;
} ActionSocketFD;

typedef struct {
    int world_id;
} ActionWorld;

typedef struct {
    int direction; /* or your own enum */
} ActionInput;

typedef struct {
    ActionType type;
    union {
        ActionSocketPath socket;
        ActionWorld      world;
        ActionInput      input;
        /* add more payloads here */
    } u;
} Action;

#endif //SERPENT_EVENT_H