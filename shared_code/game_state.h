#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "ipc_messages.h"
#include <stdint.h>

// konstanty len na nejake pevne limity pre polia
#define MAX_PLAYERS 100
#define MAX_SNAKE_LEN 256
#define MAX_FRUITS MAX_PLAYERS

//  sposob ukladania si suradnic na mape
typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    int active;                 // 0/1 je alebo nieje v hre
    int paused;                 // 0/1 je alebo nieje pozastaveny
    int score;
    Direction dir;              // funguje s enum direction ako smer
    int len;
    Point seg[MAX_SNAKE_LEN];   // seg[0] = hlava

    // casove statistiky pre pouzivatela na vypis
    long long join_ms;          // kedy sa had objavil
    long long death_ms;         // kedy zomrel
    long long total_alive_ms;   // suma času v hre
    long long resume_at_ms;     // cas pouzi
} Snake;

typedef struct {
    int width;
    int height;

    int running;            // 1 = hra  0 = nehra
    int game_over;          // 1 = koniec celej hry
    int multiplayer;        // 0/1 jeden alebo viac hracov

    GameMode mode;
    int time_limit_sec;     // limit casu pre herny mode

    long long start_ms;         // začiatok hry
    long long empty_since_ms;   // 0 je prazdny -1 nieje

    int has_obstacles;          // 0/1 ci je alebo nieje prekazka na mape
    uint8_t *obstacles;         // 0/1 je nieje prekazka

    Snake snakes[MAX_PLAYERS];  // vsetky hady

    int fruit_count;
    Point fruits[MAX_FRUITS];
} GameState;

//  Nastavenie hry a vymazanie hry
void gs_init(GameState *gs, int w, int h, int multiplayer);
void gs_destroy(GameState *gs);

//  Nastavovanie aky chceme rezim hry
void gs_set_mode(GameState *gs, GameMode mode, int time_limit_sec);

//  Sprava na generovanie prekazok
int gs_generate_obstacles_random(GameState *gs, int percent);

//  Pridanie a vymazanie hada
int  gs_add_snake(GameState *gs);      // vráti id hada alebo -1
void gs_remove_snake(GameState *gs, int id);

//  Ovocko
void gs_ensure_fruits(GameState *gs);

//  Tick
void gs_tick(GameState *gs, long long now_ms);

// Pomocne funkcie na bunky po nasej mapke
int gs_cell_occupied_by_snake(const GameState *gs, int x, int y);
int gs_cell_has_fruit(const GameState *gs, int x, int y);
int gs_is_obstacle(const GameState *gs, int x, int y);

#endif