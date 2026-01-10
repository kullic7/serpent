#ifndef SERPENT_RENDERER_H
#define SERPENT_RENDERER_H

#include <stdlib.h>
#include "types.h"
#include "menu.h"
#include "input.h"

typedef struct {
    int cols;
    int rows;
} TermSize;


void render_menu(const Menu *menu, InputMode input_mode, const char *text_note,
    const char *text_buffer, size_t text_len);
void render_game(ClientGameStateSnapshot state);

void term_clear(void);
void term_home(void);
void term_hide_cursor(void);
void term_show_cursor(void);

void draw_text(int x, int y, const char *text);
void draw_box(int x, int y, int w, int h);

#endif //SERPENT_RENDERER_H