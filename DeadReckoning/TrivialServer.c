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

void *pkgThread(void *arg){
	struct timeval tv, tvbase;
	long int interval = 0;
	long int tmp = 0;

	pthread_mutex_lock(&mtx);
	frame = 0;
	gettimeofday(&tvbase, NULL);
	pthread_mutex_unlock(&mtx);
	while(1){
		usleep(1000);
		pthread_mutex_lock(&mtx);
		gettimeofday(&tv, NULL);
		tmp = (tv.tv_sec - tvold.tv_sec) * 1000000;
		tmp += (tv.tv_usec - tvold.tv_usec);
		interval += tmp / 1000;
		if(frame % 3 != 0 && interval >= 33 || frame % 3 == 0 && interval >= 34){
			if(frame %3 != 0){
				interval -= 33;
			}
			else{
				interval -= 34;
			}
			frame ++;
			tvold = tv;
			for(int i = 0; i < MAX_PLAYER; i++){
				if(user_bev[i] == NULL) continue;
				cube_stepforward(&cubelist[i], 1);
			}
			if(frame % FRAMES_PER_UPDATE == 0){
				char* buf[MAXLEN];
				buf[0] = SC_FLUSH;
				*((uint32 *)&buf[1]) = frame;
				int j = sizeof(uint32) + 1;
				*((long int *)&buf[j]) = interval;
				j += sizeof(long int);
				for(int i = 0; i < MAX_PLAYER; i++){
					if(user_bev[i] == NULL) continue;
					buf[j] = (char)i;
					*((cube *)&buf[j+1]) = cubelist[i];
					j += sizeof(cube) + 1;
				}
				for(int i = 0; i < MAX_PLAYER; i++){
					if(user_bev[i] == NULL) continue;
					printf("user %d send...\n", i);
					evbuffer_add(bufferevent_get_output(user_bev[i]), buf, j);
				}
			}
		}
		pthread_mutex_unlock(&mtx);
	}
}

void on_read(struct bufferevent *bev, void *arg){
	struct evbuffer *input = bufferevent_get_input(bev);
	char buf[MAXLEN];
	cube *ptr;
	point p;
	int flag = 0;
	int user = 0;

	flag = evbuffer_remove(input, buf, 1);
	switch(buf[0]){
		case CS_LOGIN:
			flag = evbuffer_remove(input, buf, 1);
			pthread_mutex_lock(&mtx);
    		if(user_bev[buf[0]]){
    			bufferevent_free(bev);
    		}
    		else{
    			printf("User %d in...\n", buf[0]);
    			user_bev[buf[0]] = bev;
    			bufferevent_enable(bev, EV_READ|EV_WRITE);
    		}
    		pthread_mutex_unlock(&mtx);
    		break;
	}
    while(flag = evbuffer_remove(input, buf, sizeof(cube) + 2) == sizeof(cube) + 2){
    pthread_mutex_lock(&mtx);
    user = buf[1]? 1:0;
    switch((byte)buf[0]){
    	case 0x80:
    		if(user_in[user]){
    			bufferevent_free(bev);
    		}
    		else{
    			printf("User %d in.\n", user);
    			user_in[user] = 1;
    			user_bev[user] = bev;
    			bufferevent_enable(bev, EV_READ|EV_WRITE);
    		}
    		break;
    	case 0x40:
    		ptr = (cube *)&buf[2];
    		p = ptr->velocity;
    		if(p.x == 2) pkg[user].velocity.y = p.y;
    		if(p.y == 2) pkg[user].velocity.x = p.x;
#ifdef SERVER_DEBUG
	printf("GET :\n");
	printf("  %d:", user);
	cube_print(pkg[user]);
#endif
    		break;
    }
    pthread_mutex_unlock(&mtx);
    usleep(16000);
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
*//*
void on_event(struct bufferevent *bev, short int event, void *arg){
	int user;
	pthread_mutex_lock(&mtx);
	user = (bev == user_bev[1])? 1:0;
	pthread_mutex_unlock(&mtx);
    //printf("Event begin:%d\n", event);
    /*if(event & BEV_EVENT_READING){
        printf("  event:Read?\n");
    }*/
    /*if(event & BEV_EVENT_WRITING){
        printf("  event:Write?\n");
    }*//*
    if(event & BEV_EVENT_EOF){
        //printf("  event:EOF.\n");
        pthread_mutex_lock(&mtx);
        printf("User %d out\n", user);
        bufferevent_free(bev);
        user_bev[user] = NULL;
        user_in[user] = 0;
        pthread_mutex_unlock(&mtx);
        //printf("FIN\n");
    }
    if(event & BEV_EVENT_ERROR){
        printf("  event:Error!\n");
    }
    if(event & BEV_EVENT_TIMEOUT){
        printf("  event:Timeout!\n");
    }
    if(event & BEV_EVENT_CONNECTED){
        printf("  event:Connected.\n");
    }
}
*/
/*Connection Accepted*/
void on_acc(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *addr, int len, void *arg){
	struct event_base *base = evconnlistener_get_base(listener);
	struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
	struct evbuffer *output;
	struct sockaddr_in *sin = (struct sockaddr_in *)addr;
	char str[MAXLEN];
	inet_ntop(AF_INET, &sin->sin_addr, str, MAXLEN);

	printf("ACK from %s:%d\n", str, ntohs(sin->sin_port));
	close(fd);
	bufferevent_setcb(bev, NULL, NULL, on_event, arg);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
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

	int flag;
	int listenfd;
	struct sockaddr_in sin;

	memset(user_bev, 0, sizeof(user_bev));
	/*reading data from file*/
	{
		int fd,res,i;
		char buf[MAXLEN];
		char *p;
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
