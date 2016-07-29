#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
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
pthread_t tid;
/*Log*/
FILE *logfile = NULL;

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
		interval = interval + tmp;
		tvold = tv;
		pthread_mutex_lock(&mtx);
		if((frame % 3 != 0 && interval >= 33333) || (frame % 3 == 0 && interval >= 33334)){
			if(frame %3 != 0){
				interval -= 33333;
			}
			else{
				interval -= 33334;
			}
			frame ++;
			tmp = 0;
			for(int i = 0; i < MAX_PLAYER; i++){
				if(user_bev[i] == NULL) continue;
				printlog(logfile, frame);
				fprintf(logfile, "user %d: ", i);
				cube_print(logfile, cubelist[i]);
				fprintf(logfile, " -> ");
				cube_stepforward(&cubelist[i], 1);
				cube_print(logfile, cubelist[i]);
				fprintf(logfile, "\n");
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
				printlog(logfile, frame);
				fprintf(logfile, "server flush to %d users", tmp);
				for(int i = 0; i < MAX_PLAYER; i++){
					if(user_bev[i] == NULL) continue;
					fprintf(logfile, " %d", i);
					evbuffer_add(bufferevent_get_output(user_bev[i]), buf, j);
				}
				fprintf(logfile, "\n");
			}
			fflush(logfile);
		}
		pthread_mutex_unlock(&mtx);
	}
}

void on_read(struct bufferevent *bev, void *arg){
	struct evbuffer *input = bufferevent_get_input(bev);
	char buf[MAXLEN];
	char sendbuf[MAXLEN];
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
				printlog(logfile, frame);
				if(user_bev[cuser]){
					fprintf(logfile, "user %d: already in\n", cuser);
					bufferevent_free(bev);
				}
				else{
					fprintf(logfile, "user %d: in ", cuser);
					user_bev[cuser] = bev;
					bufferevent_enable(bev, EV_READ|EV_WRITE);
					sendbuf[0] = SC_CONFIRM;
					*((uint32 *)&sendbuf[1]) = frame;
					*((long int *)&sendbuf[5]) = interval;
					*((cube *)&sendbuf[9]) = cubelist[cuser];
					cube_print(logfile, cubelist[cuser]);
					fprintf(logfile, "\n");
					evbuffer_add(bufferevent_get_output(bev), sendbuf, sizeof(cube) + 9);
				}
				fflush(logfile);
				pthread_mutex_unlock(&mtx);
				break;
			case CS_UPDATE:
				flag = evbuffer_remove(input, buf, 5 + sizeof(cube));
				pthread_mutex_lock(&mtx);
				printlog(logfile, frame);
				cframe = *((uint32 *)&buf[0]);
				cuser = buf[4];
				fprintf(logfile, "update from user %d: cframe %08lX, ", cuser, cframe);
				cube_print(logfile, *((cube *)&buf[5]));
				fprintf(logfile, "\n");
				if(frame >= cframe){
					cubelist[cuser] = *((cube *)&buf[5]);
					cube_stepforward(&cubelist[cuser], frame - cframe);
				}
				fflush(logfile);
				pthread_mutex_unlock(&mtx);
				break;
		}
	}
}

void on_event(struct bufferevent *bev, short int event, void *arg){
	int user;
	pthread_mutex_lock(&mtx);
	for(int i = 0; i < MAX_PLAYER; i++){
		if(bev == user_bev[i]){
			user = i;
			break;
		}
	}
	pthread_mutex_unlock(&mtx);
    if(event & (BEV_EVENT_EOF | BEV_EVENT_ERROR)){
    	int fd, flag;
        pthread_mutex_lock(&mtx);
        printlog(logfile, frame);
        fprintf(logfile, "user %d: out last status ", user);
        cube_print(logfile, cubelist[user]);
        fprintf(logfile, "\n");
        fflush(logfile);
        bufferevent_free(bev);
        user_bev[user] = NULL;
        fd = open("data.db", O_WRONLY);
        lseek(fd, user * sizeof(cube), SEEK_SET);
        flag = write(fd, (char *)&cubelist[user], sizeof(cube));
        printf("%d\n", flag);
        close(fd);
        pthread_mutex_unlock(&mtx);
    }
    if(event & BEV_EVENT_TIMEOUT){
        printf("  event:Timeout!\n");
    }
    if(event & BEV_EVENT_CONNECTED){
        printf("  event:Connected.\n");
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
	printlog(logfile, frame);
	fprintf(logfile, "ACK from %s: %d\n", str, ntohs(sin->sin_port));
	fflush(logfile);
	pthread_mutex_unlock(&mtx);
	bufferevent_setcb(bev, on_read, NULL, on_event, arg);
	bufferevent_enable(bev, EV_READ);
}

/*Accept Failed*/
void on_acc_error(struct evconnlistener *listener, void *arg){
	struct event_base *base = evconnlistener_get_base(listener);
	int err = EVUTIL_SOCKET_ERROR();
	pthread_mutex_lock(&mtx);
	printlog(logfile, frame);
	printf("error on ACK %d(%s)\n", err, evutil_socket_error_to_string(err));
	fflush(logfile);
	pthread_mutex_unlock(&mtx);
	event_base_loopexit(base, NULL);
}

int main(){
	struct event_base *base;
	struct evconnlistener *listener;
	struct sockaddr_in sin;

	memset(user_bev, 0, sizeof(user_bev));
	logfile = fopen("server.log", "w");
	if(!logfile){
		printf("Server Down!\n");
		return 1;
	}
	/*reading data from file*/
	{
		int fd,res,i;
		char buf[MAXLEN];
		printlog(logfile, frame);
		fprintf(logfile, "opening ./data.db ... ");
		fd = open("data.db", O_RDONLY);
		if(fd < 0){
			fprintf(logfile, "failed[No such file!]\n");
			printlog(logfile, frame);
			fprintf(logfile, "closing ...\n");
			fclose(logfile);
			return 1;
		}
		fprintf(logfile, "successful\n");
		printlog(logfile, frame);
		fprintf(logfile, "reading from file ... ");
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
		if(i < 4){
			fprintf(logfile, "failed[No enough valid data!]\n");
			fclose(logfile);
			close(fd);
			return 1;
		}
		fprintf(logfile, "successful\n");
		close(fd);
	}

	/*new event base*/
	printlog(logfile, frame);
	fprintf(logfile, "creating event base ... ");
	base = event_base_new();
	if(!base){
		fprintf(logfile, "failed\n");
		fclose(logfile);
		return 1;
	}
	fprintf(logfile, "successful\n");

	/*new listen address*/
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(LISTEN_PORT);

	/*new event listener*/
	printlog(logfile, frame);
	fprintf(logfile, "creating event listener ... ");
	listener = evconnlistener_new_bind(base, on_acc, NULL, LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, 4, (struct sockaddr *)&sin, sizeof(sin));
	if(!listener){
		fprintf(logfile, "failed\n");
		fclose(logfile);
		return 1;
	}
	fprintf(logfile, "successful[Listening fd: %d]\n", evconnlistener_get_fd(listener));
	fflush(logfile);
	evconnlistener_set_error_cb(listener, on_acc_error);
	evconnlistener_enable(listener);
	printf("Server Started...\n");

	/*Thread to broadcast status*/
	pthread_create(&tid, NULL, pkgThread, NULL);

	/*start polling*/
	event_base_dispatch(base);
	return 0;
}
