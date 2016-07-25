main: TrivialServer.o util.o
	gcc -o main TrivialServer.o util.o -levent -lm -lpthread
TrivialServer.o: TrivialServer.c util.h
	gcc -c TrivialServer.c
util.o: util.c util.h
	gcc -c util.c