#define _POSIX_C_SOURCE 199309L
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include "client.h"
#include "renderer.h"
#include "callbacks.h"
#include "protocol.h"
#include "timer.h"
#include "logging.h"
#include "context.h"

void client_run(ClientContext *ctx, ClientInputQueue *iq, ServerInputQueue *sq) {
    while (ctx->running) {

        // reaping server process if exited so we do not create zombies
        poll_server_exit(ctx);

        // TODO setup timeout/retry for awaiting menu
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
                handle_game_key(ctx, key);
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

    ctx->input_mode = INPUT_KEY;

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

    init_main_menu(ctx);
    init_pause_menu(ctx);
    init_mode_select_menu(ctx);
    init_world_select_menu(ctx);
    init_player_select_menu(ctx);
    init_load_menu(ctx);
    init_game_over_menu(ctx);
    init_awaiting_menu(ctx);
    init_error_menu(ctx);

    init_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->main_menu);
}


void client_cleanup(ClientContext *ctx) {

    disconnect_from_server(ctx);

    snapshot_destroy(&ctx->game);

    term_show_cursor();
    term_clear();
    term_home();

}


// used by very first client to spawn server and connect to it
// single or multi player
bool spawn_connect_create_server(ClientContext *ctx) {

    // check if socket file not already taken
    if (wait_for_server_socket(ctx->server_path, 300)) {
        snprintf(ctx->error_menu.txt_fields[0].text, sizeof(ctx->error_menu.txt_fields[0].text),
             "Error. Socket address already taken. Use another.");
        return false;
    }

    if (!spawn_server_process(ctx)) {
        snprintf(ctx->error_menu.txt_fields[0].text, sizeof(ctx->error_menu.txt_fields[0].text),
             "Error. Spawning of server process failed.");
        return false;
    }

    // simplified readiness check for unix socket so we do not connect too early
    if (!wait_for_server_socket(ctx->server_path, 3000)) {
        snprintf(ctx->error_menu.txt_fields[0].text, sizeof(ctx->error_menu.txt_fields[0].text),
             "Error. Server did not create socket in time.");
        return false;
    }

    if (!connect_to_server(ctx)) {
        snprintf(ctx->error_menu.txt_fields[0].text, sizeof(ctx->error_menu.txt_fields[0].text),
             "Error. Could not connect to server.");
        return false;
    }

    return true;

}

bool spawn_server_process(ClientContext *ctx) {
    int pid = fork();
    if (pid == 0) {
        // child process: execute the server with appropriate arguments
        char time_arg[16];
        if (ctx->time_remaining >= 0) {
            snprintf(time_arg, sizeof(time_arg), "%d", ctx->time_remaining);
        } else {
            snprintf(time_arg, sizeof(time_arg), "-1");
        }

        execl("./serpent-server", "serpent-server",
            ctx->server_path,
            ctx->game_mode == GAME_SINGLE ? "1" : "0",
            time_arg,
            ctx->obstacles_enabled ? "1" : "0",
            ctx->obstacles_enabled && ctx->random_world_enabled ? "1" : "0",
            ctx->obstacles_enabled && !ctx->random_world_enabled ? ctx->file_path : "",
            NULL);
        // if execl returns, there was an error
        perror("execl failed");
        _exit(1);
    }
    if (pid < 0) {
        // fork failed
        perror("fork failed");
        return false; // indicate failure
    }

    ctx->server_pid = pid;

    return true; // parent process
}

void poll_server_exit(ClientContext *ctx) {
    if (ctx->server_pid <= 0)
        return;

    int status;
    const pid_t r = waitpid(ctx->server_pid, &status, WNOHANG);

    if (r == ctx->server_pid) {
        ctx->server_pid = -1; // server is gone
        handle_server_disconnect(ctx);
        log_client("SERVER DISCONNECTED handled\n");
    }
}

void setup_input(ClientContext *ctx, const char *note) {
    ctx->input_mode = INPUT_TEXT;
    ctx->text_len = 0;
    strncpy(ctx->text_note, note, sizeof(ctx->text_note));
}

bool connect_to_server(ClientContext *ctx) {
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ctx->server_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return false;
    }

    ctx->socket_fd = fd;
    return true;
}

// server must create socket before this returns true
// we will wait for that ... simplified readiness check for unix socket
// blocking call
bool wait_for_server_socket(const char *path, int timeout_ms) {
    const int step_ms = 50;
    int waited = 0;

    struct stat st;

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = step_ms * 1000000L;

    while (waited < timeout_ms) {
        if (stat(path, &st) == 0) {
            return true; // socket exists
        }

        nanosleep(&ts, NULL);
        waited += step_ms;
    }

    return false;
}


