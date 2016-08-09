#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/util.h>

#include "util.h"

/*Sync data*/
struct bufferevent *user_bev[MAX_PLAYER];
cube cubelist[MAX_PLAYER];
uint32 frame = 0;
long int interval = 0;
/*Multi-thread*/
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_t tid = 0;
/*Log*/
#ifdef LOGFILE
FILE *logfile = NULL;
#endif

void *pkgThread(void *arg){
	struct timeval tv, tvold;
	int tmp = 0;

	pthread_mutex_lock(&mtx);
	frame = 0;
	gettimeofday(&tvold, NULL);
	pthread_mutex_unlock(&mtx);
	while(1){
		usleep(1);
		gettimeofday(&tv, NULL);
		tmp = (tv.tv_sec - tvold.tv_sec) * 1000000;
		tmp = tmp + tv.tv_usec - tvold.tv_usec;
		tvold = tv;
		pthread_mutex_lock(&mtx);
		interval = interval + tmp;
		if(interval >= FRAME_LEN){
			interval -= FRAME_LEN;
			frame ++;
			tmp = 0;
			for(int i = 0; i < MAX_PLAYER; i++){
				if(user_bev[i] == NULL) continue;
				cube_stepforward(&cubelist[i], 1);
				#ifdef LOGFILE
				printlog(logfile, frame, "user %d: ", i);
				cube_print(logfile, cubelist[i]);
				fprintf(logfile, "\n");
				#endif
				tmp++;
			}
			if(frame % FRAMES_PER_UPDATE == 0){
				char buf[MAXLEN];
				int j;
				buf[0] = SC_FLUSH;
				*((uint32 *)&buf[1]) = frame;
				*((long int *)&buf[5]) = interval;
				buf[9] = tmp & 0xFF;
				j = 10;
				for(int i = 0; i < MAX_PLAYER; i++){
					if(user_bev[i] == NULL) continue;
					buf[j] = i & 0xFF;
					*((cube *)&buf[j+1]) = cubelist[i];
					j += sizeof(cube) + 1;
				}
				#ifdef LOGFILE
				printlog(logfile, frame, "server flush to %d users", tmp);
				#endif
				for(int i = 0; i < MAX_PLAYER; i++){
					if(user_bev[i] == NULL) continue;
					#ifdef LOGFILE
					fprintf(logfile, " %d", i);
					#endif
					evbuffer_add(bufferevent_get_output(user_bev[i]), buf, j);
				}
				#ifdef LOGFILE
				fprintf(logfile, "\n");
				#endif
			}
			#ifdef LOGFILE
			fflush(logfile);
			#endif
		}
		pthread_mutex_unlock(&mtx);
	}
}

void on_read(struct bufferevent *bev, void *arg){
	struct evbuffer *input = bufferevent_get_input(bev);
	char buf[MAXLEN];
	int flag = 0;
	uint32 cframe;
	int cuser;

	while(1){
		flag = evbuffer_remove(input, buf, 1);
		if(flag <= 0) break;
		switch((byte)buf[0]){
			case CS_LOGIN:
				evbuffer_remove(input, buf, 1);
				cuser = buf[0];
				pthread_mutex_lock(&mtx);
				if(user_bev[cuser]){
					#ifdef LOGFILE
					printlog(logfile, frame, "user %d: already in\n", cuser);
					#endif
					bufferevent_free(bev);
				}
				else{
					user_bev[cuser] = bev;
					bufferevent_enable(bev, EV_READ|EV_WRITE);
					buf[0] = SC_CONFIRM;
					*((uint32 *)&buf[1]) = frame;
					*((long int *)&buf[5]) = interval;
					*((cube *)&buf[9]) = cubelist[cuser];
					evbuffer_add(bufferevent_get_output(bev), buf, sizeof(cube) + 9);
					#ifdef LOGFILE
					printlog(logfile, frame, "user %d: in ", cuser);
					cube_print(logfile, cubelist[cuser]);
					fprintf(logfile, "\n");
					#else
					printf("user %d: in\n", cuser);
					#endif
				}
				pthread_mutex_unlock(&mtx);
				break;
			case CS_UPDATE:
				flag = evbuffer_remove(input, buf, 5 + sizeof(cube));
				pthread_mutex_lock(&mtx);
				cframe = *((uint32 *)&buf[0]);
				cuser = buf[4];
				#ifdef LOGFILE
				printlog(logfile, frame, "update from user %d: cframe %08lX, ", cuser, cframe);
				cube_print(logfile, *((cube *)&buf[5]));
				fprintf(logfile, "\n");
				#endif
				if(frame >= cframe){
					cubelist[cuser] = *((cube *)&buf[5]);
					cube_stepforward(&cubelist[cuser], frame - cframe);
				}
				pthread_mutex_unlock(&mtx);
				break;
		}
		#ifdef LOGFILE
		fflush(logfile);
		#endif
	}
}

