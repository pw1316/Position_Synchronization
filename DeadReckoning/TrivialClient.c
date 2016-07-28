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
pthread_mutex_t keymtx = PTHREAD_MUTEX_INITIALIZER;
pthread_t tid, disptid, keytid;
int user;
uint32 frame;
long int interval;
struct timeval tv, tvold;
keyevent_queue_ptr keyqueue;
FILE *logfile;

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
    keyevent kev;

    while(1){
        usleep(1);
        pthread_mutex_lock(&mtx);
        gettimeofday(&tv, NULL);
        tmp = (tv.tv_sec - tvold.tv_sec) * 1000000;
        tmp = tmp + tv.tv_usec - tvold.tv_usec;
        interval = interval + tmp;
        tvold = tv;
        if((frame % 3 != 0 && interval >= 33333) || (frame % 3 == 0 && interval >= 33334)){
            if(frame %3 != 0){
                interval -= 33333;
            }
            else{
                interval -= 33334;
            }
            frame ++;
            while(keyevent_queue_gethead(keyqueue, &kev) == 0){
                if(kev.frame < frame){
                    keyevent_queue_dequeue(keyqueue, &kev);
                    switch(kev.key){
                        case KEY_W:
                            if(kev.value == 1){
                                cubelist[user].velocity.y = mid(0, cubelist[user].velocity.y, MAX_SPEED);
                                cubelist[user].accel.y = MAX_ACCEL;
                            }
                            else{
                                if(cubelist[user].velocity.y > 0){
                                    cubelist[user].accel.y = -MAX_ACCEL;
                                }
                            }
                            break;
                        case KEY_S:
                            if(kev.value == 1){
                                cubelist[user].velocity.y = mid(-MAX_SPEED, cubelist[user].velocity.y, 0);
                                cubelist[user].accel.y = -MAX_ACCEL;
                            }
                            else{
                                if(cubelist[user].velocity.y < 0){
                                    cubelist[user].accel.y = MAX_ACCEL;
                                }
                            }
                            break;
                        case KEY_A:
                            if(kev.value == 1){
                                cubelist[user].velocity.x = mid(-MAX_SPEED, cubelist[user].velocity.x, 0);
                                cubelist[user].accel.x = -MAX_ACCEL;
                            }
                            else{
                                if(cubelist[user].velocity.x < 0){
                                    cubelist[user].accel.x = MAX_ACCEL;
                                }
                            }
                            break;
                        case KEY_D:
                            if(kev.value == 1){
                                cubelist[user].velocity.x = mid(0, cubelist[user].velocity.x, MAX_SPEED);
                                cubelist[user].accel.x = MAX_ACCEL;
                            }
                            else{
                                if(cubelist[user].velocity.x > 0){
                                    cubelist[user].accel.x = -MAX_ACCEL;
                                }
                            }
                            break;
                        case KEY_ESC:
                            printf("out status ");
                            cube_print(stdout, cubelist[user]);
                            printf("\n");
                            bufferevent_free(arg);
                            event_base_loopexit(bufferevent_get_base(arg), NULL);
                            break;
                    }
                }
                else{
                    break;
                }
            }
            tmp = 0;
            for(int i = 0; i < MAX_PLAYER; i++){
                if(user_in[i] == 0) continue;
                printlog(logfile, frame);
                fprintf(logfile, "user %d: ", i);
                cube_print(logfile, cubelist[i]);
                fprintf(logfile, " -> ");
                cube_stepforward(&cubelist[i], 1);
                cube_print(logfile, cubelist[i]);
                fprintf(logfile, "\n");
                fflush(logfile);
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
                evbuffer_add(bufferevent_get_output(arg), buf, 6 + sizeof(cube));
                printlog(logfile, frame);
                fprintf(logfile, "client %d update to server\n", user);
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
    long int sinterval;
    byte tmp;

    while(1){
        int j;
        flag = evbuffer_remove(input, buf, 1);
        if(flag <= 0) break;
        switch((byte)buf[0]){
            case SC_CONFIRM:
                j = 0;
                flag = evbuffer_remove(input, buf, sizeof(cube) + 8);
                pthread_mutex_lock(&mtx);
                gettimeofday(&tvold, NULL);
                frame = *((uint32 *)&buf[j]);
                j += sizeof(uint32);
                interval = *((long int *)&buf[j]);
                j += sizeof(long int);
                cubelist[user] = *((cube *)&buf[j]);
                user_in[user] = 1;
                pthread_create(&tid, NULL, pkgThread, bev);
                pthread_mutex_unlock(&mtx);
                break;
            case SC_FLUSH:
                j = 0;
                evbuffer_remove(input, buf, 9);
                sframe = *((uint32 *)&buf[0]);
                sinterval = *((long int *)&buf[4]);
                tmp = (byte)buf[8];
                j = 9;
                pthread_mutex_lock(&mtx);
                memset(user_in, 0, sizeof(user_in));
                user_in[user] = 1;
                while(tmp--){
                    evbuffer_remove(input, buf, sizeof(cube) + 1);
                    if(buf[0] != user){
                        cube tmpcube = *((cube *)&buf[1]);
                        if(frame >= sframe){
                            cube_stepforward(&tmpcube, frame - sframe);
                        }
                        else{
                            cube_stepforward(&cubelist[(int)buf[0]], sframe - frame);
                        }
                        if(point_euclidean_distance(tmpcube.position, cubelist[(int)buf[0]].position, 2) > THRESHOLD){
                            cubelist[(int)buf[0]].position.x /= 2;
                            cubelist[(int)buf[0]].position.x += tmpcube.position.x / 2;
                            cubelist[(int)buf[0]].position.y /= 2;
                            cubelist[(int)buf[0]].position.y += tmpcube.position.y / 2;
                            cubelist[(int)buf[0]].velocity = tmpcube.velocity;
                            cubelist[(int)buf[0]].accel = tmpcube.accel;
                        }
                        else{
                            cubelist[(int)buf[0]] = *((cube *)&buf[1]);
                        }
                    }
                    user_in[(int)buf[0]] = 1;
                }
                printlog(logfile, frame);
                fprintf(logfile, "got server flush, sframe %08lX\n", sframe);
                if(sframe > frame){
                    gettimeofday(&tvold, NULL);
                    interval = sinterval;
                    cube_stepforward(&cubelist[user], sframe - frame);
                    frame = sframe;
                }
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

void *SendingThread(void *arg){
    int kbdfd;
    struct input_event event;
    keyevent kev;

    kbdfd = open("/dev/input/event1", O_RDONLY);
    if(kbdfd < 0){
        printf("Open device failed!\n");
        return NULL;
    }
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
    logfile = fopen("client.log", "w");

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
    keyqueue = keyevent_queue_new(1024);
    pthread_create(&keytid, NULL, SendingThread, NULL);

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