#include "client.h"
#include "input.h"


void *read_input_thread(void *arg) {
    const InputThreadArgs *args = arg;

    ClientInputQueue *queue = args->queue;
    const _Atomic bool *running = args->running;

    read_input(queue, running);

    return NULL;
}

int main(int argc, char *argv[]) {
    ClientContext ctx;

    client_init(&ctx);

    ClientInputQueue queue = {0};

    _Atomic bool running = true;

    pthread_t input_thread;
    InputThreadArgs args = {&queue, &running};
    int rc = pthread_create(&input_thread, NULL, read_input_thread, &args);
    if (rc != 0) {
        // handle error
        client_cleanup(&ctx);
        return 1;
    }

    client_run(&ctx, &queue);
    running = false; // signal input thread to stop

    pthread_join(input_thread, NULL);
    client_cleanup(&ctx);

    return 0;
}
