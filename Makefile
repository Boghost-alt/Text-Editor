CC = clang
# CFLAGS = -Wall -I/opt/homebrew/Cellar/libmicrohttpd/1.0.1/include
# LDFLAGS = -L/opt/homebrew/Cellar/libmicrohttpd/1.0.1/lib -lmicrohttpd

SRC = main.c
OBJ = $(SRC:.c=.o)
EXEC = main

.PHONY: all clean

all: $(EXEC)

# Link the executable
$(EXEC): $(OBJ)
	$(CC) -o $(EXEC) $(OBJ) $(LDFLAGS)

# Compile source files
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

# Clean object files and executable
clean:
	rm -f $(OBJ) $(EXEC)
