CC = g++
FLAGS += -g -I. -I./include -I./client -I./server -I./util

all: c.out g.out l.out

testchord.out: chordtst.cpp client.o chord.o
	$(CC) $(FLAGS) -o testchord.out chordtst.cpp client.o chord.o -lpthread -ljson_linux-gcc-4.6_libmt

chord.o: util/chord.cpp
	$(CC) $(FLAGS) -c util/chord.cpp

fs.out: fstst.cpp fileserver.cpp server.cpp
	$(CC) $(FLAGS) -o fs.out fstst.cpp fileserver.cpp server.cpp 

c.out: cltst.cpp client.o syncobj.o
	$(CC) $(FLAGS) -o c.out cltst.cpp client.o syncobj.o -lpthread -ljson_linux-gcc-4.6_libmt

client.o: client/client.cpp
	$(CC) $(FLAGS) -c client/client.cpp

g.out:  gatetst.cpp syncobj.o gateserver.o server.o client.o clusterserver.o
	$(CC) $(FLAGS) -o g.out gatetst.cpp gateserver.o server.o client.o syncobj.o clusterserver.o  -lpthread -ljson_linux-gcc-4.6_libmt

l.out: leveldbtst.cpp chord.o syncobj.o leveldbserver.o server.o client.o
	$(CC) $(FLAGS) -o l.out leveldbtst.cpp chord.o client.o syncobj.o leveldbserver.o server.o -lpthread -ljson_linux-gcc-4.6_libmt -lleveldb

syncobj.o: util/syncobj.cpp
	$(CC) $(FLAGS) -c util/syncobj.cpp

gateserver.o: server/gateserver.cpp
	$(CC) $(FLAGS) -c server/gateserver.cpp

clusterserver.o: server/clusterserver.cpp
	$(CC) $(FLAGS) -c server/clusterserver.cpp

leveldbserver.o: server/leveldbserver.cpp
	$(CC) $(FLAGS) -c server/leveldbserver.cpp

server.o: server/server.cpp
	$(CC) $(FLAGS) -c server/server.cpp

clean:
	rm -rf *.out *.o *.stackdump *~ *# ./include/*~ ./include/*# ./client/*~ ./client/*# ./server/*~ ./server/*# ./util/*~ ./util/*#
