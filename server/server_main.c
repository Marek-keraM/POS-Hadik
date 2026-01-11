#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/select.h>

#include "../shared_code/ipc_messages.h"
#include "../shared_code/game_state.h"

#define MAX_CLIENTS 100
#define TICK_MS 200

typedef enum {
    NO_GAME = 0,
    RUNNING = 1,
    GAME_OVER = 2
} ServerGameStatus;

// ziskavanie aktualneho casu v milisekundach
static long long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000LL;
}

//  Vytvorenie server socketu
static int create_server_socket(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);       // domain socketu
    if (fd < 0) { perror("socket"); return -1; }

    struct sockaddr_un addr;        //  nastavovanie cesty k socketu
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SNAKE_SOCK_PATH, sizeof(addr.sun_path) - 1); //  cesta

    unlink(SNAKE_SOCK_PATH);    // odstranenie stareho socketu

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 10) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

//  Pomocná funkcia aby nedochadzalo k 180 stupnovemu otoceniu hada
static int is_opposite_dir(Direction cur, Direction nd) {
    return (cur == DIR_UP && nd == DIR_DOWN) ||
           (cur == DIR_DOWN && nd == DIR_UP) ||
           (cur == DIR_LEFT && nd == DIR_RIGHT) ||
           (cur == DIR_RIGHT && nd == DIR_LEFT);
}

//  Vytvaranie prekazky a ovocia na pozicií
static char cell_char(const GameState *gs, int x, int y) {
    if (gs_is_obstacle(gs, x, y)) return '#';   // # prekazka

    for (int i = 0; i < gs->fruit_count; i++) {
        if (gs->fruits[i].x == x && gs->fruits[i].y == y) return '*';   // * ovocko
    }

    for (int s = 0; s < MAX_PLAYERS; s++) { //  hady hráčov hlava @ a telo o
        if (!gs->snakes[s].active) continue;
        for (int j = 0; j < gs->snakes[s].len; j++) {
            if (gs->snakes[s].seg[j].x == x &&
                gs->snakes[s].seg[j].y == y) {
                return (j == 0) ? '@' : 'o';
            }
        }
    }

    return '.';     // . predstavuje len plochu kde sa mozme hýbat
}

/* ========================================================= */

