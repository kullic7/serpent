#ifndef SERPENT_RECV_H
#define SERPENT_RECV_H

#include <unistd.h>

// blocking
/*
* If there is data:

it copies bytes into buf

returns number of bytes read

If there is NO data yet:

👉 the thread SLEEPS

it does NOT spin

it does NOT consume CPU

OS wakes it when data arrives
 */
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
void recv_all(int fd, void *buf, size_t size);

// Example usage:
/*
recv_all(fd, &header, sizeof(header));
recv_all(fd, buffer, header.length);
*/



#endif //SERPENT_RECV_H