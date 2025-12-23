#include "input.h"
#include "menu.h"
#include "common/config.h"
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

void handle_menu_key(Menu *menu, const Key key) {
    printf("Key pressed: %d (%c)\n", (int)key, (char)key);
    switch (key) {
        case KEY_UP:
            if (menu->selected_index > 0) {
                menu->selected_index--;
            }
            break;
        case KEY_DOWN:
            if (menu->selected_index < menu->button_count - 1) {
                menu->selected_index++;
            }
            break;
        // todo user data are so far always client context pointer so maybe we can simplify
        case KEY_ENTER:
            if (menu->buttons[menu->selected_index].on_press) {
                menu->buttons[menu->selected_index].on_press(menu->buttons[menu->selected_index].user_data);
            }
            break;

        case KEY_QUIT:
            btn_exit_game(menu->buttons[menu->selected_index].user_data);
            break;
        case KEY_ESC:
            btn_back_in_menu(menu->buttons[menu->selected_index].user_data);
            break;
        default:
            break;
    }
}

void handle_game_key(Key key, ClientContext *ctx) {
    if (key == KEY_QUIT || key == KEY_PAUSE || key == KEY_ESC) {
        ctx->mode = CLIENT_PAUSED;
        clear_menus_stack(&ctx->menus);
        snprintf(ctx->pause_menu.txt_fields[0].text, sizeof(ctx->pause_menu.txt_fields[0].text),
             "Your current score is: %d", ctx->score);
        menu_push(&ctx->menus, &ctx->pause_menu);
    }

    // otherwise send direction to server
    //send_input(ctx, key); TODO
}

void read_input(ClientInputQueue *queue, const _Atomic bool *running) {
    struct termios old_tio, new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    while (*running) {
        int ch = getchar();
        if (ch == EOF) break;
        enqueue_input(queue, (Key)ch);
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
}

void enqueue_input(ClientInputQueue *queue, Key key) {
    if (!((key >= 32 && key <= 126) || key == '\n' || key == '\r' || key == '\t' || key == 27)) {
        return; /* ignore unsupported keys */
    }

    pthread_mutex_lock(&queue->lock);
    if (queue->count < sizeof(queue->events) / sizeof(queue->events[0])) {
        queue->events[queue->count++] = key;
    }
    pthread_mutex_unlock(&queue->lock);
}

bool dequeue_input(ClientInputQueue *queue, Key *key) {
    pthread_mutex_lock(&queue->lock);
    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->lock);
        return false;
    }
    *key = queue->events[0];

    // shift remaining events
    for (size_t i = 1; i < queue->count; ++i) {
        queue->events[i - 1] = queue->events[i];
    }
    queue->count--;
    pthread_mutex_unlock(&queue->lock);
    return true;
}

