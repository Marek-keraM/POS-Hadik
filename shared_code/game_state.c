#include "game_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//  POMOCNE FUNKCIE

static int idx_of(const GameState *gs, int x, int y) {
    return y * gs->width + x;
}

//  Kapacita pola seg
static int snake_seg_capacity(const Snake *s) {
    return (int)(sizeof(s->seg) / sizeof(s->seg[0]));
}

//  Kapacita ovocia
static int fruits_capacity(const GameState *gs) {
    return (int)(sizeof(gs->fruits) / sizeof(gs->fruits[0]));
}

// Overenie ci bunka je prekazka
int gs_is_obstacle(const GameState *gs, int x, int y) {
    if (!gs->obstacles) return 0; // len svet bez prekazok
    if (x < 0 || y < 0 || x >= gs->width || y >= gs->height) return 1;
    return gs->obstacles[idx_of(gs, x, y)] ? 1 : 0;
}

// Spocitanie hadov/ hracov v hre
static int count_active_snakes(const GameState *gs) {
    int c = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (gs->snakes[i].active) c++;
    }
    return c;
}

// Ci na bunke je ovocie
static int fruit_exists(const GameState *gs, int x, int y) {
    for (int i = 0; i < gs->fruit_count; i++) {
        if (gs->fruits[i].x == x && gs->fruits[i].y == y) return 1;
    }
    return 0;
}

// Ci uz na bunke je had
static int cell_occupied_by_snake(const GameState *gs, int x, int y) {
    for (int s = 0; s < MAX_PLAYERS; s++) {
        if (!gs->snakes[s].active) continue;
        for (int j = 0; j < gs->snakes[s].len; j++) {
            if (gs->snakes[s].seg[j].x == x && gs->snakes[s].seg[j].y == y) return 1;
        }
    }
    return 0;
}

// Dosiahnutelnost ovocia pre hadika
static void bfs_fill(const GameState *gs, int sx, int sy, unsigned char *out) {
    int w = gs->width;
    int h = gs->height;
    int n = w * h;
    memset(out, 0, (size_t)n);  // vynuluje vsetko nedosiahnutelne
    if (sx < 0 || sy < 0 || sx >= w || sy >= h) return;
    if (gs_is_obstacle(gs, sx, sy)) return;

    int *qx = (int *)malloc((size_t)n * sizeof(int));
    int *qy = (int *)malloc((size_t)n * sizeof(int));
    if (!qx || !qy) { free(qx); free(qy); return; }

    int head = 0, tail = 0;
    qx[tail] = sx; qy[tail] = sy; tail++;
    out[idx_of(gs, sx, sy)] = 1;

    while (head < tail) {
        int x = qx[head];
        int y = qy[head];
        head++;

        const int dx[4] = { 1, -1, 0, 0 };
        const int dy[4] = { 0, 0, 1, -1 };

        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k];
            int ny = y + dy[k];

            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            if (gs_is_obstacle(gs, nx, ny)) continue;

            int id = idx_of(gs, nx, ny);
            if (!out[id]) {
                out[id] = 1;
                qx[tail] = nx;
                qy[tail] = ny;
                tail++;
            }
        }
    }

    free(qx);
    free(qy);
}

// spocitanie kolko je policok je este
static int popcount_u8(const unsigned char *arr, int n) {
    int c = 0;
    for (int i = 0; i < n; i++) c += (arr[i] ? 1 : 0);
    return c;
}

//  Funkcia na kontrolu ci je dosiahnutelna odmena/ovocie dosiahnutelna pre kazdeho hraca 0/1
static void reachable_intersection_all_snakes(const GameState *gs, unsigned char *out) {
    int n = gs->width * gs->height;
    memset(out, 1, (size_t)n);
    unsigned char *tmp = (unsigned char *)malloc((size_t)n);
    if (!tmp) return;
    int first = 1;

    for (int s = 0; s < MAX_PLAYERS; s++) {
        if (!gs->snakes[s].active) continue;

        int hx = gs->snakes[s].seg[0].x;
        int hy = gs->snakes[s].seg[0].y;

        bfs_fill(gs, hx, hy, tmp);

        if (first) {
            memcpy(out, tmp, (size_t)n);
            first = 0;
        } else {
            for (int i = 0; i < n; i++) out[i] = (unsigned char)(out[i] && tmp[i]);
        }
    }
    free(tmp);
    if (first) {
        memset(out, 1, (size_t)n);
    }

    for (int y = 0; y < gs->height; y++) {
        for (int x = 0; x < gs->width; x++) {
            if (gs_is_obstacle(gs, x, y)) out[idx_of(gs, x, y)] = 0;
        }
    }
}

