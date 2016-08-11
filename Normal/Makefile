DR: DRserver.o DRclient.o util.o
	gcc -o SS DRserver.o util.o -levent -lm -lpthread
	gcc -o CC DRclient.o util.o -levent -lm -lpthread -lGL -lGLU -lglut
FL: FLserver.o FLclient.o util.o
	gcc -o SS FLserver.o util.o -levent -lm -lpthread
	gcc -o CC FLclient.o util.o -levent -lm -lpthread -lGL -lGLU -lglut
UTIL: util.o
	gcc -o SS util.o -lm
DRserver.o: DRserver.c util.h
	gcc -c DRserver.c -Wall -O1 -ggdb3
DRclient.o: DRclient.c util.h
	gcc -c DRclient.c -Wall -O1 -ggdb3
FLserver.o: FLserver.c util.h
	gcc -c FLserver.c -Wall -O1 -ggdb3
FLclient.o: FLclient.c util.h
	gcc -c FLclient.c -Wall -O1 -ggdb3
util.o: util.c util.h
	gcc -c util.c -Wall -O1 -ggdb3
clean:
	if [ -f util.o ]; then \
		rm *.o ;\
	fi ;\
	if [ -f CC ]; then \
		rm CC ;\
	fi ;\
	if [ -f SS ]; then \
		rm SS ;\
	fi