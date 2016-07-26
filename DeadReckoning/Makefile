SS: TrivialServer.o util.o CC
	gcc -o SS TrivialServer.o util.o -levent -lm -lpthread
TrivialServer.o: TrivialServer.c util.h
	gcc -c TrivialServer.c -Wall -O1
CC: TrivialClient.o util.o
	gcc -o CC TrivialClient.o util.o -levent -lm -lpthread -lGL -lGLU -lglut
TrivialClient.o: TrivialClient.c util.h
	gcc -c TrivialClient.c -Wall -O1
util.o: util.c util.h
	gcc -c util.c -Wall -O1
clean:
	rm *.o