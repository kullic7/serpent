#include "client.h"
#include "renderer.h"
#include "buttons.h"
#include "protocol.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <timer.h>
#include <poll.h>
#include "logging.h"

void client_run(ClientContext *ctx, ClientInputQueue *iq, ServerInputQueue *sq) {
    while (ctx->running) {

        poll_server_exit(ctx);

        // TODO signal handling for graceful exit e.g. game time elapsed on server side
        // e.g. servers game over, here we need to recv
        // async processing of keyboard signals, when too many we drop them
        Key key;
        while (dequeue_key(iq, &key)) {

            if (ctx->input_mode == INPUT_TEXT) {
                handle_text_input(ctx, key);
                continue;
            }

            if (ctx->mode == CLIENT_MENU || ctx->mode == CLIENT_PAUSED || ctx->mode == CLIENT_GAME_OVER) {
                Menu *menu = menu_current(&ctx->menus);
                handle_menu_key(menu, key);
            }
            else if (ctx->mode == CLIENT_PLAYING) {
                handle_game_key(key, ctx);
            }
            client_input_queue_flush(iq);
        }

        // async processing of server messages; when queue is full secondary thread waits
        Message msg; // message payload on heap needs freeing
        while (dequeue_msg(sq, &msg)) {
            handle_server_msg(ctx, msg);
            message_destroy(&msg);  // if payload_size > 0
        }

        if (ctx->mode == CLIENT_MENU || ctx->mode == CLIENT_PAUSED || ctx->mode == CLIENT_GAME_OVER) {
            render_menu(menu_current(&ctx->menus), ctx->input_mode, ctx->text_note, ctx->text_buffer, ctx->text_len);
        }
        else if (ctx->mode == CLIENT_PLAYING) {
            render_game(ctx->game);
        }
    }

}

void client_init(ClientContext *ctx) {
    memset(ctx, 0, sizeof(*ctx));

    ctx->running = true;
    ctx->mode = CLIENT_MENU;
    ctx->socket_fd = -1; // Not connected yet
    ctx->server_pid = -1;

    ctx->input_mode = INPUT_GAME;

    //ctx->score = 0;
    //ctx->time_elapsed = 0;

    const ClientGameStateSnapshot state = {
        .width = 0,
        .height = 0,
        .score = 0,
        .game_time_remaining = 0,
        .player_time_elapsed = 0,
        .snake_count = 0,
        .snakes = NULL,
        .fruit_count = 0,
        .fruits = NULL,
        .obstacle_count = 0,
        .obstacles = NULL
    };

    ctx->game = state;

    // todo probably explicit menu is not needed, can be selected from ctx
    init_main_menu(&ctx->main_menu, ctx);
    init_pause_menu(&ctx->pause_menu, ctx);
    init_mode_select_menu(&ctx->mode_select_menu, ctx);
    init_world_select_menu(&ctx->world_select_menu, ctx);
    init_player_select_menu(&ctx->player_select_menu, ctx);
    init_load_menu(&ctx->load_menu, ctx);
    init_game_over_menu(&ctx->game_over_menu, ctx);
    init_awaiting_menu(&ctx->awaiting_menu, ctx);
    init_error_menu(&ctx->error_menu, ctx);

    init_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->main_menu);
}


void client_cleanup(ClientContext *ctx) {

    if (ctx->socket_fd >= 0)
        close(ctx->socket_fd);

    term_show_cursor();
    term_clear();
    term_home();

    //free_game_state(&ctx->game);
}

void init_main_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            .text = "New Game",
            .on_press = btn_new_game,
            .user_data = NULL
        },
        {
            .text = "Join Game",
            //.on_press = btn_new_game,
            .on_press = btn_connect_to_server,
            .user_data = NULL
        },
        {
            .text = "Exit Game",
            .on_press = btn_exit_game,
            .user_data = NULL
        }
    };

    init_menu_fields(menu, buttons, LEN(buttons), NULL, 0, ctx);
}


