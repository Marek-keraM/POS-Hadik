#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <termios.h>

#include "../shared_code/ipc_messages.h"

//      Pripojovanie k serveru
//      path = cesta k socketu (napriklad pre nase pouzitie dolezite /tmp/snake_server.sock)
static int connect_to_server(const char *path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);   // domain socketu
    if (fd < 0) { perror("socket"); return -1; }

    struct sockaddr_un addr;    // nastavovanie cesty k socketu
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    // ak je path NULL alebo prazdny, pouzije sa default
    if (!path || path[0] == '\0') {
        path = SNAKE_SOCK_PATH;
    }

    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1); // cesta

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {  // pripojenie klienta
        perror("connect");
        close(fd);
        return -1;
    }
    return fd;
}

//  Ciselne vstupy
static int read_int(const char *prompt) {
    int x;
    printf("%s", prompt);
    fflush(stdout);
    if (scanf("%d", &x) != 1) return -1;    // celé číslo
    return x;
}

//  Cistenie bufferu
static void clear_stdin_line(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}  // odstranovanie znakov cely rad!
}

//  Odstranenie '\n' z konca stringu
static void trim_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    if (n > 0 && s[n - 1] == '\n') s[n - 1] = '\0';
}

//  Bezpecne nacitanie riadku cesty k socketu
static int read_line(const char *prompt, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return -1;

    printf("%s", prompt);
    fflush(stdout);

    if (!fgets(out, (int)out_sz, stdin)) return -1;
    trim_newline(out);
    return 0;
}
//  Ulozenie povodneho terminalu
static struct termios g_old_term;

//  Zapnutie raw
static void term_raw_on(void) {
    struct termios t;
    tcgetattr(STDIN_FILENO, &g_old_term);   // aktualny stav teminalu
    t = g_old_term; // kopia nastavenia
    t.c_lflag &= ~(ICANON | ECHO);  //  vypnuite kanoicky rezim
    // neblokujuce nastavenie citania
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);   // aplikovanie okamzite
}
//  Vypnutie raw
static void term_raw_off(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_old_term);
}
//  Cistenie obrazovky
static void clear_screen(void) {
    printf("\033[2J\033[H");
}

//  CMD sprava -> server
static void send_simple_cmd(int fd, int cmd) {
    IpcMessage m = { cmd, 0, 0, 0 };
    send_ipc_message(fd, &m);

    IpcMessage r;
    if (recv_ipc_message(fd, &r) == 0) {
        (void)r;    //  ignorujeme aby sa nezahltil vypis
    }
}

//  Vykreslovanie mapy a stavy
static int fetch_and_render(int fd) {
    IpcMessage req = { C_GET_STATE, 0, 0, 0 };
    send_ipc_message(fd, &req);

    IpcMessage head;    //potrebne rozmery a cas
    if (recv_ipc_message(fd, &head) != 0) return -1;
    if (head.cmd == S_ERROR) return -2;
    if (head.cmd != S_STATE_HEADER) return -3;

    int w = head.a;
    int h = head.b;

    int time_val = head.c;

    char *grid = (char *)malloc((size_t)w * (size_t)h); // alokacia buffera ASCII
    if (!grid) return -4;

    //  Tvorenie mapy bunka po bunke
    for (int i = 0; i < w * h; i++) {
        IpcMessage cell;
        if (recv_ipc_message(fd, &cell) != 0) { free(grid); return -1; }
        if (cell.cmd != S_STATE_CELL) { free(grid); return -3; }
        grid[i] = (char)cell.a;
    }

    IpcMessage tail;
    if (recv_ipc_message(fd, &tail) != 0) { free(grid); return -1; }
    int score = (tail.cmd == S_OK) ? tail.a : 0;

    clear_screen();

    if (time_val >= 0) {
        printf("Zostávajúci čas: %ds | Skóre: %d\n", time_val, score);
    } else {
        int elapsed = -time_val - 1;
        printf("Čas: %ds | Skóre: %d\n", elapsed, score);
    }

    printf("Ovládanie hry: w/a/s/d = pohyb | p = pauza | r = pokrčovanie | q = spať\n\n");

    // Finálne vykreslenie mriežky mapy
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            putchar(grid[y * w + x]);
            putchar(' ');
        }
        putchar('\n');
    }

    free(grid);
    fflush(stdout);
    return 0;
}

//      Ovládanie a vykreslovanie
static void live_mode(int fd) {
    term_raw_on();  // raw mode zapnutie

    int running = 1;
    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0; //  necaka testuje dostupnost pre moznu zmenu

        int sel = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
        if (sel > 0 && FD_ISSET(STDIN_FILENO, &rfds)) { // zaciatok citania dostupnych klaves 1 znak len
            char ch;
            ssize_t n = read(STDIN_FILENO, &ch, 1);
            if (n > 0) {
                if (ch == 'q') {
                    running = 0;
                } else if (ch == 'p') {
                    send_simple_cmd(fd, C_PAUSE);
                } else if (ch == 'r') {
                    IpcMessage m = { C_RESUME, 0, 0, 0 };
                    send_ipc_message(fd, &m);

                    IpcMessage r1;
                    if (recv_ipc_message(fd, &r1) != 0) running = 0;
                    else {
                        if (r1.cmd == S_FREEZE) {   // freez je len 3 sekundy !! potom pokračuje sprava OK!
                            IpcMessage r2;
                            if (recv_ipc_message(fd, &r2) != 0) running = 0;
                        }
                    }
                } else {
                    int dir = -1;
                    if (ch == 'w') dir = DIR_UP;
                    else if (ch == 'd') dir = DIR_RIGHT;
                    else if (ch == 's') dir = DIR_DOWN;
                    else if (ch == 'a') dir = DIR_LEFT;

                    if (dir != -1) {
                        IpcMessage m = { C_INPUT, dir, 0, 0 };
                        send_ipc_message(fd, &m);

                        IpcMessage resp;
                        recv_ipc_message(fd, &resp); // nevypisuje sa len ontroluje
                    }
                }
            }
        }

        if (fetch_and_render(fd) < 0) {
            printf("\nChyba pri nacitani stavu. Koncim live mode.\n");
            break;
        }

        usleep(200000); /* 200ms */
    }

    term_raw_off();
}

