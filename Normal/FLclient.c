#include <arpa/inet.h>
#include <linux/input.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <event2/dns.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include <GL/freeglut.h>

#include "util.h"

/*Sync data*/
byte user_in[MAX_PLAYER];
cube cubelist[MAX_PLAYER];
uint32 frame = 0xFFFFFFFF;
long int interval = 33333;
struct timeval tv, tvold;
int user;
int stall = 1;
/*Multi-thread*/
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_t tid, disptid, keytid;
/*Log*/
#ifdef LOGFILE
FILE *logfile = NULL;
#endif
/*Key board*/
keyevent_queue_ptr keyqueue = NULL;
/*display*/
int dispflag = 0;

void display(){
	usleep(1);
	glClear(GL_COLOR_BUFFER_BIT);
	pthread_mutex_lock(&mtx);
	for(int i = 0; i < MAX_PLAYER; i++){
		float x, y;
		if(user_in[i] == 0) continue;
		if(i == user){
			glColor3f(0.0, 0.0, 0.0);
		}
		else{
			glColor3f(0.8, 0.2, 0.0);
		}
		x = cubelist[i].position.x;
		y = cubelist[i].position.y;
		glBegin(GL_TRIANGLE_FAN);
		glVertex3f(x + OBJ_WIDTH, y + OBJ_WIDTH, 0);
		glVertex3f(x, y + OBJ_WIDTH, 0);
		glVertex3f(x, y, 0);
		glVertex3f(x + OBJ_WIDTH, y, 0);
		glEnd();
	}
	pthread_mutex_unlock(&mtx);
	glFlush();
	glutPostRedisplay();
}

void reshape(int w, int h){
	glViewport (0, 0, (GLsizei)w, (GLsizei)h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-MAX_WIDTH, MAX_WIDTH, -MAX_HEIGHT, MAX_HEIGHT, 5, 15);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0, 0, 10, 0, 0, 0, 0, 1, 0);
}

void *RenderThread(void *arg){
	glutCreateWindow("Client");
	glClearColor(1.0, 1.0, 1.0, 0.0);
	glMatrixMode(GL_PROJECTION);
	glOrtho(-MAX_WIDTH, MAX_WIDTH, -MAX_HEIGHT, MAX_HEIGHT, 5, 15);
	glMatrixMode(GL_MODELVIEW);
	gluLookAt(0, 0, 10, 0, 0, 0, 0, 1, 0);
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutMainLoop();
	return NULL;
}

void *SendingThread(void *arg){
	int kbdfd;
	struct input_event event;
	keyevent kev;

	pthread_mutex_lock(&mtx);
	/*Open keyboard*/
	kbdfd = open("/dev/input/event1", O_RDONLY);//Should ajust according to actual keyboard device
	if(kbdfd < 0){
		#ifdef LOGFILE
		printlog(logfile, frame, "opening keyboard ... failed[Permission denied!]\n");
		#endif
		pthread_mutex_unlock(&mtx);
		pthread_cond_signal(&cond);
		return NULL;
	}
	#ifdef LOGFILE
	printlog(logfile, frame, "opening keyboard ... successful\n");
	#endif
	assert(keyqueue != NULL);
	pthread_mutex_unlock(&mtx);
	pthread_cond_signal(&cond);
	while(1){
		if(read(kbdfd, &event, sizeof(event)) == sizeof(event)){
			if(event.type == EV_KEY && event.value != 2){
				pthread_mutex_lock(&mtx);
				kev.frame = frame;
				kev.interval = interval;
				kev.key = event.code;
				kev.value = event.value;
				keyevent_queue_enqueue(keyqueue, &kev);
				pthread_mutex_unlock(&mtx);
			}
		}
	}
}

