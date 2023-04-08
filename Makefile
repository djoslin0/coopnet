CXX = g++
CXXFLAGS = -Wall -Werror -std=c++11 -g
INCLUDES = -Icommon -Ilib/include
LDFLAGS = -pthread

COMMON_SRC := $(wildcard common/*.cpp)
COMMON_OBJ := $(patsubst common/%.cpp, bin/o/%.o, $(COMMON_SRC))

CLIENT_SRC = $(wildcard client/*.cpp) $(COMMON_SRC)
CLIENT_OBJ = $(patsubst %.cpp, bin/o/%.o, $(CLIENT_SRC))

SERVER_SRC = $(wildcard server/*.cpp) $(COMMON_SRC)
SERVER_OBJ = $(patsubst %.cpp, bin/o/%.o, $(SERVER_SRC))

BIN_DIR = bin
LIB_DIR = lib
LIBS = -ljuice

.PHONY: all client server lib clean

all: client server lib

client: $(CLIENT_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -L$(LIB_DIR) $(LDFLAGS) -o $(BIN_DIR)/$@ $(CLIENT_OBJ) $(LIBS)

server: $(SERVER_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -L$(LIB_DIR) $(LDFLAGS) -o $(BIN_DIR)/$@ $(SERVER_OBJ) $(LIBS)

lib: $(COMMON_OBJ) | $(BIN_DIR)
	ar rcs $(BIN_DIR)/libcoopnet.a $(COMMON_OBJ)

$(COMMON_OBJ): $(COMMON_SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

bin/o/%.o: %.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)
	mkdir -p $(BIN_DIR)/o
	mkdir -p $(BIN_DIR)/o/client
	mkdir -p $(BIN_DIR)/o/server
	mkdir -p $(BIN_DIR)/o/common

clean:
	rm -rf $(BIN_DIR)
