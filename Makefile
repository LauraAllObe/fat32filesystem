SRC := src
BIN := bin
EXECUTABLE := filesys

SRCS := $(wildcard $(SRC)/*.c)
OBJS := $(patsubst $(SRC)/%.c, $(BIN)/%.o, $(SRCS))
DIRS := $(BIN)/
EXEC := $(BIN)/$(EXECUTABLE)
CC := gcc
CFLAGS := -g -Wall -std=c99
LDFLAGS :=

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(EXEC)

$(BIN)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(EXEC)
	$(EXEC)

clean:
	rm -f $(OBJS) $(EXEC)

$(shell mkdir -p $(DIRS))

.PHONY: run clean all
