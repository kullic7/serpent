#include "client.h"
#include <unistd.h>

int main(int argc, char *argv[]) {
    ClientContext ctx;

    client_init(&ctx);

    ClientInputQueue cq;
    client_input_queue_init(&cq);

    ServerInputQueue sq;
    server_input_queue_init(&sq);

    _Atomic bool running = true;
    bool error = false;

    pthread_t input_thread;
    InputThreadArgs args = {&cq, &running};
    const int rc = pthread_create(&input_thread, NULL, read_input_thread, &args);
    if (rc != 0) error = true;

    pthread_t recv_thread;
    ReceiveThreadArgs recv_args = {ctx.socket_fd, &sq, &running};
    const int rc2 = pthread_create(&recv_thread, NULL, recv_server_thread, &recv_args);
    if (rc2 != 0) error = true;

    if (!error) client_run(&ctx, &cq, &sq);

    running = false; // signal input thread to stop

    pthread_join(input_thread, NULL);
    pthread_join(recv_thread, NULL);

    client_cleanup(&ctx);

    client_input_queue_destroy(&cq);
    server_input_queue_destroy(&sq);

    return 0;
}