//      Hlavná časť
int main(void) {
    // aktualne zvoleny server (socket path)
    char server_path[108]; // typicka max dlzka pre sun_path
    strncpy(server_path, SNAKE_SOCK_PATH, sizeof(server_path) - 1);
    server_path[sizeof(server_path) - 1] = '\0';

    // spojenie sa vytvori az ked ho budeme potrebovat (alebo ked si ho zvolime v menu)
    int fd = -1;

    while (1) {
        printf("\n      Menu    \n");
        printf("0)  Vybrať / zmeniť server -> (aktuálne: %s)\n", server_path);
        printf("1)  Vytvorenie novej hry ->\n");
        printf("2)  Pripojenie sa do hry ->\n");
        printf("3)  Začať hrať hru ->\n");
        printf("4)  Ukončiť Menu ->\n");
        printf("Váš výber akcie: ");

        int choice = 0;
        if (scanf("%d", &choice) != 1) break;
        clear_stdin_line(); // aby sme mohli pouzivat fgets() pri volbe servera

        // 0) dorobena pre viac serverov (volba servera)!!!!!
        if (choice == 0) {
            char buf[108];
            printf("\nAktuálny server: %s\n", server_path);
            printf("Zadaj cestu k server socketu (Enter = ponechat):\n");

            // nacitajme riadok (moze byt aj prazdny)
            if (read_line("> ", buf, sizeof(buf)) == 0) {
                if (buf[0] != '\0') {
                    strncpy(server_path, buf, sizeof(server_path) - 1);
                    server_path[sizeof(server_path) - 1] = '\0';
                }

                // ak uz sme boli pripojeni, zatvorime stare spojenie
                if (fd != -1) {
                    close(fd);
                    fd = -1;
                }

                // pokusime sa pripojit hned, aby user videl ci to funguje
                fd = connect_to_server(server_path);
                if (fd < 0) {
                    printf("ERROR: Nepodarilo sa pripojit na %s\n", server_path);
                    fd = -1;
                } else {
                    printf("OK: Pripojeny na %s\n", server_path);
                }
            }
            continue;
        }

        // ak nie sme pripojeni, skusime sa pripojit na aktualne zvoleny server
        if (fd == -1) {
            fd = connect_to_server(server_path);
            if (fd < 0) {
                printf("ERROR: Nie som pripojeny na server (%s). Najprv zvol server v menu (0).\n", server_path);
                fd = -1;
                continue;
            }
        }

        if (choice == 1) {
            NewGameParams p;
            memset(&p, 0, sizeof(p));

            p.mode =    (GameMode)read_int("Druh herného režimu 1 = basic, 2 = časový: ");
            p.world =   (WorldType)read_int("Druh herného sveta 1 = prázdny, 2 = generované prekážky, 3 = možnosť vlastného súboru: ");
            p.multiplayer = read_int("Hra pre viac hráčov 0 = jeden hráč, 1 = pre viac hráčov:");

            if (p.world != WORLD_FROM_FILE) {
                p.width = read_int("Šírka mapy: ");
                p.height = read_int("Výška mapy: ");
            } else {
                p.width = 20;
                p.height = 10;
                printf("(Zatiaľ file svet nie je implementovaný, použije sa 20x10)\n");
            }

            if (p.mode == MODE_TIMED) p.time_limit_sec = read_int("Čas hry v sekundách: ");
            else p.time_limit_sec = 0;

            IpcMessage m = { C_NEW_GAME, 0, 0, 0 };
            send_ipc_message(fd, &m);
            send_new_game_params(fd, &p);

            IpcMessage resp;
            if (recv_ipc_message(fd, &resp) == 0 && resp.cmd == S_OK) {
                printf("GRATULUJEM: Nová hra vytvorená.\n");
            } else {
                printf("ERROR: Nová hra zlyhala.\n");
            }
        }
        else if (choice == 2) {
            IpcMessage m = { C_JOIN_GAME, 0, 0, 0 };
            send_ipc_message(fd, &m);

            IpcMessage r1;
            if (recv_ipc_message(fd, &r1) != 0) { printf("Odpojene.\n"); break; }

            if (r1.cmd == S_FREEZE) {
                printf("Freeze na %d sekundy...\n", r1.a);
                IpcMessage r2;
                if (recv_ipc_message(fd, &r2) == 0 && r2.cmd == S_OK) {
                    printf("GRATULUJEM: Pripojeny (snake_id=%d)\n", r2.a);
                } else {
                    printf("ERROR: Join problem.\n");
                }
            } else if (r1.cmd == S_OK) {
                printf("GRATULUJEM: Pripojeny.\n");
            } else {
                printf("ERROR: Pripojenie zamietnuty (kod=%d).\n", r1.a);
            }
        }
        else if (choice == 3) {
            live_mode(fd);
        }
        else if (choice == 4) {
            IpcMessage m = { C_QUIT, 0, 0, 0 };
            send_ipc_message(fd, &m);
            IpcMessage r;
            recv_ipc_message(fd, &r);
            break;
        }
    }

    if (fd != -1) close(fd);
    return 0;
}