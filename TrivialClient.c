#include <arpa/inet.h>
#include <linux/input.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
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

#include <GL/glut.h>

#include "util.h"

package pkg;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_t tid, disptid;
int user;
/*
void on_read(struct bufferevent *bev, void *arg){
    struct evbuffer *input = bufferevent_get_input(bev);
    char buf[MAXLEN];
    int flag = 0;
    int total = 0;
    PositionPackage acpkg;

    total = 0;
    while(1){
        flag = evbuffer_remove(input, (char *)&acpkg + total, sizeof(PositionPackage) - total);
        if(flag == sizeof(PositionPackage) - total){
            while((flag = evbuffer_remove(input, buf, sizeof(buf))) > 0);
            break;
        }
        else{
            total += flag;
        }
    }
    pthread_mutex_lock(&mtx);
    pkg = acpkg;
    //pkg_print(pkg);
    pthread_mutex_unlock(&mtx);
}*/

void on_event(struct bufferevent *bev, short event, void *arg){
    struct evbuffer *output = bufferevent_get_output(bev);
    char buf[MAXLEN];
    printf("event begin:%d\n", event);
    if(event & BEV_EVENT_READING){
        printf("Read?\n");
    }
    if(event & BEV_EVENT_WRITING){
        printf("Write?\n");
    }
    if(event & BEV_EVENT_EOF){
        printf("EOF.\n");
        pthread_cancel(tid);
        pthread_cancel(disptid);
        bufferevent_free(bev);
    }
    if(event & BEV_EVENT_ERROR){
        printf("Error!\n");
    }
    if(event & BEV_EVENT_TIMEOUT){
        printf("Timeout!\n");
    }
    if(event & BEV_EVENT_CONNECTED){
        printf("Connected.\n");
        buf[0] = 0x80;
        buf[1] = user & 0xFF;
        *((package *)(&buf[2])) = pkg;
        evbuffer_add(output, buf, sizeof(pkg) + 2);
    }
}
/*
void *SendingThread(void *arg){
    int kbdfd;
    struct input_event event;
    struct evbuffer *output = bufferevent_get_output(arg);
    PositionPackage newpkg;

    kbdfd = open("/dev/input/event1", O_RDONLY);
    if(kbdfd < 0){
        printf("Open device failed!\n");
        return NULL;
    }
    while(1){
        if(read(kbdfd, &event, sizeof(event)) == sizeof(event)){
            if(event.type == EV_KEY && event.value == 1){
                pthread_mutex_lock(&mtx);
                newpkg = pkg;
                switch(event.code){
                    case KEY_W:
                        if(newpkg.vy < 0) newpkg.vy = 0;
                        else newpkg.vy = 1;
                        evbuffer_add(output, (char *)&newpkg, sizeof(newpkg));
                        break;
                    case KEY_S:
                        if(newpkg.vy > 0) newpkg.vy = 0;
                        else newpkg.vy = -1;
                        evbuffer_add(output, (char *)&newpkg, sizeof(newpkg));
                        break;
                    case KEY_A:
                        if(newpkg.vx > 0) newpkg.vx = 0;
                        else newpkg.vx = -1;
                        evbuffer_add(output, (char *)&newpkg, sizeof(newpkg));
                        break;
                    case KEY_D:
                        if(newpkg.vx < 0) newpkg.vx = 0;
                        else newpkg.vx = 1;
                        evbuffer_add(output, (char *)&newpkg, sizeof(newpkg));
                        break;
                }
                pthread_mutex_unlock(&mtx);
            }
        }
    }
}*/

void display(){
    int x, y;
    glClear(GL_COLOR_BUFFER_BIT);       
    glColor3f(1.0, 0, 0);
    pthread_mutex_lock(&mtx);
    x = pkg.position.x;
    y = pkg.position.y;
    pthread_mutex_unlock(&mtx);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(x + 2, y + 2, 0);
    glVertex3f(x - 2, y + 2, 0);
    glVertex3f(x - 2, y - 2, 0);
    glVertex3f(x + 2, y - 2, 0);
    glEnd();
    glFlush();
    glutPostRedisplay();
}

void reshape(int w, int h){
    glViewport (0, 0, (GLsizei) w, (GLsizei) h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-200, 200, -200, 200, 5, 15);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0, 0, 10, 0, 0, 0, 0, 1, 0);
}

void *RenderThread(void *arg){
    glutCreateWindow("Client");
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glMatrixMode(GL_PROJECTION);
    glOrtho(-200, 200, -200, 200, 5, 15);
    glMatrixMode(GL_MODELVIEW);
    gluLookAt(0, 0, 10, 0, 0, 0, 0, 1, 0);
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMainLoop();
    return 0;
}

int main(int argc,char* argv[]){
    struct event_base *base;
    struct evdns_base *dns_base;
    struct bufferevent *bev;
	int fd,res,i;
	struct sockaddr_in sin;
	char recbuf[100];

    if(argc != 3){
        printf("usage: %s <host> <0-1>\n", argv[0]);
        return 1;
    }
    user = strtol(argv[2], NULL, 10);

    /*GL init*/
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_SINGLE);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(600, 600);

    base = event_base_new();
    if(!base){
        printf("Get base failed!\n");
        return 1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(LISTEN_PORT);
    res = inet_pton(AF_INET, argv[1], &sin.sin_addr);
    if(res <= 0){
        printf("Addr failed!\n");
        return 1;
    }

    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
    if(bev == NULL){
        printf("Create bufferevent failed!\n");
        return 1;
    }

    //pthread_create(&tid, NULL, SendingThread, bev);
    pthread_create(&disptid, NULL, RenderThread, bev);
    
    fd = bufferevent_socket_connect(bev, (struct sockaddr*)&sin, sizeof(sin));
    //fd = bufferevent_socket_connect_hostname(bev, dns_base, AF_UNSPEC,"www.google.com",80);
    if(fd < 0){
        printf("Connect failed!\n");
        bufferevent_free(bev);
        return 1;
    }
    bufferevent_setcb(bev, NULL, NULL, on_event, NULL);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
    //evbuffer_add_printf(bufferevent_get_output(bev), "GET /\r\n");
    printf("Start polling\n");
    event_base_dispatch(base);
    printf("Stop polling\n");
    pthread_join(tid, NULL);
	return 0;
}