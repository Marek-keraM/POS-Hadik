#ifndef IPC_MESSAGES_H
#define IPC_MESSAGES_H

#include <stdint.h>
#include <unistd.h>

//  Cesta unix socket
#define SNAKE_SOCK_PATH "/tmp/snake_server.sock"

//  Rezim hry
typedef enum {
    MODE_STANDARD = 1,   // konci po 10 s od opojenia hraca
    MODE_TIMED = 2       // konci po skonceni casu
} GameMode;

//  Druh herneho stylu
typedef enum {
    WORLD_EMPTY = 1,     // bez nejakych prekazok v hre alebo okolo
    WORLD_OBSTACLES = 2, // nahodne generovane prekazky
    WORLD_FROM_FILE = 3  // po precitani suboru
} WorldType;

//  Smer hada
typedef enum {
    DIR_UP = 0,
    DIR_RIGHT = 1,
    DIR_DOWN = 2,
    DIR_LEFT = 3
} Direction;

// Prikazy co posiela klient na server
typedef enum {
    C_NEW_GAME = 1,         // nova hra
    C_JOIN_GAME = 2,        // chce sa pripojit
    C_PAUSE = 3,            // pozastavenie
    C_RESUME = 4,           // spustenie
    C_QUIT = 5,             // odpojenie hraca
    C_INPUT = 6,            // smer hraca ovladanie
    C_GET_STATE = 7         // aktualny stav hry
} ClientCmd;

// Prikazy server na klient
typedef enum {
    S_OK = 100,             // vsetko ok
    S_ERROR = 101,          // chyba
    S_FREEZE = 102,         // pozastav
    S_STATE_HEADER = 110,   // informacie herneho stavu ako sirka vyska cas
    S_STATE_CELL = 111      // bunka mapy ako ascii
} ServerCmd;

//  IPC spravy
typedef struct {
    int32_t cmd;   // client alebo server cmd
    int32_t a;
    int32_t b;
    int32_t c;
} IpcMessage;

//  Standartne parametre novej hry
typedef struct {
    GameMode mode;
    WorldType world;
    int32_t width;
    int32_t height;
    int32_t time_limit_sec;     // len pre casovany mod
    int32_t multiplayer;        // je aleob nieje pre jedneho hraca
} NewGameParams;


//  Spravy na posielanie a prijmanie IPC sprav
int send_ipc_message(int fd, const IpcMessage *msg);
int recv_ipc_message(int fd, IpcMessage *msg);

int send_new_game_params(int fd, const NewGameParams *params);
int recv_new_game_params(int fd, NewGameParams *params);

#endif