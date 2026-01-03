#ifndef SERPENT_PROTOCOL_H
#define SERPENT_PROTOCOL_H

#include <stdint.h>
#include <stdlib.h>
#include "game_types.h"

typedef enum {
    MSG_INPUT, // client sends input to server
    MSG_PAUSE, // client notifies server of pause
    MSG_RESUME, // client notifies server of resume
    MSG_LEAVE, // client notifies server of leaving
    MSG_READY, // server notifies client that game is ready
    MSG_GAME_OVER, // server notifies client of game over (or some sort of correct ending)
    MSG_STATE, // server sends snapshot of game state to client for rendering
    MSG_TIME, // server sends remaining time to client for rendering ... just for testing
    MSG_ERROR, // server notifies client of error (incorrect ending)
} MessageType;

// in-memory version (semantic)
typedef struct {
    MessageType type;
    uint32_t payload_size;
    void *payload; // owned by Message
} Message;

// wire stable helper
typedef struct {
    uint32_t type;
    uint32_t payload_size;
} MsgHeader;


static int send_all(int fd, const void *buf, size_t size);
static int recv_all(int fd, void *buf, size_t size);

// message -> byte send ->
int send_message(int fd, const Message *msg);
// -> byte recv -> message
int recv_message(int fd, Message *msg);

void message_destroy(Message *msg);

// specializations for convenience
// type -> payload mapping -> message -> byte send ->
int send_input(int fd, Direction dir);
int send_pause(int fd);
int send_resume(int fd);
int send_leave(int fd);
int send_ready(int fd);
int send_game_over(int fd);
int send_state(int fd, const GameRenderState *st);
int send_time(int fd, int seconds);
int send_error(int fd, const char *error_msg);

// (byte recv -> message ... done elsewhere i.e. not called recv_input ...)
// message -> payload mapping -> type
int msg_to_input(const Message *msg, Direction *dir);
int msg_to_state(const Message *msg, GameRenderState *st);
int msg_to_time(const Message *msg, int *seconds);
int msg_to_error(const Message *msg, char *error_msg);

#endif //SERPENT_PROTOCOL_H