void disconnect_from_server(ClientContext *ctx) {
    if (ctx->socket_fd >= 0) {
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }
}

void handle_server_disconnect(ClientContext *ctx) {
    disconnect_from_server(ctx);

    if (ctx->mode == CLIENT_PLAYING || ctx->mode == CLIENT_PAUSED) {
        ctx->mode = CLIENT_MENU;
        snprintf(ctx->error_menu.txt_fields[0].text, sizeof(ctx->error_menu.txt_fields[0].text),
         "Error. Disconnected from server.");
        clear_menus_stack(&ctx->menus);
        menu_push(&ctx->menus, &ctx->error_menu);
    }

}

void init_main_menu(ClientContext *ctx) {
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

    init_menu_fields(&ctx->main_menu, buttons, LEN(buttons), NULL, 0, ctx);
}


void init_pause_menu(ClientContext *ctx) {
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

    init_menu_fields(&ctx->pause_menu, buttons, LEN(buttons), txt_fields, LEN(txt_fields), ctx);
}


void init_error_menu(ClientContext *ctx) {
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

    init_menu_fields(&ctx->error_menu, buttons, LEN(buttons), txt_fields, LEN(txt_fields), ctx);
}


void init_mode_select_menu(ClientContext *ctx) {
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

    init_menu_fields(&ctx->mode_select_menu, buttons, 3, txt_fields, 1, ctx);
}


void init_world_select_menu(ClientContext *ctx) {
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

    init_menu_fields(&ctx->world_select_menu, buttons, 3, txt_fields, 1, ctx);
}

void init_load_menu(ClientContext *ctx) {
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

    init_menu_fields(&ctx->load_menu, buttons, 3, txt_fields, 1, ctx);
}

void init_player_select_menu(ClientContext *ctx) {
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

    init_menu_fields(&ctx->player_select_menu, buttons, 3, txt_fields, 1, ctx);
}


void init_game_over_menu(ClientContext *ctx) {
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


    init_menu_fields(&ctx->game_over_menu, buttons, 3, txt_fields, 1, ctx);
}

void init_awaiting_menu(ClientContext *ctx) {
    static Button buttons[] = {
        {
            .text = "Cancel",
            .on_press = btn_cancel_awaiting,
            .user_data = NULL
        }
    };

    static TextField txt_fields[] = {
        {
            .text = "Awaiting server...", // for awaiting message
        }
    };

    init_menu_fields(&ctx->awaiting_menu, buttons, 1, txt_fields, 1, ctx);
}

static void init_menu_fields(Menu *menu, Button *buttons, const size_t b_count,
    TextField *txt_fields, const size_t t_count, ClientContext *ctx) {
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

void handle_game_key(ClientContext *ctx, const Key key) {
    if (key == KEY_QUIT || key == KEY_PAUSE || key == KEY_ESC) {
        ctx->mode = CLIENT_PAUSED;

        log_client("sending message paused to server\n");
        if (send_pause(ctx->socket_fd) < 0) {
            log_client("FAILED: to send paused\n");
        } else log_client("send\n");

        clear_menus_stack(&ctx->menus);
        snprintf(ctx->pause_menu.txt_fields[0].text, sizeof(ctx->pause_menu.txt_fields[0].text),
             "Your current score is: %d Time in game is: %d s", (int)ctx->game.score, ctx->game.player_time_elapsed);
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
    if (send_input(ctx->socket_fd, dir) < 0) {
        log_client("FAILED: to send direction\n");
    } else log_client("send\n");
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

            clear_menus_stack(&ctx->menus);
            menu_push(&ctx->menus, &ctx->game_over_menu);

            snprintf(ctx->game_over_menu.txt_fields[0].text, sizeof(ctx->game_over_menu.txt_fields[0].text),
                     "Game over. Your score is: %d Time in game is: %d s", (int)ctx->game.score, ctx->game.player_time_elapsed);
            log_client("msg game over received\n");
            break;
        case MSG_STATE: {
            ClientGameStateSnapshot new_state = {0};

            if (msg_to_state(&msg, &new_state) == 0) {
                snapshot_destroy(&ctx->game);   // free old
                ctx->game = new_state;          // move ownership  TODO by value so is it ok ??????
                log_client("game state updated\n");
            } else {
                snapshot_destroy(&new_state);
                log_client("FAILED: to parse state message\n");
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
        ctx->input_mode = INPUT_KEY;
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
        ctx->input_mode = INPUT_KEY;
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
            sleepn((long)timeout_ms * 1000000L); // 100 ms

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