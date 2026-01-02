#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/un.h>

#include "registry.h"
#include "server.h"
#include "logging.h"


int main(int argc, char **argv) {
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
        log_server("failed to set up server socket\n");
        exit(1); // client fails on timeout
    }

    ClientRegistry registry;
    registry_init(&registry);
    _Atomic bool accepting = true;

    if (single_player) {
        // in single player no extra thread needed
        accept_connection(&registry, listen_fd); // blocking call; accept one and return
        log_server("not accepting any more messages \n");
        close(listen_fd);
        unlink(socket_path);
    } else {
        // in multiplayer spawn accept thread
        pthread_t accept_thread;
        AcceptLoopThreadArgs args = {&registry, listen_fd, &accepting};
        const int rc = pthread_create(&accept_thread, NULL, accept_loop_thread, &args);
        if (rc != 0) {
            // handle error
            close(listen_fd);
            unlink(socket_path);
            exit(1); // client fails on timeout
        }
    }

    // worker thread
    // --------------------------------------------------------
    _Atomic bool running = true;
    //pthread_t worker_thread;
    //WorkerThreadArgs worker_args = {&registry, &running, game_time};
    //const int rc = pthread_create(&worker_thread, NULL, game_worker_thread, &worker_args);
    //if (rc != 0) {
    //    // handle error
    //    running = false;
    //    pthread_join(accept_thread, NULL);
    //    close(listen_fd);
    //    unlink(socket_path);
    //    exit(1); // client fails on timeout on awaiting server MSG_READY signal
    //}

    // game loop
    // --------------------------------------------------------

    //game_init(&registry, game_time, obstacles_enabled, random_world, obstacles_file_path);
    //game_loop(&registry, inq, outq);


    const time_t start = time(NULL);
    if (start == (time_t)-1) {
        perror("time");
        return 1;
    }

    while (difftime(time(NULL), start) < 60.0) {
        // main server loop doing nothing, just for testing
        sleep(1);
    }

    registry_destroy(&registry);
    if (!single_player) {
        accepting = false;
        close(listen_fd);
        unlink(socket_path);
    }
    log_server(" ---------- Server shut down ------------ \n");
    return 0;

    }
