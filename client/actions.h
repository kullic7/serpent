#ifndef SERPENT_ACTIONS_H
#define SERPENT_ACTIONS_H

// button actions/callbacks

void btn_resume_game(void *ctx_ptr);
void btn_exit_to_main_menu(void *ctx_ptr);
void btn_back_in_menu(void *ctx_ptr);

void btn_new_game(void *ctx_ptr);
void btn_exit_game(void *ctx_ptr);

void btn_single_player(void *ctx_ptr);
void btn_multiplayer(void *ctx_ptr);

void btn_time_mode(void *ctx_ptr);
void btn_standard_mode(void *ctx_ptr);

void btn_easy_world_mode(void *ctx_ptr);
void btn_hard_world_mode(void *ctx_ptr);

void btn_connect_to_server(void *ctx_ptr);

void btn_random_world(void *ctx_ptr);
void btn_load_from_file(void *ctx_ptr);

void btn_cancel_awaiting(void *ctx_ptr);

#endif //SERPENT_ACTIONS_H