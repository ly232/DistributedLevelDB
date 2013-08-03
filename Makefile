FLAGS += -I. -I./include -I${HOME}/levdb/include
LIBS += -lleveldb -ljson_linux-gcc-4.6_libmt

all: s.out c.out a.out g.out

s.out: fstst.cpp fileserver.cpp server.cpp
	g++ $(FLAGS) -o s.out fstst.cpp fileserver.cpp server.cpp $(LIBS)

c.out: cltst.cpp client.cpp
	g++ $(FLAGS) -o c.out cltst.cpp client.cpp $(LIBS)

a.out: hello.cpp
	g++ $(FLAGS) hello.cpp $(LIBS)

g.out: gatetst.cpp gateserver.cpp server.cpp
	g++ $(FLAGS) -o g.out gatetst.cpp gateserver.cpp server.cpp $(LIBS)

clean:
	rm -rf *.out *.stackdump *~ ./include/*~
