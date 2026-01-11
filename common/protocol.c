#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

// message framing ... sockets are byte streams
// we need to ensure we send/recv all bytes
/**
 * Sends the entire buffer over a socket.
 *
 * Repeatedly calls send() until all bytes are sent or an error occurs.
 * Handles partial sends and interrupted system calls.
 *
 * @param fd    Socket file descriptor.
 * @param buf   Pointer to data buffer.
 * @param size  Number of bytes to send.
 * @return 0 on success, -1 on error or peer disconnect.
 */
static int send_all(const int fd, const void *buf, const size_t size) {
    size_t sent = 0;
    while (sent < size) {
        // Attempt to send the remaining part of the buffer
        const ssize_t n = send(fd, (const char *)buf + sent, size - sent, 0);
        // Check if send() failed
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

/**
 * Receives an exact number of bytes from a socket.
 *
 * Repeatedly calls recv() until the requested number of bytes is received
 * or the connection is closed or an error occurs.
 *
 * @param fd    Socket file descriptor.
 * @param buf   Destination buffer.
 * @param size  Number of bytes to receive.
 * @return 0 on success, -1 on error or peer disconnect.
 */
static int recv_all(const int fd, void *buf, const size_t size) {
    size_t received = 0;
    while (received < size) {
        // Attempt to receive the remaining bytes
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
/**
 * Sends a complete protocol message over a socket.
 *
 * The message is sent in two parts: a fixed-size header containing
 * the message type and payload size, followed by the optional payload.
 *
 * @param fd   Socket file descriptor.
 * @param msg  Pointer to the message to send.
 * @return 0 on success, -1 on error.
 */
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
/**
 * Receives a complete protocol message from a socket.
 *
 * First receives a fixed-size header to determine the message type and
 * payload size, then allocates memory and receives the payload if present.
 *
 * @param fd   Socket file descriptor.
 * @param msg  Pointer to a Message structure to fill.
 * @return 0 on success, -1 on error or connection failure.
 *
 * @note The caller is responsible for freeing msg->payload when payload_size > 0.
 */
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

/**
 * Sends a serialized snapshot of the current client game state.
 *
 * The game state is packed into a single contiguous payload consisting of
 * a fixed-size header followed by variable-length arrays (snakes, fruits,
 * obstacles). The payload is then sent as a MSG_STATE message.
 *
 * @param fd  Socket file descriptor.
 * @param st  Pointer to the client game state snapshot.
 * @return 0 on success, -1 on error.
 */
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

int send_error(const int fd, const char *error_msg) {
    if (!error_msg) return -1;
    Message msg;
    msg.type = MSG_ERROR;
    msg.payload_size = (uint32_t)(strlen(error_msg) + 1); // include null terminator
    msg.payload = (void *)error_msg;
    return send_message(fd, &msg);
}

// dynamically allocates arrays inside st, caller must free them
/**
 * Converts a MSG_STATE message payload into a client game state snapshot.
 *
 * Deserializes the wire-format payload into dynamically allocated game state
 * structures. The caller becomes the owner of all allocated memory and must
 * free it using snapshot_destroy().
 *
 * @param msg  Pointer to the received message.
 * @param st   Pointer to the destination game state snapshot.
 * @return 0 on success, -1 on error or invalid message.
 */
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


int msg_to_error(const Message *msg, char *error_msg, const size_t buf_size) {
    if (!msg || !error_msg) return -1;
    if (msg->type != MSG_ERROR ||
        msg->payload_size <= 0 ||
        !msg->payload) {
        return -1;
        }

    size_t n = msg->payload_size;
    if (n > buf_size) n = buf_size - 1;

    // copy payload to error_msg buffer
    memcpy(error_msg, msg->payload, n);
    error_msg[n] = '\0'; // ensure null termination
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