#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

// message framing ... sockets are byte streams
// we need to ensure we send/recv all bytes
static int send_all(const int fd, const void *buf, const size_t size) {
    size_t sent = 0;
    while (sent < size) {
        const ssize_t n = send(fd, (const char *)buf + sent, size - sent, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EPIPE || errno == ECONNRESET) {
                // peer disconnected
                return -1;
            }
            return -1;
        }
        sent += n;
    }
    return 0;
}

static int recv_all(const int fd, void *buf, const size_t size) {
    size_t received = 0;
    while (received < size) {
        const ssize_t n = recv(fd, (char *)buf + received, size - received, 0);
        if (n <= 0) return -1;
        received += (size_t)n;
    }
    return 0;
}

void message_destroy(Message *msg) {
    if (!msg) return;
    free(msg->payload);
    msg->payload = NULL;
    msg->payload_size = 0;
}

// payload is owned by msg, this func must be used synchronously (main thread/ worker thread)
// as we do no heap allocate
int send_message(const int fd, const Message *msg) {
    if (!msg)
        return -1;

    const MsgHeader h = {
        .type = (uint32_t)msg->type,
        .payload_size = msg->payload_size
    };

    // send header
    if (send_all(fd, &h, sizeof(h)) < 0)
        return -1;

    // send payload if any
    if (msg->payload_size > 0) {
        if (!msg->payload)
            return -1;

        if (send_all(fd, msg->payload, msg->payload_size) < 0)
            return -1;
    }

    return 0;
}

// payload lives on heap, must be freed by caller/handler only -> ownership transfer
// will be used asynchronously via heap message bus on client and synchronously on
// server (input thread receives and translates to Event)
int recv_message(const int fd, Message *msg) {
    if (!msg)
        return -1;

    MsgHeader h;

    // receive header
    if (recv_all(fd, &h, sizeof(h)) < 0)
        return -1;

    msg->type = (MessageType)h.type;
    msg->payload_size = h.payload_size;
    msg->payload = NULL;

    if (msg->payload_size == 0)
        return 0;


    // allocate payload
    msg->payload = malloc(msg->payload_size);
    if (!msg->payload)
        return -1;


    // receive payload
    if (recv_all(fd, msg->payload, msg->payload_size) < 0) {
        free(msg->payload);
        msg->payload = NULL;
        return -1;
    }

    return 0;
}


// non-portable ... for simplicity
// send/recv as is in memory
// padding bytes may differ between architectures etc.
// but we are on same machine and using unix sockets only so far

int send_input(const int fd, Direction dir) {
    Message msg;
    msg.type = MSG_INPUT;
    msg.payload_size = sizeof(Direction);
    msg.payload = &dir; // raw in-memory value
    return send_message(fd, &msg);
}

int send_pause(const int fd) {
    Message msg;
    msg.type = MSG_PAUSE;
    msg.payload_size = 0;
    msg.payload = NULL;
    return send_message(fd, &msg);
}

int send_resume(const int fd) {
    Message msg;
    msg.type = MSG_RESUME;
    msg.payload_size = 0;
    msg.payload = NULL;
    return send_message(fd, &msg);
}

int send_leave(const int fd) {
    Message msg;
    msg.type = MSG_LEAVE;
    msg.payload_size = 0;
    msg.payload = NULL;
    return send_message(fd, &msg);
}

int send_ready(const int fd) {
    Message msg;
    msg.type = MSG_READY;
    msg.payload_size = 0;
    msg.payload = NULL;
    return send_message(fd, &msg);
}

int send_game_over(const int fd) {
    Message msg;
    msg.type = MSG_GAME_OVER;
    msg.payload_size = 0;
    msg.payload = NULL;
    return send_message(fd, &msg);
}

int send_state(const int fd, const ClientGameStateSnapshot *st) {
    // we pack data as header + arrays (bytes)
    // other message does need this as they have no dynamic arrays
    const GameStateWireHeader h = {
        .width = st->width,
        .height = st->height,
        .score = st->score,
        .player_time_elapsed = st->player_time_elapsed,
        .game_time_remaining = st->game_time_remaining,
        .snake_count = st->snake_count,
        .fruit_count = st->fruit_count,
        .obstacle_count = st->obstacle_count,
    };

    size_t payload_size = sizeof(h);

    // snakes
    for (size_t i = 0; i < st->snake_count; i++) {
        payload_size += sizeof(uint32_t); // length
        payload_size += st->snakes[i].length * sizeof(Position);
    }

    payload_size += st->fruit_count    * sizeof(Fruit);
    payload_size += st->obstacle_count * sizeof(Obstacle);

    void *buf = calloc(1, payload_size);
    if (!buf) return -1;


    // writing to buffer
    uint8_t *p = buf;

    memcpy(p, &h, sizeof(h));
    p += sizeof(h);

    // snakes
    for (size_t i = 0; i < st->snake_count; i++) {
        uint32_t len = st->snakes[i].length;
        memcpy(p, &len, sizeof(len));
        p += sizeof(len);

        memcpy(p, st->snakes[i].body, len * sizeof(Position));
        p += len * sizeof(Position);
    }

    memcpy(p, st->fruits,    st->fruit_count * sizeof(Fruit));
    p += st->fruit_count * sizeof(Fruit);
    memcpy(p, st->obstacles, st->obstacle_count * sizeof(Obstacle));

    const Message msg = {
        .type = MSG_STATE,
        .payload_size = payload_size,
        .payload = buf,
    };

    const int rc = send_message(fd, &msg);
    free(buf);

    return rc;
}


