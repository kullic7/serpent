#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

// message framing ... sockets are byte streams
// we need to ensure we send/recv all bytes
static int send_all(const int fd, const void *buf, const size_t size) {
    size_t sent = 0;
    while (sent < size) {
        const ssize_t n = send(fd, (const char *)buf + sent, size - sent, 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
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