void init_pause_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            .text = "Resume Game",
            .on_press = btn_resume_game,
            .user_data = NULL
        },
        {
                .text = "Exit to Main Menu",
                .on_press = btn_exit_to_main_menu,
                .user_data = NULL
            }
    };

    static TextField txt_fields[] = {
        {
            .text = "", // for score message
        }
    };

    init_menu_fields(menu, buttons, LEN(buttons), txt_fields, LEN(txt_fields), ctx);
}


void init_error_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            .text = "Exit to Main Menu",
            .on_press = btn_exit_to_main_menu,
            .user_data = NULL
        }
    };

    static TextField txt_fields[] = {
        {
            .text = "", // for error message
        }
    };

    init_menu_fields(menu, buttons, LEN(buttons), txt_fields, LEN(txt_fields), ctx);
}


void init_mode_select_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            .text = "Standard Mode",
            .on_press = btn_standard_mode,
            .user_data = NULL
        },
        {
            .text = "Timed Mode",
            .on_press = btn_time_mode,
            .user_data = NULL
        },
        {
            .text = "Back",
            .on_press = btn_back_in_menu,
            .user_data = NULL
        }
    };

    static TextField txt_fields[] = {
        {
            .text = "Select game mode.",
        }
    };

    init_menu_fields(menu, buttons, 3, txt_fields, 1, ctx);
}


void init_world_select_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            .text = "Easy World",
            .on_press = btn_easy_world_mode,
            .user_data = NULL
        },
        {
            .text = "Hard World", // with obstacles
            .on_press = btn_hard_world_mode,
            .user_data = NULL
        },
        {
            .text = "Back",
            .on_press = btn_back_in_menu,
            .user_data = NULL
        }
    };

    static TextField txt_fields[] = {
        {
            .text = "Select difficulty. (Obstacles)",
        }
    };

    init_menu_fields(menu, buttons, 3, txt_fields, 1, ctx);
}

void init_load_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            .text = "From File", // pre-defined obstacles
            .on_press = btn_load_from_file,
            .user_data = NULL
        },
        {
            .text = "Random", // random obstacles
            .on_press = btn_random_world,
            .user_data = NULL
        },
        {
            .text = "Back",
            .on_press = btn_back_in_menu,
            .user_data = NULL
        }
    };

    static TextField txt_fields[] = {
        {
            .text = "Select world load method.",
        }
    };

    init_menu_fields(menu, buttons, 3, txt_fields, 1, ctx);
}

void init_player_select_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            .text = "For One",
            .on_press = btn_single_player,
            .user_data = NULL
        },
        {
            .text = "For Many",
            .on_press = btn_multiplayer,
            .user_data = NULL
        },
        {
            .text = "Back",
            .on_press = btn_back_in_menu,
            .user_data = NULL
        },
    };

    static TextField txt_fields[] = {
        {
            .text = "Select num. of players.",
        }
    };

    init_menu_fields(menu, buttons, 3, txt_fields, 1, ctx);
}


void init_game_over_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            .text = "Play Again",
            .on_press = btn_new_game,
            .user_data = NULL
        },
        {
            .text = "Exit to Main Menu",
            .on_press = btn_exit_to_main_menu,
            .user_data = NULL
        },
        {
            .text = "Exit Game",
            .on_press = btn_exit_game,
            .user_data = NULL
        }

    };

    static TextField txt_fields[] = {
        {
            .text = "", // for game over message
        }
    };


    init_menu_fields(menu, buttons, 3, txt_fields, 1, ctx);
}

void init_awaiting_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            .text = "Cancel",
            .on_press = btn_cancel_awaiting, // TODO it should be real cancel action ... like notify server, destroy process etc
            .user_data = NULL
        }
    };

    static TextField txt_fields[] = {
        {
            .text = "Awaiting server...", // for awaiting message
        }
    };

    init_menu_fields(menu, buttons, 1, txt_fields, 1, ctx);
}

