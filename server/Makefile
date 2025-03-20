# Makefile contents here
server: server.o digraph.o dijkstra.o
	g++ -Wall -g -o server server.o digraph.o dijkstra.o

server.o: server.cpp
	g++ -Wall -g -c server.cpp

dijkstra.o: dijkstra.cpp
	g++ -Wall -g -c dijkstra.cpp

digraph.o: digraph.cpp
	g++ -Wall -g -c digraph.cpp

run: server
	gnome-terminal -- ./server

clean:
	rm -f *o
	rm -f server