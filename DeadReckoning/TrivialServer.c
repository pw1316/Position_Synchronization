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

cube cubelist[MAX_PLAYER];
struct bufferevent *user_bev[MAX_PLAYER];
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_t tid;
uint32 frame;
long int interval = 0;

void *pkgThread(void *arg){
	struct timeval tv, tvold;
	int tmp = 0;

	pthread_mutex_lock(&mtx);
	frame = 0;
	gettimeofday(&tvold, NULL);
	pthread_mutex_unlock(&mtx);
	while(1){
		usleep(1000);
		pthread_mutex_lock(&mtx);
		gettimeofday(&tv, NULL);
		tmp = (tv.tv_sec - tvold.tv_sec) * 1000000;
		tmp += (tv.tv_usec - tvold.tv_usec);
		interval += tmp / 1000;
		if((frame % 3 != 0 && interval >= 330) || (frame % 3 == 0 && interval >= 340)){
			if(frame %3 != 0){
				interval -= 330;
			}
			else{
				interval -= 340;
			}
			frame ++;
			tvold = tv;
			tmp = 0;
			for(int i = 0; i < MAX_PLAYER; i++){
				if(user_bev[i] == NULL) continue;
				cube_stepforward(&cubelist[i], 1);
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
				//printf("UPDATE: frame %ld + %ldms | %d players in", frame, interval, tmp);
				for(int i = 0; i < MAX_PLAYER; i++){
					if(user_bev[i] == NULL) continue;
					//printf(" | %d", i);
					evbuffer_add(bufferevent_get_output(user_bev[i]), buf, j);
				}
				//printf("\n");
			}
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
				printf("LOGIN: ");
				flag = evbuffer_remove(input, buf, 1);
				//printf("%d more bytes | ", flag);
				pthread_mutex_lock(&mtx);
    			if(user_bev[(int)buf[0]]){
    				printf("user %d already in\n", buf[0]);
    				bufferevent_free(bev);
    			}
    			else{
    				int user = buf[0];
    				printf("user %d in\n", user);
    				user_bev[user] = bev;
    				bufferevent_enable(bev, EV_READ|EV_WRITE);
    				sendbuf[0] = SC_CONFIRM;
    				*((uint32 *)&sendbuf[1]) = frame;
    				*((long int *)&sendbuf[5]) = interval;
    				//printf("af frame %ld + %ldms | ", frame, interval);
    				*((cube *)&sendbuf[9]) = cubelist[user];
    				//printf("at (%.2f,%.2f) | ", cubelist[user].position.x, cubelist[user].position.y);
    				//printf("v (%.2f,%.2f) | ", cubelist[user].velocity.x, cubelist[user].velocity.y);
    				//printf("a (%.2f,%.2f)\n", cubelist[user].accel.x, cubelist[user].accel.y);
    				evbuffer_add(bufferevent_get_output(bev), sendbuf, sizeof(cube) + 9);
    			}
    			pthread_mutex_unlock(&mtx);
    			break;
    		case CS_UPDATE:
    			flag = evbuffer_remove(input, buf, 5 + sizeof(cube));
    			pthread_mutex_lock(&mtx);
    			cframe = *((uint32 *)&buf[0]);
    			cuser = buf[4];
    			cubelist[cuser] = *((cube *)&buf[5]);
    			cube_stepforward(&cubelist[cuser], frame - cframe);
    			pthread_mutex_unlock(&mtx);
    			break;
		}
	}
}
/*
void on_write(struct bufferevent *bev, void *arg){
	struct evbuffer *output = bufferevent_get_output(bev);
	cube pkg_in_queue[2];
	int user;
	pthread_mutex_lock(&mtx);
	user = (bev == user_bev[1])? 1:0;
	pthread_mutex_unlock(&mtx);
	while(!queue[user << 1]->next);
	pthread_mutex_lock(&mtx);
	cube_node_dequeue(queue[user << 1], &pkg_in_queue[0]);
	cube_node_dequeue(queue[(user << 1) | 1], &pkg_in_queue[1]);
#ifdef SERVER_DEBUG
	printf("SEND:\n");
	printf("  0:");
	cube_print(pkg_in_queue[0]);
	printf("  1:");
	cube_print(pkg_in_queue[1]);
#endif
	evbuffer_add(output, (char *)pkg_in_queue, sizeof(pkg_in_queue));
	pthread_mutex_unlock(&mtx);
}
*/
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
    if(event & BEV_EVENT_EOF){
        pthread_mutex_lock(&mtx);
        printf("LOGOUT: user %d out | last status ", user);
        cube_print(cubelist[user]);
        printf("\n");
        bufferevent_free(bev);
        user_bev[user] = NULL;
        pthread_mutex_unlock(&mtx);
    }
    if(event & BEV_EVENT_ERROR){
        printf("  event:Error!\n");
        pthread_mutex_lock(&mtx);
        printf("LOGOUT: user %d out | last status ", user);
        cube_print(cubelist[user]);
        printf("\n");
        bufferevent_free(bev);
        user_bev[user] = NULL;
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

	printf("ACK from %s:%d\n", str, ntohs(sin->sin_port));
	bufferevent_setcb(bev, on_read, NULL, on_event, arg);
	bufferevent_enable(bev, EV_READ);
}

/*Accept Failed*/
void on_acc_error(struct evconnlistener *listener, void *arg){
	struct event_base *base = evconnlistener_get_base(listener);
	int err = EVUTIL_SOCKET_ERROR();
	printf("Error %d(%s)\n", err, evutil_socket_error_to_string(err));
	event_base_loopexit(base, NULL);
}

int main(){
	struct event_base *base;
	struct evconnlistener *listener;
	struct sockaddr_in sin;

	memset(user_bev, 0, sizeof(user_bev));
	/*reading data from file*/
	{
		int fd,res,i;
		char buf[MAXLEN];
		fd = open("data.db", O_RDONLY);
		if(fd < 0){
			printf("File open error!\n");
			return 1;
		}
		i = 0;
		while(1){
			res = read(fd, buf, sizeof(cube));
			if(res == sizeof(cube)){
				cubelist[i] = *((cube *)buf);
				i++;
				if(i >= 4) break;
			}
			else{
				printf("Read failed!\n");
				close(fd);
				return 1;
			}
		}
		close(fd);
	}
	printf("Read file done.\n");

	/*new event base*/
	base = event_base_new();
	if(!base){
		printf("Get base failed!\n");
		return 1;
	}

	/*new listen address*/
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(LISTEN_PORT);

	/*new event listener*/
	listener = evconnlistener_new_bind(base, on_acc, NULL, LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, 4, (struct sockaddr *)&sin, sizeof(sin));
	if(!listener){
		printf("Create listener failed!\n");
		return 1;
	}
	evconnlistener_set_error_cb(listener, on_acc_error);
	evconnlistener_enable(listener);
	printf("Listen fd:%d\n", evconnlistener_get_fd(listener));

	/*Thread to broadcast status*/
	pthread_create(&tid, NULL, pkgThread, NULL);

	/*start polling*/
	printf("Start polling...\n");
	event_base_dispatch(base);
	return 0;
}
