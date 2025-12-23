#include "client.h"
#include "menu.h"
#include "renderer.h"
#include "input.h"
#include <string.h>
#include <unistd.h>

void client_run(ClientContext *ctx, ClientInputQueue *input_queue) {
    while (ctx->running) {

        // TODO signal handling for graceful exit e.g. game time elapsed on server side
        // e.g. servers game over, here we need to recv
        Key key;

        while (dequeue_input(input_queue, &key)) {

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

        // todo game over signal should be received also in pause menu (e.g. time elapsed)
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
                .snake_count = 1,
                .snakes = NULL,
                .fruit_count = 2,
                .fruits = NULL,
                .obstacle_count = 3,
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
    //init_world_select_menu(&ctx->world_select_menu, ctx);
    //init_connect_menu(&ctx->connect_menu, ctx);
    init_game_over_menu(&ctx->game_over_menu, ctx);

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

