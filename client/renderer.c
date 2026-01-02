#include "config.h"
#include "renderer.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "timer.h"


void term_clear(void) {
    printf("\033[2J");
}

void term_home(void) {
    printf("\033[H");
}

void term_hide_cursor(void) {
    printf("\033[?25l");
}

void term_show_cursor(void) {
    printf("\033[?25h");
}

int term_get_size(TermSize *out) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return -1; // error
    }
    out->cols = ws.ws_col;
    out->rows = ws.ws_row;
    return 0;
}

void draw_text(const int x, const int y, const char *text) {
    printf("\033[%d;%dH%s", y, x, text);
}

void draw_box(const int x, const int y, const int w, const int h) {
    for (int i = 0; i < w; ++i) {
        draw_text(x + i, y,     "-");
        draw_text(x + i, y + h, "-");
    }
    for (int i = 0; i <= h; ++i) {
        draw_text(x,     y + i, "-");
        draw_text(x + w, y + i, "-");
    }
}

void render_menu(const Menu *menu, const InputMode input_mode, const char *text_note, const char *text_buffer, const size_t text_len) {

    TermSize ts;
    term_get_size(&ts);

    term_clear();
    term_home();
    term_hide_cursor();

    int x_pos = ts.cols / 2 - 6;
    int y_pos = ts.rows / 3;

    draw_text(x_pos, 2, "___S E R P E N T___");
    draw_text(x_pos - 2, 3, "The Terminal Snake Game");

    if (input_mode == INPUT_TEXT) {

        draw_text(ts.cols / 7, y_pos - 1, text_note);
        printf("\033[%d;%dH%.*s", y_pos, ts.cols / 7, (int)text_len, text_buffer);
        draw_text(ts.cols / 7 + (int)text_len, y_pos, "_");
    }

    // render menu buttons
    for (size_t i = 0; i < menu->button_count; ++i) {
        const Button *b = &menu->buttons[i];

        if (i == menu->selected_index)
            draw_text(x_pos - 2, y_pos + (int)i * 2, ">");

        draw_text(x_pos, y_pos + (int)i * 2, b->text);
    }

    // render menu text fields
    for (size_t i = 0; i < menu->txt_fields_count; ++i) {
        const TextField *t = &menu->txt_fields[i];

        draw_text(ts.cols / 20, y_pos / 3, t->text);
    }

    fflush(stdout);

    sleep_frame(FRAME_TIME_MS);
}


void render_game(const GameRenderState state) {
    TermSize ts;
    term_get_size(&ts);

    term_clear();
    term_home();
    term_hide_cursor();

    // draw borders
    draw_box(75, 5, state.width + 1, state.height + 1);

    // draw score and time
    draw_text(2, 1, "Score:");
    char score_str[32];
    snprintf(score_str, sizeof(score_str), "%zu", state.score);
    draw_text(9, 1, score_str);

    if (state.time_remaining >= 0) {
        draw_text(2, 2, "Remaining time:");
        char time_str[64];
        snprintf(time_str, sizeof(time_str), "%ds", state.time_remaining);
        draw_text(18, 2, time_str);
    }

    // draw fruits
    for (size_t i = 0; i < state.fruit_count; ++i) {
        const Position fruit = state.fruits[i];
        draw_text(fruit.x + 2, fruit.y + 2, FRUIT_CHAR);
    }

    // draw obstacles
    for (size_t i = 0; i < state.obstacle_count; ++i) {
        const Obstacle obstacle = state.obstacles[i];
        draw_text(obstacle.pos.x + 2, obstacle.pos.y + 2, OBSTACLE_CHAR);
    }

    // draw snakes
    for (size_t i = 0; i < state.snake_count; ++i) {
        const Position head = state.snakes[i].body[0];
        draw_text(head.x + 2, head.y + 2, "O");
        for (size_t j = 0; j < state.snakes[i].length; ++j) {
            const Position body = state.snakes[i].body[j];
            draw_text(body.x + 2, body.y + 2, SNAKE_CHAR);
        }
    }

    fflush(stdout);

    sleep_frame(FRAME_TIME_MS);
}


