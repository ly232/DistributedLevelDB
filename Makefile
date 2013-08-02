FLAGS += -I. -I./include -I/home/ly232/levdb/include

all: s.out c.out a.out

s.out: fstst.cpp fileserver.cpp server.cpp
	g++ $(FLAGS) -o s.out fstst.cpp fileserver.cpp server.cpp

c.out: cltst.cpp client.cpp
	g++ $(FLAGS) -o c.out cltst.cpp client.cpp

a.out: hello.cpp
	g++ $(FLAGS) hello.cpp

clean:
	rm -rf *.out *.stackdump *~
