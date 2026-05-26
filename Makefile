CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
# Endurecimiento y generación automática de dependencias (-MMD -MP)
CFLAGS  += -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -MMD -MP
LDFLAGS := -pie

SRC     := $(wildcard src/*.c)
OBJ     := $(SRC:.c=.o)
DEPS    := $(SRC:.c=.d)
BIN     := minihttpd

.PHONY: all clean run

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN) 8080

clean:
	rm -f $(OBJ) $(BIN) $(DEPS)

# Incluye las dependencias autogeneradas (los archivos .d) si existen
-include $(DEPS)