void on_event(struct bufferevent *bev, short int event, void *arg){
	int user = MAX_PLAYER;
	pthread_mutex_lock(&mtx);
	for(int i = 0; i < MAX_PLAYER; i++){
		if(bev == user_bev[i]){
			user = i;
			break;
		}
	}
	assert(user < MAX_PLAYER);
	pthread_mutex_unlock(&mtx);
	if(event & (BEV_EVENT_EOF | BEV_EVENT_ERROR)){
		int fd, flag;
		pthread_mutex_lock(&mtx);
		#ifdef LOGFILE
		printlog(logfile, frame, "user %d: out ", user);
		cube_print(logfile, cubelist[user]);
		fprintf(logfile, "\n");
		#else
		printf("user %d: out ", user);
		#endif
		bufferevent_free(bev);
		user_bev[user] = NULL;
		fd = open("DR.data", O_WRONLY);
		lseek(fd, user * sizeof(cube), SEEK_SET);
		flag = write(fd, (char *)&cubelist[user], sizeof(cube));
		printf("%d bytes written\n", flag);
		close(fd);
		pthread_mutex_unlock(&mtx);
	}
}

/*Connection Accepted*/
void on_acc(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *addr, int len, void *arg){
	struct event_base *base = evconnlistener_get_base(listener);
	struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	struct sockaddr_in *sin = (struct sockaddr_in *)addr;
	char str[MAXLEN];

	inet_ntop(AF_INET, &sin->sin_addr, str, MAXLEN);
	pthread_mutex_lock(&mtx);
	#ifdef LOGFILE
	printlog(logfile, frame, "ACK from %s: %d\n", str, ntohs(sin->sin_port));
	#else
	printf("ACK from %s: %d\n", str, ntohs(sin->sin_port));
	#endif
	pthread_mutex_unlock(&mtx);
	bufferevent_setcb(bev, on_read, NULL, on_event, arg);
	bufferevent_enable(bev, EV_READ);
}

/*Accept Failed*/
void on_acc_error(struct evconnlistener *listener, void *arg){
	struct event_base *base = evconnlistener_get_base(listener);
	#ifdef LOGFILE
	int err = EVUTIL_SOCKET_ERROR();
	pthread_mutex_lock(&mtx);
	printlog(logfile, frame, "error on ACK %d(%s)\n", err, evutil_socket_error_to_string(err));
	pthread_mutex_unlock(&mtx);
	#endif
	event_base_loopexit(base, NULL);
}

int main(){
	struct event_base *base;
	struct evconnlistener *listener;
	struct sockaddr_in sin;

	memset(user_bev, 0, sizeof(user_bev));
	#ifdef LOGFILE
	logfile = fopen("server.log", "w");
	if(!logfile){
		printf("Server Down!\n");
		return 1;
	}
	#endif
	/*reading data from file*/
	{
		int fd,res,i;
		char buf[MAXLEN];
		fd = open("DR.data", O_RDONLY);
		if(fd < 0){
			#ifdef LOGFILE
			printlog(logfile, frame, "opening ./DR.data ... failed[No such file!]\n");
			printlog(logfile, frame, "closing ...\n");
			fclose(logfile);
			#endif
			return 1;
		}
		#ifdef LOGFILE
		printlog(logfile, frame, "opening ./DR.data ... successful\n");
		#endif

		i = 0;
		while(1){
			res = read(fd, buf, sizeof(cube));
			if(res == sizeof(cube)){
				cubelist[i] = *((cube *)buf);
				i++;
				if(i >= MAX_PLAYER) break;
			}
			else{
				break;
			}
		}
		close(fd);
		if(i < MAX_PLAYER){
			#ifdef LOGFILE
			printlog(logfile, frame, "reading from file ... failed[No enough valid data!]\n");
			fclose(logfile);
			#endif
			return 1;
		}
		#ifdef LOGFILE
		printlog(logfile, frame, "reading from file ... successful\n");
		#endif
	}

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

	/*new listen address*/
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(LISTEN_PORT);

	/*new event listener*/
	listener = evconnlistener_new_bind(base, on_acc, NULL, LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, 4, (struct sockaddr *)&sin, sizeof(sin));
	if(!listener){
		#ifdef LOGFILE
		printlog(logfile, frame, "creating event listener ... failed\n");
		fclose(logfile);
		#endif
		return 1;
	}
	#ifdef LOGFILE
	printlog(logfile, frame, "creating event listener ... successful[Listening fd: %d]\n", evconnlistener_get_fd(listener));
	fflush(logfile);
	#endif
	evconnlistener_set_error_cb(listener, on_acc_error);
	evconnlistener_enable(listener);
	printf("Server Started...\n");

	/*Thread to broadcast status*/
	pthread_create(&tid, NULL, pkgThread, NULL);

	/*start polling*/
	event_base_dispatch(base);
	return 0;
}