/*
void init_join_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            .text = "Join Game",
            .on_press = btn_join_game,
            .user_data = NULL
        },
        {
            .text = "Exit to Main Menu",
            .on_press = btn_exit_to_main_menu,
            .user_data = NULL
        },
        {
            .text = "Back",
            .on_press = btn_back_in_menu,
            .user_data = NULL
        }
    };

    static TextField txt_fields[] = {
        {
            .text = "Game server created.", // TODO it should be ack from server but whatever
        }
    };

    init_menu_fields(menu, buttons, 3, txt_fields, 1, ctx);
}
*/

static void init_menu_fields(Menu *menu, Button *buttons, size_t b_count,
    TextField *txt_fields, size_t t_count, ClientContext *ctx) {
    for (size_t i = 0; i < b_count; ++i) {
        buttons[i].user_data = ctx;
    }

    menu->buttons = buttons;
    menu->button_count = b_count;

    menu->txt_fields = txt_fields;
    menu->txt_fields_count = t_count;

    menu->selected_index = 0;
}

void handle_menu_key(Menu *menu, const Key key) {
    //printf("Key pressed: %d (%c)\n", (int)key, (char)key);
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

        log_client("sending message paused to server\n");
        send_pause(ctx->socket_fd);
        log_client("send\n");

        clear_menus_stack(&ctx->menus);
        snprintf(ctx->pause_menu.txt_fields[0].text, sizeof(ctx->pause_menu.txt_fields[0].text),
             "Your current score is: %d", (int)ctx->game.score);
        menu_push(&ctx->menus, &ctx->pause_menu);
    }

    Direction dir;
    switch (key) {
        case KEY_UP:
            dir = DIR_UP;
            break;
        case KEY_DOWN:
            dir = DIR_DOWN;
            break;
        case KEY_LEFT:
            dir = DIR_LEFT;
            break;
        case KEY_RIGHT:
            dir = DIR_RIGHT;
            break;
        default:
            return; // ignore other keys
    }

    log_client("sending direction to server\n");
    send_input(ctx->socket_fd, dir);
    log_client("send\n");
}

void handle_server_msg(ClientContext *ctx, const Message msg) {
    // only message handler
    /*
     * must interpret payload (here would come deserialization if needed from wire format)
     * free payload after processing
     */
    switch (msg.type) {
        case MSG_READY:
            ctx->mode = CLIENT_PLAYING;
            log_client("msg ready received\n");
            break;
        case MSG_GAME_OVER:
            ctx->mode = CLIENT_GAME_OVER;

            snprintf(ctx->game_over_menu.txt_fields[0].text, sizeof(ctx->game_over_menu.txt_fields[0].text),
                     "Game over. Your score is: %d", (int)ctx->game.score);
            log_client("msg game over received\n");
            break;
        case MSG_STATE: {
            ClientGameStateSnapshot new_state = {0};

            if (msg_to_state(&msg, &new_state) == 0) {
                snapshot_destroy(&ctx->game);   // free old
                ctx->game = new_state;          // move ownership  TODO by value so is it ok ??????
                log_client("game state updated\n");
            }

            log_client("msg state received\n");
            break;
        }
        case MSG_TIME: {
            int time;
            msg_to_time(&msg, &time);
            ctx->game.game_time_remaining = time;
            log_client("msg time received\n");
            break;
        }
        case MSG_ERROR: {
            char error_msg;
            msg_to_error(&msg, &error_msg);

            log_client("msg error received: %c\n");

            ctx->mode = CLIENT_MENU;

            snprintf(ctx->error_menu.txt_fields[0].text, sizeof(ctx->error_menu.txt_fields[0].text),
                 "Error from server: %c", error_msg);
            clear_menus_stack(&ctx->menus);
            menu_push(&ctx->menus, &ctx->error_menu);
            break;
        }
        default:
            break;

    }
}

