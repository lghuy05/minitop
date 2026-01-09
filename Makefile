CC = gcc
CFLAGS = -Wall -Wextra -g

SRC = src/main.c
BIN = minitop

all: $(BIN)
$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(BIN)

