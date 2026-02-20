CC      = cc
TARGET  = ar

SOURCES = ./ar.c \
          ./liblightfs/lightfs.c

OBJECTS = ${SOURCES:.c=.o}

BASE_CFLAGS = -g -O2 -march=native
LDFLAGS =

NOR_FLAGS = ${BASE_CFLAGS}

SEC_FLAGS = \
        -g -O2 -march=native \
        -Wall -Wextra -Wpedantic \
        -fstack-protector-strong -D_FORTIFY_SOURCE=2 \
        -Wformat=2 -Wformat-security -Wstringop-overflow -Wnull-dereference \
        -fanalyzer -Werror -Werror=unused-result \
        -fsanitize=address,undefined -fno-omit-frame-pointer

.SUFFIXES: .c .o

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

${TARGET}: ${OBJECTS}
	${CC} -o ${TARGET} ${OBJECTS} ${LDFLAGS}

nor: CFLAGS = ${NOR_FLAGS}
nor: ${TARGET}

sec: CFLAGS = ${SEC_FLAGS}
sec: ${TARGET}

minimize:
	strip ${TARGET}

clean:
	rm -f ${OBJECTS} ${TARGET} a.out

.PHONY: nor sec clean minimize

