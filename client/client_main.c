#include "client.h"

void *read_input_thread(void *arg) {
    const InputThreadArgs *args = arg;

    ClientInputQueue *queue = args->queue;
    const _Atomic bool *running = args->running;

    read_keyboard_input(queue, running);

    return NULL;
}

int main(int argc, char *argv[]) {
    ClientContext ctx;

    client_init(&ctx);

    ClientInputQueue cq;
    client_input_queue_init(&cq);

    ServerInputQueue sq;
    server_input_queue_init(&sq);

    _Atomic bool running = true;

    pthread_t input_thread;
    InputThreadArgs args = {&cq, &running};
    int rc = pthread_create(&input_thread, NULL, read_input_thread, &args);
    if (rc != 0) {
        // handle error
        client_cleanup(&ctx);
        return 1;
    }

    client_run(&ctx, &cq, &sq);
    running = false; // signal input thread to stop

    pthread_join(input_thread, NULL);
    client_cleanup(&ctx);

    client_input_queue_destroy(&cq);

    return 0;
}