CC = g++
FLAGS += -I. -I./include
SLIBS += -lleveldb -ljson_linux-gcc-4.6_libmt -lpthread
CLIBS += -ljson_linux-gcc-4.6_libmt -lpthread

all: c.out g.out

s.out: fstst.cpp fileserver.cpp server.cpp
	$(CC) $(FLAGS) -o s.out fstst.cpp fileserver.cpp server.cpp $(SLIBS)

c.out: cltst.cpp client.cpp
	$(CC) $(FLAGS) -o c.out cltst.cpp client.cpp $(CLIBS)

sc.out : simple_client.cpp client.cpp
	$(CC) $(FLAGS) -o sc.out simple_client.cpp client.cpp

a.out: hello.cpp
	$(CC) hello.cpp

g.out:  gatetst.cpp syncobj.o gateserver.o server.o
	$(CC) $(FLAGS) -o g.out gatetst.cpp gateserver.o server.o syncobj.o -lpthread -ljson_linux-gcc-4.6_libmt

syncobj.o: syncobj.cpp
	$(CC) $(FLAGS) -c syncobj.cpp

gateserver.o: gateserver.cpp
	$(CC) $(FLAGS) -c gateserver.cpp

server.o: server.cpp
	$(CC) $(FLAGS) -c server.cpp

clean:
	rm -rf *.out *.o *.stackdump *~ ./include/*~
