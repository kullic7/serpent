#ifndef SERPENT_EVENT_H
#define SERPENT_EVENT_H
#include <stddef.h>
#include <bits/pthreadtypes.h>

#include "config.h"
#include <stdbool.h>


// events from worker or other input thread to main thread
// sort of response (something has happened)
typedef enum {
    EV_CONNECTED, // worker signals new player connected needs add new player to game state : main handles ... player id
    EV_LOADED, // worker signals world loaded : main handles. ... no params
    EV_PAUSED, // input thread signals pause clicked : main handles ... player id
    EV_RESUMED, // input thread signals resume clicked : main handles
    EV_DISCONNECTED, // input thread signals player disconnected : main handles
    EV_ERROR, // worker signals error occurred : main handles (send_error_msg)
} EventType;

typedef struct {
    EventType type;
    union {
        int nodata;
        int player_id;
    } u;
} Event;

// commands from main thread to worker (worker may respond with events)
typedef enum {
    ACT_LOAD_WORLD, // main -> worker: response event EV_LOADED
    ACT_SEND_READY, // (worker sends EV_CONNECTED) main -> worker: send msg ready, with player info
    ACT_SEND_GAME_OVER, // send msg game over, with player info + game info
    ACT_BROADCAST_SHUT_DOWN, // no params
    ACT_BROADCAST_GAME_STATE, // WorldState param
    ACT_UNREGISTER_PLAYER, // player id param
    ACT_SEND_ERROR, // player id param
} ActionType;

typedef struct {
    int player_id;
    int time_in_game; // in seconds
    int score;
} PlayerInfo;

typedef struct {
    int world_id;
    // TODO add relevant data
} WorldState;


typedef struct {
    ActionType type;
    union {
        int             player_id;
        PlayerInfo      info;
        WorldState      state;
    } u;
} Action;

// *-------------- queue implementations -----------------

typedef struct {
    Event events[MAX_EVENTS];
    size_t count;
    pthread_mutex_t lock;
    pthread_cond_t not_full;
} EventQueue;

void event_queue_init(EventQueue *q);
void event_queue_destroy(EventQueue *q);
void enqueue_event(EventQueue *q, Event ev);
bool dequeue_event(EventQueue *q, Event *ev);


typedef struct {
    Action actions[MAX_ACTIONS];
    size_t count;
    pthread_mutex_t lock;
    pthread_cond_t not_full;
} ActionQueue;

void action_queue_init(ActionQueue *q);
void action_queue_destroy(ActionQueue *q);
void enqueue_action(ActionQueue *q, Action act);
bool dequeue_action(ActionQueue *q, Action *act);

void exec_action(const Action *act, EventQueue *q); // actions come via output queue and are handled in worker thread only
void handle_event(const Event *ev, ActionQueue *q); // events come via input queue and are handled in main thread only

typedef struct {
    EventQueue *eq;
    ActionQueue *aq;
    const _Atomic bool *running;
} ActionThreadArgs;

// worker thread function
void *action_thread(void *arg);

#endif //SERPENT_EVENT_H