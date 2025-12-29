#include "client.h"
#include "renderer.h"
#include "actions.h"
#include "common/protocol.h"
#include <string.h>
#include <unistd.h>

void client_run(ClientContext *ctx, ClientInputQueue *iq, ServerInputQueue *sq) {
    while (ctx->running) {

        // TODO signal handling for graceful exit e.g. game time elapsed on server side
        // e.g. servers game over, here we need to recv
        // async processing of keyboard signals, when too many we drop them
        Key key;
        while (dequeue_key(iq, &key)) {

            if (ctx->input_mode == INPUT_TEXT) {
                handle_text_input(ctx, key);
                continue;
            }

            if (ctx->mode == CLIENT_MENU || ctx->mode == CLIENT_PAUSED) {
                Menu *menu = menu_current(&ctx->menus);
                handle_menu_key(menu, key);
            }
            else if (ctx->mode == CLIENT_PLAYING) {
                handle_game_key(key, ctx);
            }
        }

        // async processing of server messages; when queue is full secondary thread waits
        Message msg;
        while (dequeue_msg(sq, &msg)) {
            handle_server_msg(ctx, msg);
        }

        if (ctx->mode == CLIENT_MENU || ctx->mode == CLIENT_PAUSED || ctx->mode == CLIENT_GAME_OVER) {
            render_menu(menu_current(&ctx->menus), ctx->input_mode, ctx->text_note, ctx->text_buffer, ctx->text_len);
        }
        else if (ctx->mode == CLIENT_PLAYING) {
            //recv_state(ctx); TODO: receive game state from server ... blocking

            GameRenderState state = {
                .width = 40,
                .height = 20,
                .score = 100,
                .time_remaining = 25,
                .snake_count = 0,
                .snakes = NULL,
                .fruit_count = 0,
                .fruits = NULL,
                .obstacle_count = 0,
                .obstacles = NULL
            };
            ctx->score = (int)state.score;
            //render_game(&ctx->game);
            render_game(&state);
        }
    }

}

void client_init(ClientContext *ctx) {
    memset(ctx, 0, sizeof(*ctx));

    ctx->running = true;
    ctx->mode = CLIENT_MENU;
    ctx->socket_fd = -1; // Not connected yet

    ctx->input_mode = INPUT_GAME;

    ctx->score = 0;

    // todo probably explicit menu is not needed, can be selected from ctx
    init_main_menu(&ctx->main_menu, ctx);
    init_new_game_menu(&ctx->new_game_menu, ctx);
    init_multiplayer_menu(&ctx->multiplayer_menu, ctx);
    init_pause_menu(&ctx->pause_menu, ctx);
    init_mode_select_menu(&ctx->mode_select_menu, ctx);
    init_world_select_menu(&ctx->world_select_menu, ctx);
    init_load_menu(&ctx->load_menu, ctx);
    init_game_over_menu(&ctx->game_over_menu, ctx);
    init_awaiting_menu(&ctx->awaiting_menu, ctx);
    init_join_menu(&ctx->join_menu, ctx);

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
            .text = "Exit Game",
            .on_press = btn_exit_game,
            .user_data = NULL
        }
    };

    init_menu_fields(menu, buttons, 2, NULL, 0, ctx);
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

    init_menu_fields(menu, buttons, 2, txt_fields, 1, ctx);
}

void init_new_game_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            .text = "Single Player",
            .on_press = btn_single_player,
            .user_data = NULL
        },
        {
                .text = "Multi Player",
                .on_press = btn_multiplayer,
                .user_data = NULL
            },
            {
                .text = "Back",
                .on_press = btn_back_in_menu,
                .user_data = NULL
            }
    };

    init_menu_fields(menu, buttons, 3, NULL, 0, ctx);
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


void init_multiplayer_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            .text = "Create Game",
            .on_press = btn_create_game,
            .user_data = NULL
        },
        {
            .text = "Connect to Server",
            .on_press = btn_connect_to_server,
            .user_data = NULL
        },
        {
            .text = "Back",
            .on_press = btn_back_in_menu,
            .user_data = NULL
        }
    };

    init_menu_fields(menu, buttons, 3, NULL, 0, ctx);
}


void init_world_select_menu(Menu *menu, ClientContext *ctx) {
    static Button buttons[] = {
        {
            .text = "Easy Mode",
            .on_press = btn_easy_world_mode,
            .user_data = NULL
        },
        {
            .text = "Hard Mode", // with obstacles
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
            .text = "Awaiting server. Please wait...", // for awaiting message
        }
    };

    init_menu_fields(menu, buttons, 1, txt_fields, 1, ctx);
}

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
        clear_menus_stack(&ctx->menus);
        snprintf(ctx->pause_menu.txt_fields[0].text, sizeof(ctx->pause_menu.txt_fields[0].text),
             "Your current score is: %d", ctx->score);
        menu_push(&ctx->menus, &ctx->pause_menu);
    }

    // otherwise send direction to server
    //send_msg(MSG_INPUT); TODO
}

void handle_server_msg(ClientContext *ctx, const Message msg) {
    // todo process and set payloads accordingly
    switch (msg.header.type) {
        case MSG_READY:
            on_game_ready(ctx);
            break;
        case MSG_GAME_OVER:
            on_game_over(ctx);
            break;
        case MSG_STATE:
            ctx->mode = CLIENT_PLAYING;
            break;
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