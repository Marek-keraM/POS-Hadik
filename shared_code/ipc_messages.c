#include "ipc_messages.h"
#include <stdio.h>

//   fd je vzdy socket z ktoreho citame
//   msg sprava ktoru posielame


//  Fukcia pre posileanie ipc spravy
int send_ipc_message(int fd, const IpcMessage *msg) {
    ssize_t sent = write(fd, msg, sizeof(IpcMessage));
    if (sent != sizeof(IpcMessage)) {   // ak nastane chyba vo write
        perror("send_ipc_message");
        return -1;
    }
    return 0;
}

//  Funkcia prijmne jednu ipc spravu ked uz je pripojena
int recv_ipc_message(int fd, IpcMessage *msg) {
    ssize_t rec = read(fd, msg, sizeof(IpcMessage));
    if (rec != sizeof(IpcMessage)) {    // ak nastane chyba v read
        perror("recv_ipc_message");
        return -1;
    }
    return 0;
}

//  Funkcia posle celkove parametre pri vytvoreni hry - server prijma od klienta
int send_new_game_params(int fd, const NewGameParams *params) {
    ssize_t sent = write(fd, params, sizeof(NewGameParams));
    if (sent != sizeof(NewGameParams)) {    // ci sa poslalo vsetko
        perror("send_new_game_params");
        return -1;
    }
    return 0;
}

//  Funkcia prijma vsetky parametre pri novej hre - server prijma od klienta
int recv_new_game_params(int fd, NewGameParams *params) {
    ssize_t rec = read(fd, params, sizeof(NewGameParams));
    if (rec != sizeof(NewGameParams)) {     // ci je vsetko to co ocakavame prijate
        perror("recv_new_game_params");
        return -1;
    }
    return 0;
}