void *pkgThread(void *arg){
	long int tmp = 0;

	pthread_mutex_lock(&mtx);
	gettimeofday(&tvold, NULL);
	pthread_mutex_unlock(&mtx);
	while(1){
		usleep(1);
		pthread_mutex_lock(&mtx);
		gettimeofday(&tv, NULL);
		tmp = (tv.tv_sec - tvold.tv_sec) * 1000000;
		tmp = tmp + tv.tv_usec - tvold.tv_usec;
		interval = interval + tmp;
		tvold = tv;
		if(interval >= FRAME_LEN){
			interval -= FRAME_LEN;
			frame ++;
			if(frame % FRAMES_PER_UPDATE == 0){
				int flag = 0;
				while(stall){
					flag = 1;
					pthread_mutex_unlock(&mtx);
					usleep(1000);
					pthread_mutex_lock(&mtx);
				}
				#ifdef LOGFILE
				if(flag){
					printlog(logfile, frame, "stall occured\n");
				}
				#endif
				stall = 1;
			}
			tmp = 0;
			for(int i = 0; i < MAX_PLAYER; i++){
				if(user_in[i] == 0) continue;
				cube_stepforward(&cubelist[i], 1);
				#ifdef LOGFILE
				printlog(logfile, frame, "user %d: ", i);
				cube_print(logfile, cubelist[i]);
				fprintf(logfile, "\n");
				#endif
				tmp++;
			}
		}
		pthread_mutex_unlock(&mtx);
	}
}

void on_read(struct bufferevent *bev, void *arg){
	struct evbuffer *input = bufferevent_get_input(bev);
	char buf[MAXLEN];
	int flag = 0;
	uint32 sframe;
	byte tmp;
	keyevent kev;

	while(1){
		usleep(100000);
		int j;
		flag = evbuffer_remove(input, buf, 1);
		if(flag <= 0) break;
		switch((byte)buf[0]){
			case SC_CONFIRM:
				pthread_mutex_lock(&mtx);
				gettimeofday(&tvold, NULL);
				user_in[(int)buf[0]] = 1;
				#ifdef LOGFILE
				printlog(logfile, frame, "user %d: in ... waiting for others ...\n", user);
				#else
				printf("pending ...\n");
				#endif
				pthread_mutex_unlock(&mtx);
				break;
			case SC_START:
				evbuffer_remove(input, buf, 1);
				tmp = buf[0];
				for(int i = 0; i < tmp; i++){
					evbuffer_remove(input, buf, 33);
					cubelist[(int)buf[0]] = *((cube *)&buf[1]);
					user_in[(int)buf[0]] = 1;
				}
				#ifdef LOGFILE
				printlog(logfile, frame, "start running ...\n");
				#endif
				dispflag = 1;
				pthread_create(&disptid, NULL, RenderThread, NULL);
				pthread_create(&tid, NULL, pkgThread, bev);
				break;
			case SC_FLUSH:
				evbuffer_remove(input, buf, 4);
				sframe = *((uint32 *)&buf[0]);
				pthread_mutex_lock(&mtx);
				#ifdef LOGFILE
				printlog(logfile, frame, "server flush %08lX\n", sframe);
				#endif
				if(sframe > frame){
					for(int i = 0; i < MAX_PLAYER; i++){
						if(user_in[i] == 0) continue;
						cube_stepforward(&cubelist[i], sframe - frame - 1);
					}
					frame = sframe - 1;
					interval = FRAME_LEN;
					gettimeofday(&tvold, NULL);
				}
				while(1){
					int cuser;
					evbuffer_remove(input, buf, 1 + sizeof(keyevent));
					if(buf[0] == MAX_PLAYER) break;
					cuser = buf[0];
					kev = *((keyevent *)&buf[1]);
					cube_set_accel(&cubelist[cuser], kev.key, kev.value);
				}
				stall = 0;
				buf[0] = CS_UPDATE;
				*((uint32 *)&buf[1]) = frame;
				buf[5] = user;
				j = 6;
				while(keyevent_queue_isempty(keyqueue) == 0){
					keyevent_queue_dequeue(keyqueue, &kev);
					*((keyevent *)&buf[j]) = kev;
					j += sizeof(keyevent);
				}
				kev.value = 2;
				*((keyevent *)&buf[j]) = kev;
				j += sizeof(keyevent);
				evbuffer_add(bufferevent_get_output(bev), buf, j);
				#ifdef LOGFILE
				printlog(logfile, frame, "update to server ...\n");
				#endif
				pthread_mutex_unlock(&mtx);
				break;
		}
	}
}

