CC      = gcc
CFLAGS  = -std=c17 -Wall -Wextra
LDFLAGS = -lm

SRCS = main.c lexer.c ast.c parser.c value.c env.c interpreter.c chunk.c compiler.c debug.c
OBJS = $(SRCS:.c=.o)

aster: $(OBJS)
	$(CC) $(CFLAGS) -o aster $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f aster $(OBJS)

.PHONY: clean
