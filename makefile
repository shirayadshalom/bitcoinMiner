.PHONY = all clean

CC = gcc
LINKERFLAG_OUT = -o
LINKER_LIBS = -pthread -lz
SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.out,${SRCS})


all: ${OBJS}

# $@ = file name of the target
# S^ = all prerequisite (no duplications)

%.out: %.c
	${CC} ${LINKERFLAG_OUT} $@ $^ ${LINKER_LIBS}

clean:
	rm -rf $(OBJS)
