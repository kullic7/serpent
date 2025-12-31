#include "menu.h"
#include "config.h"
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