int send_time(const int fd, int seconds) {
    Message msg;
    msg.type = MSG_TIME;
    msg.payload_size = sizeof(int);
    msg.payload = &seconds;
    return send_message(fd, &msg);
}

int send_error(const int fd, const char *error_msg) {
    if (!error_msg) return -1;
    Message msg;
    msg.type = MSG_ERROR;
    msg.payload_size = (uint32_t)(strlen(error_msg) + 1); // include null terminator
    msg.payload = (void *)error_msg;
    return send_message(fd, &msg);
}

// dynamically allocates arrays inside st, caller must free them
int msg_to_state(const Message *msg, ClientGameStateSnapshot *st) {

    if (!msg || msg->type != MSG_STATE || !msg->payload)
        return -1;

    const uint8_t *p = msg->payload;

    GameStateWireHeader h = {0};
    memcpy(&h, p, sizeof(h));
    p += sizeof(h);

    st->width  = (int)h.width;
    st->height = (int)h.height;
    st->score  = h.score;
    st->player_time_elapsed = (int)h.player_time_elapsed;
    st->game_time_remaining = (int)h.game_time_remaining;

    // allocate

    st->snake_count = h.snake_count;
    st->snakes = malloc(h.snake_count * sizeof(SnakeSnapshot));

    for (size_t i = 0; i < st->snake_count; i++) {
        uint32_t len;
        memcpy(&len, p, sizeof(len));
        p += sizeof(len);

        st->snakes[i].length = len;
        st->snakes[i].body = malloc(len * sizeof(Position));

        if (!st->snakes[i].body) {
            snapshot_destroy(st);
            return -1;
        }

        memcpy(st->snakes[i].body, p, len * sizeof(Position));
        p += len * sizeof(Position);
    }

    st->fruits    = malloc(h.fruit_count * sizeof(Fruit));
    st->obstacles = malloc(h.obstacle_count * sizeof(Obstacle));

    if (!st->snakes || !st->fruits || !st->obstacles) {
        snapshot_destroy(st);
        return -1;
    }

    st->fruit_count  = h.fruit_count;
    st->obstacle_count = h.obstacle_count;

    memcpy(st->fruits,    p, h.fruit_count * sizeof(Fruit));
    p += h.fruit_count * sizeof(Fruit);

    memcpy(st->obstacles, p, h.obstacle_count * sizeof(Obstacle));

    return 0;
}



int msg_to_input(const Message *msg, Direction *dir) {
    if (!msg || !dir) return -1;
    if (msg->type != MSG_INPUT ||
        msg->payload_size != (int)sizeof(Direction) ||
        !msg->payload) {
        return -1;
        }
    //*dir = *(Direction *)msg->payload;   // cast and dereference
    // case and dereference is most of the time equivalent but
    // memcpy is safer because no padding assumption etc
    memcpy(dir, msg->payload, sizeof(Direction)); // copy bytes from buffer into typed variable
    return 0;
}

int msg_to_time(const Message *msg, int *seconds) {
    if (!msg || !seconds) return -1;
    if (msg->type != MSG_TIME ||
        msg->payload_size != (int)sizeof(int) ||
        !msg->payload) {
        return -1;
        }
    memcpy(seconds, msg->payload, sizeof(int));
    return 0;
}

int msg_to_error(const Message *msg, char *error_msg) {
    if (!msg || !error_msg) return -1;
    if (msg->type != MSG_ERROR ||
        msg->payload_size <= 0 ||
        !msg->payload) {
        return -1;
        }
    // copy payload to error_msg buffer
    strncpy(error_msg, (char *)msg->payload, msg->payload_size);
    error_msg[msg->payload_size - 1] = '\0'; // ensure null termination
    return 0;
}


// portable serde ... network byte order ... avoiding for simplicity
/*
int send_input(const int fd, const Direction dir) {
    uint32_t v = htonl((uint32_t)dir);
    Message msg;
    msg.type = MSG_INPUT;
    msg.payload_size = sizeof(v);
    msg.payload = &v;
    return send_message(fd, &msg);
}


int msg_to_input(const Message *msg, Direction *dir) {
    if (!msg || !dir)
        return -1;

    if (msg->type != MSG_INPUT ||
        msg->payload_size != (int)sizeof(uint32_t) ||
        msg->payload == NULL) {
        return -1;
        }

    uint32_t v_be;
    memcpy(&v_be, msg->payload, sizeof(v_be));
    *dir = (Direction)ntohl(v_be);

    return 0;
}
*/