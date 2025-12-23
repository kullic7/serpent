#include "menu.h"
#include "common/config.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

void init_menus_stack(MenuStack *ms) {
    ms->depth = 0;
}

void clear_menus_stack(MenuStack *ms) {
    ms->depth = 0;
    for (size_t i = 0; i < MENU_STACK_MAX; i++) {
        ms->stack[i] = NULL;
    }
}

void menu_push(MenuStack *ms, Menu *menu) {
    if (ms->depth < MENU_STACK_MAX) {
        ms->stack[ms->depth++] = menu;
    }
}

void menu_pop(MenuStack *ms) {
    if (ms->depth > 1) { // prevent popping the main menu
        ms->stack[ms->depth--] = NULL;
    }
}

Menu *menu_current(MenuStack *ms) {
    if (ms->depth <= 0) return NULL;
    return ms->stack[ms->depth - 1];
}

// main menu

void btn_new_game(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    menu_push(&ctx->menus, &ctx->new_game_menu);
}

void btn_exit_game(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    ctx->running = false;
}


void init_main_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            {},
            .text = "New Game",
            .on_press = btn_new_game,
            .user_data = NULL
        },
        {
            {},
            .text = "Exit Game",
            .on_press = btn_exit_game,
            .user_data = NULL
        }
    };

    init_menu_fields(menu, buttons, 2, NULL, 0, ctx);
}

// pause menu

void btn_resume_game(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    menu_pop(&ctx->menus);
    ctx->mode = CLIENT_PLAYING;
}

void btn_exit_to_main_menu(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;

    /*
    if (ctx->socket_fd >= 0) {
        send_message(MSG_LEAVE, ctx, 0); // notify server
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }
    */

    ctx->mode = CLIENT_MENU;
    clear_menus_stack(&ctx->menus);
    menu_push(&ctx->menus, &ctx->main_menu);
}

void init_pause_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            {},
            .text = "Resume Game",
            .on_press = btn_resume_game,
            .user_data = NULL
        },
        {
            {},
            .text = "Exit to Main Menu",
            .on_press = btn_exit_to_main_menu,
            .user_data = NULL
        }
    };

    static TextField txt_fields[] = {
        {
            {{BUTTON_WIDTH, BUTTON_HEIGHT}, {10, 15}},
            .text = "", // for score message
        }
    };

    init_menu_fields(menu, buttons, 2, txt_fields, 1, ctx);
}


// new game menu

void btn_create_game(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    menu_push(&ctx->menus, &ctx->mode_select_menu);
}

void btn_multiplayer(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    menu_push(&ctx->menus, &ctx->multiplayer_menu);
}

void btn_back_in_menu(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    menu_pop(&ctx->menus);
}

void init_new_game_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            {},
            .text = "Single Player",
            .on_press = btn_create_game,
            .user_data = NULL
        },
        {
            {},
            .text = "Multi Player",
            .on_press = btn_multiplayer,
            .user_data = NULL
        },
        {
            {},
            .text = "Back",
            .on_press = btn_back_in_menu,
            .user_data = NULL
        }
    };

    init_menu_fields(menu, buttons, 3, NULL, 0, ctx);
}

// mode select menu

void btn_standard_mode(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;
    //ctx->game_config.time_limit = 10; TODO set othr config options
    menu_push(&ctx->menus, &ctx->world_select_menu);
}

void on_time_entered(void *ctx_ptr, const char *text) {
    const int seconds = strtol(text, NULL, 10);
    ClientContext *ctx = ctx_ptr;
    //ctx->game_config.time_limit = seconds; TODO
    printf("%d", seconds);

    menu_push(&ctx->menus, &ctx->world_select_menu);
}

void btn_time_mode(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;

    ctx->input_mode = INPUT_TEXT;
    ctx->text_len = 0;
    strncpy(ctx->text_note, "Time Limit (seconds): ", sizeof(ctx->text_note));

    ctx->on_text_submit = on_time_entered;
}

void init_mode_select_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            {},
            .text = "Standard Mode",
            .on_press = btn_standard_mode,
            .user_data = NULL
        },
        {
                {},
                .text = "Timed Mode",
                .on_press = btn_time_mode,
                .user_data = NULL
            },
            {
                {},
                .text = "Back",
                .on_press = btn_back_in_menu,
                .user_data = NULL
            }
    };

    init_menu_fields(menu, buttons, 3, NULL, 0, ctx);
}

// multiplayer menu

