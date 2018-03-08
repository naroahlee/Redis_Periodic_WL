INCLUDE_DIR := ./inc
SRC_PATH := ./src
LIB_PATH := ./lib
CFLAG = -I${INCLUDE_DIR} -Wall -O2

CC = /usr/bin/gcc

LDLIBS = -L${LIB_PATH} -lhiredis -lrt

vpath %.c ${SRC_PATH}

ALL = periodic_client

.PHONY: all clean

all: ${ALL}


clean:
	rm -rf ${ALL} *.o
	clear

update:
	cscope -Rb

%.o: %.c
	${CC} ${CFLAG} -c $^ -o $@

periodic_client: periodic_client.o
	${CC} ${CFLAG} $^ ${LDLIBS} -o $@
