#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
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

package pkg[2];
package_ptr queue[4] = {NULL, NULL, NULL, NULL};
int user_in[2] = {0, 0};
struct bufferevent *user_bev[2] = {NULL, NULL};
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_t tid;

void *pkgThread(void *arg){
	int flag;
	while(1){
		usleep(50000);
		pthread_mutex_lock(&mtx);
		flag = (user_in[1] << 1) | user_in[0];
		switch(flag){
			case 0:
				break;
			case 1:
				package_node_enqueue(queue[0], &pkg[0]);
				package_node_enqueue(queue[1], &pkg[1]);
				pkg[0].position.x += pkg[0].velocity.x;
				pkg[0].position.y += pkg[0].velocity.y;
				break;
			case 2:
				package_node_enqueue(queue[2], &pkg[0]);
				package_node_enqueue(queue[3], &pkg[1]);
				pkg[1].position.x += pkg[1].velocity.x;
				pkg[1].position.y += pkg[1].velocity.y;
				break;
			case 3:
				package_node_enqueue(queue[0], &pkg[0]);
				package_node_enqueue(queue[1], &pkg[1]);
				package_node_enqueue(queue[2], &pkg[0]);
				package_node_enqueue(queue[3], &pkg[1]);
				pkg[0].position.x += pkg[0].velocity.x;
				pkg[0].position.y += pkg[0].velocity.y;
				pkg[1].position.x += pkg[1].velocity.x;
				pkg[1].position.y += pkg[1].velocity.y;
				break;
		}
		pthread_mutex_unlock(&mtx);	
	}
}

void on_read(struct bufferevent *bev, void *arg){
	struct evbuffer *input = bufferevent_get_input(bev);
	char buf[MAXLEN];
	package *ptr;
	int flag = 0;
	int user = 0;

    flag = evbuffer_remove(input, buf, sizeof(package) + 2);
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
    		ptr = (package *)&buf[2];
    		pkg[user].velocity.x = ptr->velocity.x;
    		pkg[user].velocity.y = ptr->velocity.y;
    		break;
    }
    pthread_mutex_unlock(&mtx);
}

void on_write(struct bufferevent *bev, void *arg){
	struct evbuffer *output = bufferevent_get_output(bev);
	package pkg_in_queue[2];
	int user;
	pthread_mutex_lock(&mtx);
	user = (bev == user_bev[1])? 1:0;
	pthread_mutex_unlock(&mtx);
	while(!queue[user << 1]->next);
	pthread_mutex_lock(&mtx);
	package_node_dequeue(queue[user << 1], &pkg_in_queue[0]);
	package_node_dequeue(queue[(user << 1) | 1], &pkg_in_queue[1]);
	evbuffer_add(output, (char *)pkg_in_queue, sizeof(pkg_in_queue));
	pthread_mutex_unlock(&mtx);
}

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
    }*/
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

/*Connection Accepted*/
void on_acc(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *addr, int len, void *arg){
	struct event_base *base = evconnlistener_get_base(listener);
	struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	struct evbuffer *output;
	struct sockaddr_in *sin = (struct sockaddr_in *)addr;
	char str[MAXLEN];
	inet_ntop(AF_INET, &sin->sin_addr, str, MAXLEN);

	printf("ACK from %s:%d\n", str, ntohs(sin->sin_port));
	bufferevent_setcb(bev, on_read, on_write, on_event, arg);
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

	int flag;
	int listenfd;
	struct sockaddr_in sin;

	/*reading data from file
	 *binary form
	 *┌-----------┬-----------┬-----------┬-----------┐
	 *|   32bit   |   32bit   |   32bit   |   32bit   |
	 *├-----------┼-----------┼-----------┼-----------┤
	 *|     x     |     y     |    vx     |    vy     |
	 *├-----------┼-----------┼-----------┼-----------┤
	 *|     x     |     y     |    vx     |    vy     |
	 *└-----------┴-----------┴-----------┴-----------┘
	 *Little Endian
	 */
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
		res = read(fd, buf, sizeof(buf));
		if(res <= 0){
			printf("Read failed!\n");
			close(fd);
			return 1;
		}
		pkg[0] = *((package *)buf);
		pkg[1] = *((package *)buf + 1);
		close(fd);
		queue[0] = package_create_queue();
		queue[1] = package_create_queue();
		queue[2] = package_create_queue();
		queue[3] = package_create_queue();
		if(!queue[0] || !queue[1] || !queue[2] || !queue[3]){
			printf("No enough memory!\n");
			return 1;
		}
		queue[0]->next = NULL;
		queue[1]->next = NULL;
		queue[2]->next = NULL;
		queue[3]->next = NULL;
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
	listener = evconnlistener_new_bind(base, on_acc, NULL, LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, 5, (struct sockaddr *)&sin, sizeof(sin));
	if(!listener){
		printf("Create listener failed!\n");
		return 1;
	}
	evconnlistener_set_error_cb(listener, on_acc_error);
	evconnlistener_enable(listener);
	printf("Listen fd:%d\n", evconnlistener_get_fd(listener));

	/*Thread to add package to queue*/
	pthread_create(&tid, NULL, pkgThread, NULL);

	/*start polling*/
	printf("Start polling...\n");
	event_base_dispatch(base);
	return 0;
}