void on_socket_path_entered(void *ctx_ptr, const char *path) {
    ClientContext *ctx = ctx_ptr;
    strncpy(ctx->server_path, path, sizeof(ctx->server_path));
    connect_to_server(ctx);
    ctx->mode = CLIENT_PLAYING;
}


void btn_connect(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;

    ctx->input_mode = INPUT_TEXT;
    ctx->text_len = 0;
    strncpy(ctx->text_note, "Server address (socket path): ", sizeof(ctx->text_note));

    ctx->on_text_submit = on_socket_path_entered;
}

void init_multiplayer_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            {},
            .text = "Create Game",
            .on_press = btn_create_game,
            .user_data = NULL
        },
        {
                {},
                .text = "Connect to Server",
                .on_press = btn_connect,
                .user_data = NULL
            },
            {
                {},
                .text = "Back",
                .on_press = btn_back_in_menu,
                .user_data = NULL
            }
    };

    init_menu_fields(menu, buttons, 3, NULL, 0, ctx);
}

// world select menu


void init_world_select_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            {},
            .text = "Easy World",
            .on_press = btn_create_game,
            .user_data = NULL
        },
        {
            {},
            .text = "Hard Mode", // with obstacles
            .on_press = btn_connect,
            .user_data = NULL
        },
        {
            {},
            .text = "Back",
            .on_press = btn_back_in_menu,
            .user_data = NULL
        }
    };

    init_menu_fields(menu, buttons, 3, NULL, 0, ctx);
}

// game over menu

void init_game_over_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            {},
            .text = "Play Again",
            .on_press = btn_new_game,
            .user_data = NULL
        },
        {
            {},
            .text = "Exit to Main Menu",
            .on_press = btn_exit_to_main_menu,
            .user_data = NULL
        },
        {
            {},
            .text = "Exit Game",
            .on_press = btn_exit_game,
            .user_data = NULL
        }

    };

    static TextField txt_fields[] = {
        {
            {{BUTTON_WIDTH, BUTTON_HEIGHT}, {10, 15}},
            .text = "", // for game over message
        }
    };


    init_menu_fields(menu, buttons, 3, txt_fields, 1, ctx);
}

// helpers

void handle_game_over(void *ctx_ptr) {
    ClientContext *ctx = ctx_ptr;

    ctx->mode = CLIENT_GAME_OVER;
    ctx->text_len = 0;

    snprintf(ctx->game_over_menu.txt_fields[0].text, sizeof(ctx->game_over_menu.txt_fields[0].text),
             "Game over. Your score is: %d", ctx->score);

}


static void init_menu_fields(Menu *menu, Button *buttons, size_t b_count,
    TextField *txt_fields, size_t t_count, ClientContext *ctx) {
    for (size_t i = 0; i < b_count; ++i) {
        buttons[i].user_data = ctx;
        buttons[i].box.pos.x = 100;
        buttons[i].box.pos.y = 20 + (int)i * 2;
        buttons[i].box.size.width = BUTTON_WIDTH;
        buttons[i].box.size.height = BUTTON_HEIGHT;
    }

    menu->buttons = buttons;
    menu->button_count = b_count;

    menu->txt_fields = txt_fields;
    menu->txt_fields_count = t_count;

    menu->selected_index = 0;
}

void handle_text_input(ClientContext *ctx, const Key key) {

    // handle escape
    if (key == KEY_ESC) {
        ctx->input_mode = INPUT_GAME;
        ctx->text_len = 0;
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
        }
        return;
    }

    // append character to buffer
    if (ctx->text_len < sizeof(ctx->text_buffer) - 1 &&
        key >= 32 && key <= 126) {
        ctx->text_buffer[ctx->text_len++] = (char)key;
        }
}

void connect_to_server(ClientContext *ctx) {
    // TODO: Implement actual Unix domain socket connection
    // struct sockaddr_un addr;
    // ctx->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    // addr.sun_family = AF_UNIX;
    // strncpy(addr.sun_path, ctx->server_path, sizeof(addr.sun_path) - 1);
    // connect(ctx->socket_fd, (struct sockaddr*)&addr, sizeof(addr));

    ctx->socket_fd = 1; // dummy socket fd for now
}


void pause_game(ClientContext *ctx) {
    if (ctx->mode == CLIENT_PLAYING) {
        ctx->mode = CLIENT_PAUSED;
    } else if (ctx->mode == CLIENT_PAUSED) {
        ctx->mode = CLIENT_PLAYING;
    }
}








