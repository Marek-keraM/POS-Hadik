CC = gcc
CFLAGS = -Wall -Wextra -std=c11

SERVER = server/server
CLIENT = client/client

SERVER_SRC = server/server_main.c shared_code/ipc_messages.c shared_code/game_state.c
CLIENT_SRC = client/client_main.c shared_code/ipc_messages.c

all: $(SERVER) $(CLIENT)

$(SERVER): $(SERVER_SRC)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $(SERVER)

$(CLIENT): $(CLIENT_SRC)
	$(CC) $(CFLAGS) $(CLIENT_SRC) -o $(CLIENT)

clean:
	rm -f $(SERVER) $(CLIENT)