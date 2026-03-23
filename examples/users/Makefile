CC = gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -I../../include
LDFLAGS =

LIBVDB = ../../lib/libvdb.a

TARGET = usersadm

all: $(TARGET)

$(TARGET): usersadm.c $(LIBVDB)
	$(CC) $(CFLAGS) -o $(TARGET) usersadm.c $(LIBVDB) $(LDFLAGS)

$(LIBVDB):
	$(MAKE) -C ../..

clean:
	rm -f $(TARGET) users.dat users.idx users.jnl users.i00 users.i01

.PHONY: all clean