void on_event(struct bufferevent *bev, short event, void *arg){
	struct evbuffer *output = bufferevent_get_output(bev);
	struct event_base *base = bufferevent_get_base(bev);
	char buf[MAXLEN];
	if(event & BEV_EVENT_EOF){
		pthread_mutex_lock(&mtx);
		#ifdef LOGFILE
		printlog(logfile, frame, "server down\n");
		#endif
		bufferevent_free(bev);
		glutLeaveMainLoop();
		event_base_loopexit(base, NULL);
		pthread_mutex_unlock(&mtx);
	}
	if(event & BEV_EVENT_ERROR){
		pthread_mutex_lock(&mtx);
		#ifdef LOGFILE
		printlog(logfile, frame, "connecting to server ... failed[ERROR]\n");
		#endif
		bufferevent_free(bev);
		glutLeaveMainLoop();
		event_base_loopexit(base, NULL);
		pthread_mutex_unlock(&mtx);
	}
	if(event & BEV_EVENT_CONNECTED){
		pthread_mutex_lock(&mtx);
		#ifdef LOGFILE
		printlog(logfile, frame, "connecting to server ... successful\n");
		#endif
		buf[0] = CS_LOGIN;
		buf[1] = user & 0xFF;
		evbuffer_add(output, buf, 2);
		pthread_mutex_unlock(&mtx);
	}
}

int main(int argc,char* argv[]){
	struct event_base *base;
	struct bufferevent *bev;
	int fd;
	struct sockaddr_in sin;

	/*check if CLI is right*/
	if(argc != 3){
		printf("usage: %s <host> <0-%d>\n", argv[0], MAXLEN - 1);
		return 1;
	}
	#ifdef LOGFILE
	logfile = fopen("client.log", "w");
	if(!logfile){
		printf("Client Down!\n");
		return 1;
	}
	#endif

	/*new server address(argv[1])*/
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(LISTEN_PORT);
	fd = inet_pton(AF_INET, argv[1], &sin.sin_addr);
	if(fd <= 0){
		#ifdef LOGFILE
		printlog(logfile, frame, "checking host ... failed\n");
		fclose(logfile);
		#endif
		return 1;
	}
	#ifdef LOGFILE
	printlog(logfile, frame, "checking host ... successful\n");
	#endif

	/*user(argv[2])*/
	user = strtol(argv[2], NULL, 10);
	memset(user_in, 0, sizeof(user_in));
	#ifdef LOGFILE
	printlog(logfile, frame, "checking user ... %d\n", user);
	#endif

	/*GL init*/
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_SINGLE);
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(600, 400);

	/*new event base*/
	base = event_base_new();
	if(!base){
		#ifdef LOGFILE
		printlog(logfile, frame, "creating event base ... failed\n");
		fclose(logfile);
		#endif
		return 1;
	}
	#ifdef LOGFILE
	printlog(logfile, frame, "creating event base ... successful\n");
	#endif

	/*new buffer event*/
	bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
	if(!bev){
		#ifdef LOGFILE
		printlog(logfile, frame, "creating buffer event ... failed\n");
		fclose(logfile);
		#endif
		return 1;
	}
	#ifdef LOGFILE
	printlog(logfile, frame, "creating buffer event ... successful\n");
	#endif

	/*new keyboard queue*/
	keyqueue = keyevent_queue_new(1024);
	if(!keyqueue){
		#ifdef LOGFILE
		printlog(logfile, frame, "creating keybord queue ... failed\n");
		fclose(logfile);
		#endif
		return 1;
	}
	#ifdef LOGFILE
	printlog(logfile, frame, "creating keybord queue ... successful\n");
	#endif
	
	/*A thread that listen keybord*/
	pthread_mutex_lock(&mtx);
	pthread_create(&keytid, NULL, SendingThread, NULL);
	pthread_cond_wait(&cond, &mtx);
	pthread_mutex_unlock(&mtx);

	/*A thread that render with OpenGL*/
	//pthread_create(&disptid, NULL, RenderThread, NULL);

	/*Connect to server*/
	fd = bufferevent_socket_connect(bev, (struct sockaddr*)&sin, sizeof(sin));
	if(fd < 0){
		#ifdef LOGFILE
		printlog(logfile, frame, "connecting to server ... failed[FD]\n");
		fclose(logfile);
		#endif
		bufferevent_free(bev);
		return 1;
	}
	bufferevent_setcb(bev, on_read, NULL, on_event, NULL);
	bufferevent_enable(bev, EV_READ|EV_WRITE);

	/*start polling*/
	event_base_dispatch(base);
	#ifdef LOGFILE
	fclose(logfile);
	#endif
	if(dispflag){
		pthread_join(disptid, NULL);
	}
	return 0;
}
