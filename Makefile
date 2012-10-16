LDFLAGS=-ggdb -static
CFLAGS=-O2 -Wall -ggdb
BIN=alfred
OBJS=main.o server.o client.o netsock.o send.o recv.o hash.o unix_sock.o util.o
default:	all
all:		$(BIN)
$(BIN):	$(OBJS)
	gcc $(LDFLAGS) $(OBJS) -o $(BIN)
.c.o:	
	gcc $(CFLAGS) -o $@ -c $<
clean:
	rm -f $(BIN) $(OBJS)
