#ifndef SERPENT_CONFIG_H
#define SERPENT_CONFIG_H

#define LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#define SERVER_LOG_FILE "server_log.txt"
#define CLIENT_LOG_FILE "client_log.txt"

#define MAX_CLIENTS 16
#define TICK_RATE 10  // Game updates per second
#define INITIAL_SNAKE_LENGTH 3
#define FRUIT_COUNT 5

#define MAX_EVENTS 1024
#define MAX_ACTIONS 1024
#define MAX_KEY_EVENTS 16
#define MAX_MESSAGES 1024

#define MENU_MAX_TEXT_LENGTH 64

#define BUTTON_WIDTH 40
#define BUTTON_HEIGHT 10


#define TARGET_FPS 60
#define FRAME_TIME_MS (1000 / TARGET_FPS)

#define SNAKE_CHAR "o"
#define FRUIT_CHAR "*"
#define OBSTACLE_CHAR "#"

// ascii key codes
#define KEY_DOWN    's'
#define KEY_UP      'w'
#define KEY_LEFT    'a'
#define KEY_RIGHT   'd'
#define KEY_PAUSE   'p'
#define KEY_QUIT    'q'
#define KEY_ENTER   '\n'
#define KEY_ESC     '\x1b'

#define MENU_STACK_MAX 12

#endif //SERPENT_CONFIG_H