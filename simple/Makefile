TrivialServer: TrivialServer.o util.o TrivialClient
	gcc -o TrivialServer TrivialServer.o util.o -levent -lpthread -lm
TrivialServer.o: TrivialServer.c util.h
	gcc -c TrivialServer.c
TrivialClient: TrivialClient.o util.o
	gcc -o TrivialClient TrivialClient.o util.o -levent -lpthread -lglut -lGL -lGLU -lm
TrivialClient.o: TrivialClient.c util.h
	gcc -c TrivialClient.c
util.o: util.c util.h
	gcc -c util.c