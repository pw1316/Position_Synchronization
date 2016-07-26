#include <arpa/inet.h>
#include <linux/input.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
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

cube cubelist[MAX_PLAYER];
byte user_in[MAX_PLAYER];
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_t tid, disptid;
int user;
uint32 frame;
long int interval;
struct timeval tv, tvold;

void display(){
    float x, y;
    glClear(GL_COLOR_BUFFER_BIT);
    pthread_mutex_lock(&mtx);
    for(int i = 0; i < MAX_PLAYER; i++){
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
    glViewport (0, 0, (GLsizei) w, (GLsizei) h);
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

void *pkgThread(void *arg){
    long int tmp = 0;

    while(1){
        usleep(1000);
        pthread_mutex_lock(&mtx);
        gettimeofday(&tv, NULL);
        tmp = (tv.tv_sec - tvold.tv_sec) * 1000000;
        tmp += (tv.tv_usec - tvold.tv_usec);
        interval += tmp / 1000;
        if((frame % 3 != 0 && interval >= 33) || (frame % 3 == 0 && interval >= 34)){
            if(frame %3 != 0){
                interval -= 33;
            }
            else{
                interval -= 34;
            }
            frame ++;
            tvold = tv;
            tmp = 0;
            for(int i = 0; i < MAX_PLAYER; i++){
                if(user_in[i] == 0) continue;
                cube_stepforward(&cubelist[i], 1);
                tmp++;
            }
            if(frame % FRAMES_PER_UPDATE == 0){
                char buf[MAXLEN];
                int j;
                buf[0] = CS_UPDATE;
                *((uint32 *)&buf[1]) = frame;
                buf[5] = user & 0xFF;
                *((cube *)&buf[6]) = cubelist[user];
                j+= sizeof(cube);
                //printf("frame %ld:user %d at (%.2f,%.2f)\n", frame, user, cubelist[user].position.x, cubelist[user].position.y);
                evbuffer_add(bufferevent_get_output(arg), buf, 6 + sizeof(cube));
            }
        }
        pthread_mutex_unlock(&mtx);
    }
}

void on_read(struct bufferevent *bev, void *arg){
    struct evbuffer *input = bufferevent_get_input(bev);
    char buf[MAXLEN];
    int flag = 0;
    //uint32 sframe;
    //long int sinterval;
    //byte tmp;

    while(1){
        int j;
        flag = evbuffer_remove(input, buf, 1);
        if(flag <= 0) break;
        switch((byte)buf[0]){
            case SC_CONFIRM:
                //printf("CONFIRM: ");
                j = 0;
                flag = evbuffer_remove(input, buf, sizeof(cube) + 8);
                //printf("%d more bytes | ", flag);
                pthread_mutex_lock(&mtx);
                gettimeofday(&tvold, NULL);
                frame = *((uint32 *)&buf[j]);
                j += sizeof(uint32);
                interval = *((long int *)&buf[j]);
                j += sizeof(long int);
                //printf("frame %ld + %ldms | ", frame, interval);
                cubelist[user] = *((cube *)&buf[j]);
                //printf("initial position (%.2f,%.2f)\n", cubelist[user].position.x, cubelist[user].position.y);
                user_in[user] = 1;
                pthread_create(&tid, NULL, pkgThread, bev);
                pthread_mutex_unlock(&mtx);
            case SC_FLUSH:
                /*j = 0;
                evbuffer_remove(input, buf, sizeof(uint32) + sizeof(long int) + 1);
                sframe = *((uint32 *)&buf[0]);
                j += sizeof(uint32);
                sinterval = *((long int *)&buf[j]);
                j += sizeof(long int);
                tmp = (byte)buf[j];
                j++;
                pthread_mutex_lock(&mtx);
                //memset(user_in, 0, sizeof(user_in));
                while(tmp--){
                    evbuffer_remove(input, buf, sizeof(cube) + 1);
                    if(buf[0] != user){
                        cubelist[(int)buf[0]] = *((cube *)&buf[1]);
                    }
                    user_in[(int)buf[0]] = 1;
                }
                if(sframe > frame){
                    gettimeofday(&tvold, NULL);
                    interval = sinterval;
                    cube_stepforward(&cubelist[user], sframe - frame);
                    frame = sframe;
                }
                pthread_mutex_unlock(&mtx);*/
                break;
        }
    }
}

void on_event(struct bufferevent *bev, short event, void *arg){
    struct evbuffer *output = bufferevent_get_output(bev);
    struct event_base *base = bufferevent_get_base(bev);
    char buf[MAXLEN];
    //printf("event begin:%d\n", event);
    if(event & BEV_EVENT_EOF){
        printf("Logged out.\n");
        pthread_cancel(tid);
        bufferevent_free(bev);
        event_base_loopexit(base, NULL);
    }
    if(event & BEV_EVENT_ERROR){
        printf("Error!\n");
    }
    if(event & BEV_EVENT_TIMEOUT){
        printf("Timeout!\n");
    }
    if(event & BEV_EVENT_CONNECTED){
        printf("Connected.\n");
        buf[0] = CS_LOGIN;
        buf[1] = user & 0xFF;
        evbuffer_add(output, buf, 2);
    }
}
/*
void *SendingThread(void *arg){
    int kbdfd;
    struct input_event event;
    //struct evbuffer *output = bufferevent_get_output(arg);

    kbdfd = open("/dev/input/event1", O_RDONLY);
    if(kbdfd < 0){
        printf("Open device failed!\n");
        return NULL;
    }
    while(1){
        if(read(kbdfd, &event, sizeof(event)) == sizeof(event)){
            if(event.type == EV_KEY && event.value != 2){
                pthread_mutex_lock(&mtx);
                switch(event.code){
                    case KEY_W:
                        printf("KEY  UP   %d\n", event.value);
                        break;
                    case KEY_S:
                        printf("KEY DOWN  %d\n", event.value);
                        break;
                    case KEY_A:
                        printf("KEY LEFT  %d\n", event.value);
                        break;
                    case KEY_D:
                        printf("KEY RIGHT %d\n", event.value);
                        break;
                    case KEY_ESC:
                        bufferevent_free(arg);
                        break;
                }
                pthread_mutex_unlock(&mtx);
            }
        }
    }
}
*/

int main(int argc,char* argv[]){
    struct event_base *base;
    struct bufferevent *bev;
	int fd,res;
	struct sockaddr_in sin;

    if(argc != 3){
        printf("usage: %s <host> <0-3>\n", argv[0]);
        return 1;
    }
    user = strtol(argv[2], NULL, 10);
    printf("User %d\n", user);
    memset(user_in, 0, sizeof(user_in));

    /*GL init*/
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_SINGLE);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(600, 400);

    /*new event base*/
    base = event_base_new();
    if(!base){
        printf("Get base failed!\n");
        return 1;
    }

    /*new server address*/
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(LISTEN_PORT);
    res = inet_pton(AF_INET, argv[1], &sin.sin_addr);
    if(res <= 0){
        printf("Addr failed!\n");
        return 1;
    }

    /*new buffer event*/
    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
    if(bev == NULL){
        printf("Create bufferevent failed!\n");
        return 1;
    }

    /*A thread that listen keybord*/
    //pthread_create(&tid, NULL, SendingThread, bev);

    /*A thread that render with OpenGL*/
    pthread_create(&disptid, NULL, RenderThread, bev);
    
    /*Connect to server*/
    fd = bufferevent_socket_connect(bev, (struct sockaddr*)&sin, sizeof(sin));
    if(fd < 0){
        printf("Connect failed!\n");
        bufferevent_free(bev);
        return 1;
    }
    bufferevent_setcb(bev, on_read, NULL, on_event, NULL);
    bufferevent_enable(bev, EV_READ|EV_WRITE);

    /*start polling*/
    printf("Start polling\n");
    event_base_dispatch(base);

    /*Wait until Xwindow is closed*/
    pthread_join(disptid, NULL);
	return 0;
}