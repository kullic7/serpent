# Serpent - A Terminal Multiplayer Snake Game in C

Classical snake game with multiplayer support for unix implemented
in pure C with no additional dependencies.

- Multithreaded client-server architecture
- ASCII terminal rendering
- Keyboard only input
- Non-blocking server implementation (via event queues)
- Custom lightweight communication protocol over Unix domain sockets

## Installation

(Only for Unix-like systems)

**Building from source**
```bash
mkdir build
cd build
cmake ..
make
```

**Memory check**

```bash
make memcheck
```

##  Architecture

Client and server are implemented as separate programs communicating
over Unix domain sockets. Both the client and the server are multithreaded
applications and use thread-safe queues for inter-thread communication.

### Client

The client consists of three threads and is designed as a responsive
state machine.

*Main thread - rendering & state machine*:
- reads `Message`s and `Key`s from corresponding queues)
- interprets `Message`s from server (`ClientGameStateSnapshot`)
- interprets `Key`s from keyboard
- updates the client state machine
- sends appropriate `Message`s to server
- renders ASCII grid/menu/game


*Input thread - keyboard input*:
- reads keyboard (blocking call)
- pushes received `Key`s to `ClientInputQueue`


*RecvInput thread - incoming server messages*:
- reads socket (blocking call)
- pushes received `Message`s to `ServerInputQueue`


### Server

The server consists of multiple threads and is designed as an authoritative
game loop managing multiple clients. Inter-thread communication is done
via thread-safe queues which allows the server to be non-blocking, simplifies memory management,
and avoids common issues such as race conditions.

*Main thread - authoritative game loop*:
- creates and initializes the server socket
- accepts very first client connection (with a 10-second timeout)
- runs game loop where on every tick it:
  - broadcasts snapshot of game state to all connected clients
  - handles input events
  - updates game state
  - determines whether the game has ended over and, if so, broadcasts game-over message
- spawns accept thread (in multiplayer mode only)

*Worker thread - Actions executor*:
- reads `Action`s from `ActionQueue`
- executes `Action`s which are meant to be possibly blocking calls, such as sending messages to clients
- if needed responses with `Event`s pushed to the main thread's `EventQueue`
- spawns detached threads for blocking calls when needed

*N-times ClientInput thread - incoming client messages*:
- reads socket (blocking call)
- translates `Message`s into `Event`s
- pushes `Event`s to the main thread's `EventQueue`

*Accept thread - multiplayer only accept loop*:
- accepts new client connections (blocking call)
- spawns new ClientInput thread for each new client
- registers new clients to thread-safe `ClientRegistry`

*ResumeWait thread - optional*:
- executes blocking sleep after player is resumed
- responses with `Event` to main thread when sleep is over

*EndWait thread - optional*:
- executes blocking sleep after game over in multiplayer mode so we wait before shutting down
- responses with `Event` to main thread when sleep is over


### Communication Protocol
The communication protocol between the client and the server is based on
sending and receiving `Message` structures over Unix domain sockets.
The protocol is lightweight and tailored specifically to the needs of the game.

Unix domain sockets implicitly assume that both the client and the server
run on the same machine, which enables low-latency communication and
simplifies the implementation. This allows the protocol to avoid concerns
such as endianness conversion and cross-architecture compatibility.

To ensure correct message framing over the byte stream, each message is
prefixed with a fixed-size header that specifies the message type and the
size of the payload.

## Possible Improvements

- Client's main thread should not block at all (currently send_message() may block) so we should utilize
  similar idea as on server particularly worker thread with event queues.
- Move towards TCP sockets for networked multiplayer support.