void handle_text_input(ClientContext *ctx, const Key key) {

    // handle escape
    if (key == KEY_ESC) {
        ctx->input_mode = INPUT_GAME;
        ctx->text_len = 0;
        memset(ctx->text_buffer, 0, sizeof(ctx->text_buffer));
        return;
    }

    // handle submission
    if (key == '\n' || key == '\r') {
        ctx->text_buffer[ctx->text_len] = '\0';

        if (ctx->on_text_submit) {
            ctx->on_text_submit(ctx, ctx->text_buffer);
        }

        ctx->text_len = 0;
        ctx->input_mode = INPUT_GAME;
        return;
    }

    // handle backspace removal ... backspace nor del seem to work well so using '-' for now
    if (key == '-') {
        if (ctx->text_len > 0) {
            ctx->text_len--;
            ctx->text_buffer[ctx->text_len] = '\0';
        }
        return;
    }

    // append character to buffer
    if (ctx->text_len < sizeof(ctx->text_buffer) - 1 &&
        key >= 32 && key <= 126) {
        ctx->text_buffer[ctx->text_len++] = (char)key;
        }
}

// thread functions


void *read_input_thread(void *arg) {
    const InputThreadArgs *args = arg;

    ClientInputQueue *queue = args->queue;
    const _Atomic bool *running = args->running;

    read_keyboard_input(queue, running);

    return NULL;
}


/*
void *read_input_thread(void *arg) {
    const InputThreadArgs *args = arg;

    ClientInputQueue *queue = args->queue;
    const _Atomic bool *running = args->running;

    const int timeout_ms = 100;
    struct pollfd pfd = {
        .fd = 0,          // stdin or your input fd
        .events = POLLIN,
        .revents = 0
    };

    while (*running) {
        pfd.revents = 0;

        int ret = poll(&pfd, 1, timeout_ms);
        if (ret < 0) {
            // optional: log error
            continue;
        }
        if (ret == 0) {
            // timeout, just re-check *running
            continue;
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            // optional: handle input error / closed
            continue;
        }

        if (pfd.revents & POLLIN) {
            // should be a non-blocking / single-iteration read helper
            read_keyboard_input(queue, running);
        }
    }

    return NULL;
}
*/
/*
void *recv_server_thread(void *arg) {
    const ReceiveThreadArgs *args = arg;

    const int *socket_fd = args->socket_fd;
    ServerInputQueue *queue = args->queue;
    const _Atomic bool *running = args->running;

    const int timeout_ms = 100;

    while (*running) {
        Message msg;
        if (*socket_fd < 0) {
            // not yet connected
            continue;
        }

        if (recv_message(*socket_fd, &msg) < 0) {
            // handle error, e.g. connection lost
            // TODO it should rather send event
            //break;
            continue;
        }
        enqueue_msg(queue, msg);
    }

    return NULL;
}
*/

// improved version with poll and timeout to check running flag
void *recv_server_thread(void *arg) {
    const ReceiveThreadArgs *args = arg;

    const int *socket_fd = args->socket_fd;
    ServerInputQueue *queue = args->queue;
    const _Atomic bool *running = args->running;

    // timeout in milliseconds (e.g. 100 ms)
    const int timeout_ms = 100;

    while (*running) {
        struct pollfd pfd;
        pfd.fd = *socket_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        if (*socket_fd < 0) {
            // not yet connected, sleep a bit to avoid busy loop
            sleepn(10000); // 100 ms

            continue;
        }

        const int ret = poll(&pfd, 1, timeout_ms);
        if (ret < 0) {
            // error in poll; you might want to log and break or continue
            continue;
        } else if (ret == 0) {
            // timeout: nothing to read, loop again so we can check *running
            continue;
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            // connection error/closed
            // TODO: set some error flag, or stop running
            continue;
        }

        if (pfd.revents & POLLIN) {
            Message msg;
            if (recv_message(*socket_fd, &msg) < 0) {
                // handle recv error (e.g. connection lost)
                // TODO: send event or stop running
                continue;
            }
            enqueue_msg(queue, msg);
        }
    }

    return NULL;
}