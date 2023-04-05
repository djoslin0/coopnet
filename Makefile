CC = gcc
CFLAGS = -Wall -Werror
INCLUDES = -Icommon
LDFLAGS = -pthread

COMMON_SRC := $(wildcard common/*.c)
COMMON_OBJ := $(patsubst common/%.c, bin/o/%.o, $(COMMON_SRC))

CLIENT_SRC = client/client.c $(COMMON_SRC)
CLIENT_OBJ = $(patsubst %.c, bin/o/%.o, $(CLIENT_SRC))

SERVER_SRC = server/server.c $(COMMON_SRC)
SERVER_OBJ = $(patsubst %.c, bin/o/%.o, $(SERVER_SRC))

BIN_DIR = bin

.PHONY: all client server clean

all: client server

client: $(CLIENT_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(BIN_DIR)/$@ $(CLIENT_OBJ)

server: $(SERVER_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $(BIN_DIR)/$@ $(SERVER_OBJ)

bin/o/%.o: %.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)
	mkdir -p $(BIN_DIR)/o
	mkdir -p $(BIN_DIR)/o/client
	mkdir -p $(BIN_DIR)/o/server
	mkdir -p $(BIN_DIR)/o/common

clean:
	rm -rf $(BIN_DIR)
