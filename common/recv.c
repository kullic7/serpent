#include "recv.h"

void recv_all(const int fd, void *buf, const size_t size) {
    size_t received = 0;
    while (received < size) {
        const ssize_t n = recv(fd, (char*)buf + received, size - received, 0);
        if (n <= 0) handle_disconnect();
        received += n;
    }
}