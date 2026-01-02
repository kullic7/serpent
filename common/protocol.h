#ifndef SERPENT_PROTOCOL_H
#define SERPENT_PROTOCOL_H

typedef enum {
    MSG_INPUT, // client sends input to server
    MSG_PAUSE, // client notifies server of pause
    MSG_RESUME, // client notifies server of resume
    MSG_LEAVE, // client notifies server of leaving
    MSG_READY, // server notifies client that game is ready
    MSG_GAME_OVER, // server notifies client of game over
    MSG_SHUT_DOWN, // server notifies client of shutdown
    MSG_STATE, // server sends game state to client
    MSG_ERROR, // server notifies client of error
    //MSG_ACK, // acknowledgment message
} MessageType;

typedef struct MessageHeader {
    MessageType type;
    int client_id;
    int payload_size;
} MessageHeader;

typedef struct Message {
    MessageHeader header;
    void *payload;
} Message;


#endif //SERPENT_PROTOCOL_H