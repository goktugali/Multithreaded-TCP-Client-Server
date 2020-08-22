all: server.o client.o utils.o
	gcc -Wall server.o utils.o -lrt -lpthread -o server -lm
	gcc -Wall client.o utils.o -lrt -lpthread -o client -lm

client.o: client.c
	gcc -c -Wall -pedantic-errors client.c -std=gnu99 -g -lm

server.o: server.c
	gcc -c -Wall -pedantic-errors server.c -std=gnu99 -g -lm
	
utils.o: utils.c utils.h	
	gcc -c -Wall -pedantic-errors utils.c -std=gnu99 -g -lm

clean:
	rm -rf *o