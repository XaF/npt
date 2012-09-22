
OPT=-Wall -D_GNU_SOURCE -O2 -I.

LIB=-lm -lrt -llttng-ust -llttng-ctl -ldl

TARGET=npt

SRC=npt.c tracepoint_ust.c

all:
	gcc -o ${TARGET} ${OPT} ${SRC} ${LIB}

clean:
	rm ${TARGET}