//  Kontrola ovocia ci je zjedene
static int eat_fruit_if_present(GameState *gs, int hx, int hy) {
    for (int i = 0; i < gs->fruit_count; i++) {
        if (gs->fruits[i].x == hx && gs->fruits[i].y == hy) {   // ak sa hlava hada nachazda na ovoci rychlo odstrani z pola
            gs->fruits[i] = gs->fruits[gs->fruit_count - 1];
            gs->fruit_count--;
            return 1;   // uz len vypise ze ovocie bolo zjedene
        }
    }
    return 0;   // nebollo ziadne zjedene
}

//  Funckia na doplnanie ovocia podla poctu hadov/hracov
static void ensure_fruits(GameState *gs) {
    int target = count_active_snakes(gs);
    int cap = fruits_capacity(gs);

    if (target > cap) target = cap;

    if (gs->fruit_count > target) { // ak sa nahoodu had odpoji sa znizi kapacita
        gs->fruit_count = target;
    }

    if (gs->fruit_count == target) return;

    int n = gs->width * gs->height;
    unsigned char *allowed = (unsigned char *)malloc((size_t)n);
    if (!allowed) return;

    if (gs->obstacles) {    // cast kontrolovania ci mozme ovocie zjest
        reachable_intersection_all_snakes(gs, allowed); // ci je dosiahnutelna

        // ak su velky hadi tak su oddeleny tak im dovolime aby sa spawnovalo ovocie zvlast
        if (popcount_u8(allowed, n) == 0) {
            for (int y = 0; y < gs->height; y++) {
                for (int x = 0; x < gs->width; x++) {
                    allowed[idx_of(gs, x, y)] = (unsigned char)(!gs_is_obstacle(gs, x, y));
                }
            }
        }
    } else {
        memset(allowed, 1, (size_t)n);  // bez prekazok je povolene vsade
    }

    while (gs->fruit_count < target) {  // ak sa stane ze neni umiestni sa dalsi kus vzdy nahodna bunka
        int placed = 0;

        for (int tries = 0; tries < 3000; tries++) {
            int x = rand() % gs->width;
            int y = rand() % gs->height;

            if (!allowed[idx_of(gs, x, y)]) continue;   // dosiahnutelna
            if (fruit_exists(gs, x, y)) continue;       // nesmie tam byt ovocko
            if (cell_occupied_by_snake(gs, x, y)) continue; // nesmie tam byt had

            // tu sa nastavy ak vsetko preslo
            gs->fruits[gs->fruit_count].x = x;
            gs->fruits[gs->fruit_count].y = y;
            gs->fruit_count++;
            placed = 1;
            break;
        }

        if (!placed) break; // ak sa stane ze neni miesto skoncime
    }

    free(allowed);
}

// Funkcia ci su bunky dosiahnutelne
static int map_is_connected_free(const GameState *gs) {
    int w = gs->width;
    int h = gs->height;
    int n = w * h;
    int sx = -1, sy = -1;
    int free_count = 0; // finalny pocet volnych buniek

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (!gs_is_obstacle(gs, x, y)) {
                free_count++;
                if (sx == -1) { sx = x; sy = y; }
            }
        }
    }

    if (free_count == 0) return 0;
    unsigned char *vis = (unsigned char *)malloc((size_t)n);
    if (!vis) return 0;

    bfs_fill(gs, sx, sy, vis);

    int reached = 0;
    for (int i = 0; i < n; i++) reached += (vis[i] ? 1 : 0);
    free(vis);
    return reached == free_count;
}

//  Funkcia nahodneho generovania prekazky
int gs_generate_obstacles_random(GameState *gs, int obstacle_count) {
    if (obstacle_count < 0) obstacle_count = 0;
    int n = gs->width * gs->height;

    if (!gs->obstacles) {
        gs->obstacles = (unsigned char *)malloc((size_t)n);
        if (!gs->obstacles) return -1;
    }

    for (int attempt = 0; attempt < 200; attempt++) {
        memset(gs->obstacles, 0, (size_t)n);

        int placed = 0;
        int tries = 0;

        // nahodne vybera miesto podla x y na prekazdku
        while (placed < obstacle_count && tries < obstacle_count * 300) {
            tries++;
            int x = rand() % gs->width;
            int y = rand() % gs->height;
            int id = idx_of(gs, x, y);
            if (gs->obstacles[id]) continue;

            gs->obstacles[id] = 1;
            placed++;
        }

        if (map_is_connected_free(gs)) return 0;
    }

    return -1;
}

