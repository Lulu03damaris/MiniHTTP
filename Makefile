CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LDFLAGS =

SRC_DIR = src
OBJ_DIR = build
TARGET = minihttpd

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/server.c \
       $(SRC_DIR)/http.c \
       $(SRC_DIR)/files.c \
       $(SRC_DIR)/mime.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

.PHONY: all clean asan

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

asan: CFLAGS = -Wall -Wextra -g -fsanitize=address,undefined -Iinclude
asan: LDFLAGS = -fsanitize=address,undefined
asan: clean $(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
