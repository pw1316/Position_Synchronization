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

package pkg_0;
package pkg_1;
package_ptr queue_0 = NULL;
package_ptr queue_1 = NULL;
int user_in[2] = {0, 0};
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_t tid;

void *pkgThread(void *arg){
	while(1){
		usleep(50000);
		pthread_mutex_lock(&mtx);
		switch((user_in[1] << 1) | user_in[0]){
			case 0:
				break;
			case 1:
				package_node_enqueue(queue_0, &pkg_0);
				package_node_enqueue(queue_1, &pkg_1);
				pkg_0.position.x += pkg_0.velocity.x;
				pkg_0.position.y += pkg_0.velocity.y;
				break;
			case 2:
				package_node_enqueue(queue_0, &pkg_0);
				package_node_enqueue(queue_1, &pkg_1);
				pkg_1.position.x += pkg_1.velocity.x;
				pkg_1.position.y += pkg_1.velocity.y;
				break;
			case 3:
				package_node_enqueue(queue_0, &pkg_0);
				package_node_enqueue(queue_1, &pkg_1);
				pkg_0.position.x += pkg_0.velocity.x;
				pkg_0.position.y += pkg_0.velocity.y;
				pkg_1.position.x += pkg_1.velocity.x;
				pkg_1.position.y += pkg_1.velocity.y;
				break;
		}
		pthread_mutex_unlock(&mtx);	
	}
}

void on_read(struct bufferevent *bev, void *arg){
	struct evbuffer *input = bufferevent_get_input(bev);
	char buf[MAXLEN];
	int flag = 0;
	int total = 0;

    flag = evbuffer_remove(input, buf, sizeof(buf));
    switch((byte)buf[0]){
    	case 0x80:
    		;
    }
    
    pthread_mutex_lock(&mtx);
    //pkg_1.vx = acpkg.vx;
    //pkg_1.vy = acpkg.vy;
    //pkg_print(pkg);
    pthread_mutex_unlock(&mtx);
	//evbuffer_add_buffer(bufferevent_get_output(bev), bufferevent_get_input(bev));
}
/*
void on_write(struct bufferevent *bev, void *arg){
	struct evbuffer *output = bufferevent_get_output(bev);
	PositionPackage pkg_in_queue;
	while(!queue_1->next);
	pthread_mutex_lock(&mtx);
	//printf("SEND:");
	pkg_in_queue = pkg_node_dequeue(queue_1);
	//pkg_print(pkg_in_queue);
	evbuffer_add(output, (char *)&pkg_in_queue, sizeof(pkg_in_queue));
	pthread_mutex_unlock(&mtx);
}*/

void on_event(struct bufferevent *bev, short int event, void *arg){
    printf("Event begin:%d\n", event);
    if(event & BEV_EVENT_READING){
        printf("  event:Read?\n");
    }
    if(event & BEV_EVENT_WRITING){
        printf("  event:Write?\n");
    }
    if(event & BEV_EVENT_EOF){
        printf("  event:EOF.\n");
        pthread_mutex_lock(&mtx);
        pthread_cancel(tid);
        bufferevent_free(bev);
        pthread_mutex_lock(&mtx);
        printf("FIN\n");
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

void on_acc(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *addr, int len, void *arg){
	struct event_base *base = evconnlistener_get_base(listener);
	struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	struct evbuffer *output;
	struct sockaddr_in *sin = (struct sockaddr_in *)addr;
	char str[MAXLEN];
	inet_ntop(AF_INET, &sin->sin_addr, str, MAXLEN);

	printf("ACK from %s:%d\n", str, ntohs(sin->sin_port));
	bufferevent_setcb(bev, on_read, NULL, on_event, arg);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
}

void on_acc_error(struct evconnlistener *listener, void *arg){
	struct event_base *base = evconnlistener_get_base(listener);
	int err = EVUTIL_SOCKET_ERROR();
	printf("Error %d(%s)\n", err, evutil_socket_error_to_string(err));
	event_base_loopexit(base,NULL);
}

int main(){
	struct event_base *base;
	struct evconnlistener *listener;

	int flag;
	int listenfd;
	struct sockaddr_in sin;

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
		res = read(fd, buf, sizeof(buf));
		if(res <= 0){
			printf("Read failed!\n");
			close(fd);
			return 1;
		}
		pkg_0 = *((package *)buf);
		pkg_1 = *((package *)buf + 1);
		close(fd);
		queue_0 = package_create_queue();
		queue_1 = package_create_queue();
		if(!queue_0 || !queue_1){
			printf("No enough memory!\n");
			return 1;
		}
		queue_0->next = NULL;
		queue_1->next = NULL;
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

	/*start polling*/
	printf("Start polling...\n");
	pthread_create(&tid, NULL, pkgThread, NULL);
	event_base_dispatch(base);
	return 0;
}
