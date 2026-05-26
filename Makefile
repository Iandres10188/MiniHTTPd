CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE
LDFLAGS = -fPIE -pie

SRC_DIR = src
OBJ_DIR = src
BIN = minihttpd

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

-include $(DEPS)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BIN): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(BIN)

.PHONY: all clean run

all: $(BIN)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(SRC_DIR)/*.o $(SRC_DIR)/*.d $(BIN)