//  Funkcia na celkove inicializovanie herneho stavu hry do startu
void gs_init(GameState *gs, int width, int height, int multiplayer) {
    memset(gs, 0, sizeof(*gs));

    gs->width = width;
    gs->height = height;
    gs->multiplayer = multiplayer;

    gs->mode = MODE_STANDARD;
    gs->time_limit_sec = 0;
    gs->start_ms = 0;
    gs->empty_since_ms = 0;
    gs->game_over = 0;

    gs->obstacles = NULL;
    gs->fruit_count = 0;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        gs->snakes[i].active = 0;
        gs->snakes[i].len = 0;
        gs->snakes[i].dir = DIR_RIGHT;
        gs->snakes[i].score = 0;
        gs->snakes[i].paused = 0;
        gs->snakes[i].resume_at_ms = 0;
        gs->snakes[i].join_ms = 0;
        gs->snakes[i].total_alive_ms = 0;
    }
}

// uvolni alokovane csti v hre
void gs_destroy(GameState *gs) {
    if (gs->obstacles) {
        free(gs->obstacles);
        gs->obstacles = NULL;
    }
}

// nastavy herny rezim a resetuje
void gs_set_mode(GameState *gs, GameMode mode, int time_limit_sec) {
    gs->mode = mode;
    gs->time_limit_sec = (mode == MODE_TIMED) ? time_limit_sec : 0;
    gs->start_ms = 0;
    gs->empty_since_ms = 0;
    gs->game_over = 0;
}

// funkcia na pridavanie noveho hada
int gs_add_snake(GameState *gs) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (gs->snakes[i].active) continue;

        Snake *s = &gs->snakes[i];
        s->active = 1;
        s->dir = DIR_RIGHT;
        s->score = 0;
        s->paused = 0;
        s->resume_at_ms = 0;

        int cap = snake_seg_capacity(s);
        if (cap < 3) { s->active = 0; return -1; }

        s->len = 3;

        int placed = 0;
        for (int tries = 0; tries < 4000; tries++) {
            int x = rand() % gs->width;
            int y = rand() % gs->height;

            if (gs_is_obstacle(gs, x, y)) continue;
            if (cell_occupied_by_snake(gs, x, y)) continue;

            // ponastavuje telo smerom v lavo
            int x1 = x - 1;
            int x2 = x - 2;

            if (!gs->obstacles) {
                if (x1 < 0) x1 += gs->width;
                if (x2 < 0) x2 += gs->width;
            } else {
                if (x2 < 0) continue;
                if (gs_is_obstacle(gs, x1, y) || gs_is_obstacle(gs, x2, y)) continue;
            }

            // nastavenie pociatocnych segmentov hada hlava a telo
            s->seg[0].x = x;  s->seg[0].y = y;
            s->seg[1].x = x1; s->seg[1].y = y;
            s->seg[2].x = x2; s->seg[2].y = y;

            placed = 1; // hodnota ze sa spawnul had
            break;
        }

        if (!placed) {
            s->active = 0;
            s->len = 0;
            return -1;  // ak sa neda nikde spawnut da chybu
        }

        ensure_fruits(gs);  // doplnenie ovocka po hadovy
        return i;
    }

    return -1;
}

// odstranenie hada podla jeho id
void gs_remove_snake(GameState *gs, int snake_id) {
    if (snake_id < 0 || snake_id >= MAX_PLAYERS) return;
    gs->snakes[snake_id].active = 0;
    gs->snakes[snake_id].len = 0;
    ensure_fruits(gs);
}

// funkcia na vypocitanie pozicie hlavy podla direction
static void compute_next_head(const GameState *gs, const Snake *s, int *nx, int *ny) {
    int x = s->seg[0].x;
    int y = s->seg[0].y;

    if (s->dir == DIR_UP) y--;
    else if (s->dir == DIR_DOWN) y++;
    else if (s->dir == DIR_LEFT) x--;
    else if (s->dir == DIR_RIGHT) x++;

    if (!gs->obstacles) {
        if (x < 0) x = gs->width - 1;
        if (x >= gs->width) x = 0;
        if (y < 0) y = gs->height - 1;
        if (y >= gs->height) y = 0;
    }

    *nx = x;
    *ny = y;
}

