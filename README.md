# Serpent - Multiplayer Snake Game

`client = player = snake controlled by user`

- obstacles and snakes have same model
- fruit is just a position
- only server's gam loop thread modifies game state.

## Project Structure


```
serpent/
├── CMakeLists.txt          (local dev only)
├── Makefile                (final, for frios2)
│
├── common/
│   ├── protocol.h          // message types, structs
│   ├── protocol.c
│   ├── game_types.h        // Snake, Fruit, Position
│   └── config.h            // constants
│
├── server/
│   ├── server_main.c
│   ├── server.h
│   ├── server.c
│   ├── game_state.h
│   ├── game_state.c
│   ├── physics.c           // collisions, movement
│   └── client_registry.c   // connected clients
│
├── client/
│   ├── client_main.c
│   ├── client.h
│   ├── client.c
│   ├── renderer.c
│   └── input.c
│
└── README.md
```

## Client-Server Architecture

### Client
*Main thread - renderer - state machine*:
- receives game state
- renders ASCII grid
```
render_thread
 └── recv(MSG_STATE)
       draw()

```

*Input thread - acts also as main because we need right away info*:
- reads keyboard
- sends input messages to server
```
input_thread
└── read key
send(MSG_INPUT)

input_thread
 └── while (running)
       key = read_key()   // blocks
       push key into queue


```

### Server

*Main thread*:
- creates socket
- accept() loop
- registers clients
- spawns per-client threads (optional)
```
server_main
 └── accept loop

```

*Game loop thread*:
- ticks every X ms
- moves snakes
- checks collisions
- updates fruits
- sends state to all clients
```
game_loop_thread
 └── while (running)
       lock(queue)
       copy events
       clear input queue
       unlock(queue)

       apply inputs
       update positions
       detect collisions
       broadcast state

       sleep(TICK)


```

*Client handled threads*:
- receives keyboard input
- updates snake direction
```
client_handler_thread
 └── recv(MSG_INPUT)
       lock(queue)
       push input event
       signal(has_data)
       unlock(queue)


```


### Communication Protocol
- MSG_REGISTER: client -> server
- MSG_INPUT: client -> server
- MSG_STATE: server -> client
#### Lifecycle
- ONE socket connection per client
- REUSED for entire session
- Sockets are persistent streams
```
client:
socket()
connect()
────────────────────────────────
send input messages (many times)
recv state updates (many times)
────────────────────────────────
close()
```

#### Flow
```
Client                    Server
  | ---- MSG_JOIN ------> |
  |                       |
  | ---- MSG_INPUT -----> |
  |                       |
  | <--- MSG_STATE ----- |
  |                       |
  | ---- MSG_INPUT -----> |
  | <--- MSG_STATE ----- |

```
