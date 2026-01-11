#ifndef SERPENT_MENU_H
#define SERPENT_MENU_H

#include <stdlib.h>
#include "types.h"

typedef void (*ButtonAction)(void *user_data);

typedef struct {
    const char text[MENU_MAX_TEXT_LENGTH];

    ButtonAction on_press;
    void *user_data; // TODO pointer to client context usually

} Button;

typedef struct {

    size_t button_count;
    Button *buttons;

    size_t txt_fields_count;
    TextField *txt_fields;

    size_t selected_index;

} Menu;

// models a stack (hierarchy) of menus for navigation ... rendered is always the top menu
typedef struct {
    Menu *stack[MENU_STACK_MAX];
    size_t depth;
} MenuStack;


void init_menus_stack(MenuStack *ms);
void clear_menus_stack(MenuStack *ms);

void menu_push(MenuStack *ms, Menu *menu);
void menu_pop(MenuStack *ms);
Menu *menu_current(MenuStack *ms);


#endif //SERPENT_MENU_H