// zistuje ci x a y oredstavuje koliziu so stenou
static int will_hit_wall_or_obstacle(const GameState *gs, int x, int y) {
    if (gs->obstacles) {
        if (x < 0 || y < 0 || x >= gs->width || y >= gs->height) return 1;
    }
    return gs_is_obstacle(gs, x, y);
}

// zistujeme ci pozicia koliduje s nejakym telom hada
static int hits_any_snake(const GameState *gs, int x, int y) {
    for (int s = 0; s < MAX_PLAYERS; s++) {
        if (!gs->snakes[s].active) continue;
        for (int j = 0; j < gs->snakes[s].len; j++) {
            if (gs->snakes[s].seg[j].x == x && gs->snakes[s].seg[j].y == y) return 1;
        }
    }
    return 0;
}

// Tu sa odohrava jeden tick hry teda krok simulacie
void gs_tick(GameState *gs, long long now_ms) {
    if (gs->game_over) return;
    if (gs->start_ms == 0) gs->start_ms = now_ms;

    // koniec po uplynuti casu
    if (gs->mode == MODE_TIMED && gs->time_limit_sec > 0) {
        long long elapsed = now_ms - gs->start_ms;  // cas hry od startu
        if (elapsed >= (long long)gs->time_limit_sec * 1000LL) {
            gs->game_over = 1;
            return;
        }
    }

    // vratenie do hry po 3 sekundach
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Snake *s = &gs->snakes[i];
        if (!s->active) continue;
        if (s->paused && s->resume_at_ms > 0 && now_ms >= s->resume_at_ms) {
            s->paused = 0;
            s->resume_at_ms = 0;
        }
    }

    int nextx[MAX_PLAYERS];
    int nexty[MAX_PLAYERS];
    int will_move[MAX_PLAYERS];
    int dead[MAX_PLAYERS];
    memset(dead, 0, sizeof(dead)); // na zaciatku nik neumrel

    // smery hlav pre hady
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Snake *s = &gs->snakes[i];
        if (!s->active || s->paused) {
            will_move[i] = 0;
            nextx[i] = nexty[i] = -1;
            continue;
        }
        will_move[i] = 1;
        compute_next_head(gs, s, &nextx[i], &nexty[i]);
    }

    // kolize hlavy hadov
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!will_move[i]) continue;
        int x = nextx[i];
        int y = nexty[i];

        if (will_hit_wall_or_obstacle(gs, x, y)) {
            dead[i] = 1;
            continue;
        }

        if (hits_any_snake(gs, x, y)) {
            dead[i] = 1;
            continue;
        }

        for (int j = i + 1; j < MAX_PLAYERS; j++) {
            if (!will_move[j]) continue;
            if (nextx[j] == x && nexty[j] == y) {
                dead[i] = 1;
                dead[j] = 1;
            }
        }
    }

    // aplikuje sa pohyb a smrt pre hada/hraca
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Snake *s = &gs->snakes[i];
        if (!s->active) continue;

        if (dead[i]) {
            if (s->join_ms != 0) {
                s->total_alive_ms += (now_ms - s->join_ms);
                s->join_ms = 0;
            }
            s->active = 0;
            s->len = 0;
            continue;
        }

        if (!will_move[i]) continue;

        int hx = nextx[i];
        int hy = nexty[i];
        int ate = eat_fruit_if_present(gs, hx, hy);
        if (ate) s->score += 1;
        int cap = snake_seg_capacity(s);

        //  rast bez ak zjedol, predĺž len vtedy, keď je miesto
        int new_len = s->len;
        if (ate && s->len < cap) {
            new_len = s->len + 1;
        }

        // posunie segmenty od konca k hlave
        for (int j = new_len - 1; j > 0; j--) {
            s->seg[j] = s->seg[j - 1];
        }
        s->seg[0].x = hx;
        s->seg[0].y = hy;
        s->len = new_len;
    }

    // koniec hry po zmyznuti posledneho hada 10skund
    int act = count_active_snakes(gs);
    if (act == 0) {
        if (gs->empty_since_ms == 0) gs->empty_since_ms = now_ms;
        if ((now_ms - gs->empty_since_ms) >= 10000) {
            gs->game_over = 1;
            return;
        }
    } else {
        gs->empty_since_ms = 0;
    }

    // tu sa zosuladi na konci tiku ovocie s poctom hadov
    ensure_fruits(gs);
}