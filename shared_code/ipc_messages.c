#include "ipc_messages.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>

/*
 * Pri SOCK_STREAM nie je zaručené, že read()/write() prenesú celý blok naraz.
 * Preto robíme "full" verzie, ktoré čítajú/zapisujú presne N bajtov.
 */
static int write_full(int fd, const void *buf, size_t n) {
    const char *p = (const char *)buf;
    size_t off = 0;

    while (off < n) {
        ssize_t w = write(fd, p + off, n - off);
        if (w < 0) {
            if (errno == EINTR) continue; // prerusene signalom, skus znovu
            return -1;
        }
        if (w == 0) {
            // pri write() by sa to stat nemalo casto, ale berieme to ako chybu
            return -1;
        }
        off += (size_t)w;
    }
    return 0;
}

static int read_full(int fd, void *buf, size_t n) {
    char *p = (char *)buf;
    size_t off = 0;

    while (off < n) {
        ssize_t r = read(fd, p + off, n - off);
        if (r < 0) {
            if (errno == EINTR) continue; // prerusene signalom, skus znovu
            return -1;
        }
        if (r == 0) {
            // EOF = klient/server zatvoril spojenie
            return -1;
        }
        off += (size_t)r;
    }
    return 0;
}

//  Fukcia pre posileanie ipc spravy
int send_ipc_message(int fd, const IpcMessage *msg) {
    if (write_full(fd, msg, sizeof(IpcMessage)) != 0) {
        perror("send_ipc_message");
        return -1;
    }
    return 0;
}

//  Funkcia prijmne jednu ipc spravu ked uz je pripojena
int recv_ipc_message(int fd, IpcMessage *msg) {
    if (read_full(fd, msg, sizeof(IpcMessage)) != 0) {
        perror("recv_ipc_message");
        return -1;
    }
    return 0;
}

//  Funkcia posle celkove parametre pri vytvoreni hry - server prijma od klienta
int send_new_game_params(int fd, const NewGameParams *params) {
    if (write_full(fd, params, sizeof(NewGameParams)) != 0) {
        perror("send_new_game_params");
        return -1;
    }
    return 0;
}

//  Funkcia prijma vsetky parametre pri novej hre - server prijma od klienta
int recv_new_game_params(int fd, NewGameParams *params) {
    if (read_full(fd, params, sizeof(NewGameParams)) != 0) {
        perror("recv_new_game_params");
        return -1;
    }
    return 0;
}