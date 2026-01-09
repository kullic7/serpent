#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/un.h>
#include <signal.h>
#include "registry.h"
#include "server.h"
#include "logging.h"
#include "game.h"

int main(int argc, char **argv) {

    // globally ignore SIGPIPE to avoid crashes when writing to closed sockets
    // this is desired on servers
    signal(SIGPIPE, SIG_IGN);

    // game configuration comes as command line arguments
    // --------------------------------------------------------
    // todo maybe game/world size ... but how to sync with client ?
    const char *socket_path = argc > 1 ? argv[1] : NULL;
    const bool single_player = argc > 2 ? argv[2][0] == '1' : true;
    char *endptr = NULL;
    const long tmp_game_time = argc > 3 ? strtol(argv[3], &endptr, 10) : -1;
    const int game_time = (endptr && *endptr == '\0') ? (int)tmp_game_time : -1; // default no limit (standard mode) ... game over after 10 sec when last player left
    const bool obstacles_enabled = argc > 4 ? argv[4][0] == '1' : false; // default easy world
    const bool random_world = argc > 5 ? argv[5][0] == '1' : true; // if hard world default is random obstacles
    const char *obstacles_file_path = argc > 6 ? argv[6] : NULL;

    log_server(" ------------ Server started ----------- \n");
    log_server(" ------------ Args ----------- \n");
    char buf[64];
    snprintf(buf, sizeof buf, "socket path %s\n", socket_path);
    log_server(buf);
    snprintf(buf, sizeof buf, "single player %d\n", single_player ? 1 : 0);
    log_server(buf);
    snprintf(buf, sizeof buf, "game time %d\n", game_time);
    log_server(buf);
    snprintf(buf, sizeof buf, "obstacles enabled %d\n", obstacles_enabled ? 1 : 0);
    log_server(buf);
    snprintf(buf, sizeof buf, "random world %d\n", random_world ? 1 : 0);
    log_server(buf);
    snprintf(buf, sizeof buf, "obstacles file path %s\n", obstacles_file_path ? obstacles_file_path : "NULL");
    log_server(buf);
    log_server(" ------------ ---- ----------- \n");

    if (socket_path == NULL) {
        fprintf(stderr, "socket_path is NULL\n");
        fprintf(stderr, "Usage: %s <socket_path> [single_player(1|0)] [game_time_seconds] [obstacles_enabled(1|0)] [random_world(1|0)] [obstacles_file_path]\n", argv[0]);
        exit(1);
    }


    // handle connections + input receiving
    // --------------------------------------------------------
    const int listen_fd = setup_server_socket(socket_path);

    if (listen_fd < 0) {
        log_server("FAILED: to set up server socket\n");
        exit(1); // client fails on timeout
    }

    ClientRegistry registry;
    registry_init(&registry);

    _Atomic bool accepting = true;
    _Atomic bool running = true;
    _Atomic bool error = false;

    EventQueue events;
    event_queue_init(&events);

    ActionQueue actions;
    action_queue_init(&actions);

    pthread_t accept_thread;
    RecvInputThreadArgs recv_args = {&events, -1, &running}; // client_fd will be set in accept_connection
    AcceptLoopThreadArgs args = {&registry, listen_fd, &accepting, &recv_args};

    // accept very first connection in main thread to avoid race
    if (accept_connection(&registry, listen_fd, &recv_args) == -1) error=true; // blocking call; accept one and return
    if (single_player) {
        log_server("not accepting any more messages \n");
        close(listen_fd);
        unlink(socket_path);
    } else {
        log_server("MULTIPLAYER\n");
        // in multiplayer spawn accept thread

        const int rc = pthread_create(&accept_thread, NULL, accept_loop_thread, &args);
        if (rc != 0) {
            // handle error
            log_server("FAILED: to start accepting THREAD \n");
            close(listen_fd);
            unlink(socket_path);
            exit(1); // client fails on timeout
        }
    }

    // worker thread
    // --------------------------------------------------------
    pthread_t worker_thread;
    ActionThreadArgs worker_args = {&events, &actions, &registry, &running};
    const int rc = pthread_create(&worker_thread, NULL, action_thread, &worker_args);
    if (rc != 0) {
        // handle error
        log_server("FAILED: to start worker THREAD \n");
        running = false;
        pthread_join(worker_thread, NULL);
        close(listen_fd);
        unlink(socket_path);
        exit(1); // client fails on timeout on awaiting server MSG_READY signal
    }

    // game loop
    // --------------------------------------------------------

    GameState state;
    game_init(&state, 40, 20, game_time, obstacles_enabled, random_world, obstacles_file_path); // default size 40x20

    game_run(game_time >= 0, single_player, &state, &events, &actions);


    // shutdown
    // --------------------------------------------------------
    log_server("game loop ended\n");

    broadcast_game_over(&registry); // must be done here before shutdown so we are sure all clients get it (its blocking)
    log_server("game over broadcasted to clients\n");

    accepting = false;
    running = false;

    game_destroy(&state);

    log_server("game destroyed\n");

    pthread_join(worker_thread, NULL);
    log_server("worker thread joined\n");

    registry_destroy(&registry); // joins all client threads
    log_server("registry destroyed (client recv input thread should be joined)\n");

    event_queue_destroy(&events);
    action_queue_destroy(&actions);
    log_server("event queues destroyed\n");

    if (!single_player) {
        pthread_join(accept_thread, NULL);
        close(listen_fd);
        unlink(socket_path);
    }

    log_server(" ---------- Server shut down ------------ \n");
    return 0;

    }