int main(void) {
    int server_fd = create_server_socket();
    if (server_fd < 0) return 1;

    printf("Server spustený: %s\n", SNAKE_SOCK_PATH);

    int client_fd[MAX_CLIENTS];
    int client_snake[MAX_CLIENTS];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_fd[i] = -1;
        client_snake[i] = -1;
    }

    ServerGameStatus status = NO_GAME;
    NewGameParams params;
    memset(&params, 0, sizeof(params));

    GameState gs;
    int gs_ready = 0;   // 0/1 neexistuje alebo existuje inicializovana hra

    long long last_tick = now_ms();

    //              HLAVNA SLUCKA PROGRAMU
    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);

        // Časť server socketu sleduje nové pripojenia
        FD_SET(server_fd, &rfds);
        int maxfd = server_fd;

        // Časť pre klient sockety sleduje správy od klientov
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fd[i] != -1) {
                FD_SET(client_fd[i], &rfds);
                if (client_fd[i] > maxfd)
                    maxfd = client_fd[i];
            }
        }
        // Tu vytvárame casové okno aby sa server neblokoval a vedel robit tick
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 50 * 1000;

        // Tu nám select vráti tie sockety ktoré sú pripravené na čítanie na použitie
        select(maxfd + 1, &rfds, NULL, NULL, &tv);

        //          NOVY KLIENT TU
        if (FD_ISSET(server_fd, &rfds)) {
            int cfd = accept(server_fd, NULL, NULL);
            if (cfd >= 0) {
                //  ULOZ DO PRVEJ VOLNEJ POZICIE
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_fd[i] == -1) {
                        client_fd[i] = cfd;
                        client_snake[i] = -1;
                        printf("Klient je pripojený (slot %d)\n", i);
                        break;
                    }
                }
            }
        }

        //          SPRAVY PRE KLIENTOV
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = client_fd[i];
            if (fd == -1) continue;
            if (!FD_ISSET(fd, &rfds)) continue;

            IpcMessage msg;
            if (recv_ipc_message(fd, &msg) != 0) {
                //  KLIENT ODPOJENY
                printf("Klient %d sa odpojil.\n", i);
                if (client_snake[i] != -1) gs_remove_snake(&gs, client_snake[i]);
                close(fd);
                client_fd[i] = -1;
                client_snake[i] = -1;
                continue;
            }

            long long t = now_ms();

            //   Prikazyv hre
            if (msg.cmd == C_NEW_GAME) {
                NewGameParams p;
                recv_new_game_params(fd, &p);

                if (gs_ready) gs_destroy(&gs);

                params = p;
                gs_init(&gs, p.width, p.height, p.multiplayer);
                gs_set_mode(&gs, p.mode, p.time_limit_sec);

                if (p.world == WORLD_OBSTACLES) {
                    gs_generate_obstacles_random(&gs, 20);
                }

                status = RUNNING;
                gs_ready = 1;

                for (int k = 0; k < MAX_CLIENTS; k++)   // vsetko sa vynuluje kazdy ma vynulovaneho hada
                    client_snake[k] = -1;

                IpcMessage ok = { S_OK, 0, 0, 0 };
                send_ipc_message(fd, &ok);
            }

            else if (msg.cmd == C_JOIN_GAME && gs_ready && status == RUNNING) {
                int sid = gs_add_snake(&gs);
                if (sid < 0) {
                    IpcMessage err = { S_ERROR, 4, 0, 0 };
                    send_ipc_message(fd, &err);
                    continue;
                }

                client_snake[i] = sid;
                gs.snakes[sid].join_ms = t;

                // nezabudnut ze hra sa po pripojeni pozastavy na 3 sekundy
                IpcMessage fr = { S_FREEZE, 3, 0, 0 };
                IpcMessage ok = { S_OK, sid, 0, 0 };
                send_ipc_message(fd, &fr);
                send_ipc_message(fd, &ok);
            }

            else if (msg.cmd == C_INPUT && client_snake[i] != -1) {
                Snake *s = &gs.snakes[client_snake[i]];
                Direction nd = (Direction)msg.a;
                if (!is_opposite_dir(s->dir, nd))
                    s->dir = nd;

                IpcMessage ok = { S_OK, 0, 0, 0 };
                send_ipc_message(fd, &ok);
            }

            else if (msg.cmd == C_PAUSE && client_snake[i] != -1) {
                gs.snakes[client_snake[i]].paused = 1;
                IpcMessage ok = { S_OK, 0, 0, 0 };
                send_ipc_message(fd, &ok);
            }

            else if (msg.cmd == C_RESUME && client_snake[i] != -1) {
                gs.snakes[client_snake[i]].paused = 1;
                gs.snakes[client_snake[i]].resume_at_ms = t + 3000;

                IpcMessage fr = { S_FREEZE, 3, 0, 0 };
                IpcMessage ok = { S_OK, 0, 0, 0 };
                send_ipc_message(fd, &fr);
                send_ipc_message(fd, &ok);
            }

            else if (msg.cmd == C_GET_STATE && gs_ready) {

                int w = gs.width;
                int h = gs.height;

                long long start = gs.start_ms;
                if (start == 0) start = now_ms();

                int elapsed_sec = (int)((now_ms() - start) / 1000LL);

                int c_value;    //  hodnota je zaporna ak neni casovac
                if (gs.mode == MODE_TIMED && gs.time_limit_sec > 0) {
                    int rem = gs.time_limit_sec - elapsed_sec;
                    if (rem < 0) rem = 0;
                    c_value = rem;
                } else {
                    c_value = -(elapsed_sec + 1);
                }

                IpcMessage head = { S_STATE_HEADER, w, h, c_value };
                send_ipc_message(fd, &head);

                for (int y = 0; y < h; y++) {
                    for (int x = 0; x < w; x++) {
                        char ch = cell_char(&gs, x, y);
                        IpcMessage cell = { S_STATE_CELL, (int)ch, 0, 0 };
                        send_ipc_message(fd, &cell);
                    }
                }

                int score = 0;
                if (client_snake[i] != -1 && gs.snakes[client_snake[i]].active)
                    score = gs.snakes[client_snake[i]].score;

                IpcMessage ok = { S_OK, score, 0, 0 };
                send_ipc_message(fd, &ok);
            }

            else if (msg.cmd == C_QUIT) {
                if (client_snake[i] != -1)
                    gs_remove_snake(&gs, client_snake[i]);

                client_snake[i] = -1;

                IpcMessage ok = { S_OK, 0, 0, 0 };
                send_ipc_message(fd, &ok);

                close(fd);
                client_fd[i] = -1;
            }
        }

        //  Herny tick podla pravidelneho casu
        long long now = now_ms();
        if (gs_ready && status == RUNNING && now - last_tick >= TICK_MS) {
            gs_tick(&gs, now);
            last_tick = now;

            if (gs.game_over)
                status = GAME_OVER;
        }
    }

    close(server_fd);
    unlink(SNAKE_SOCK_PATH);
    return 0;
}