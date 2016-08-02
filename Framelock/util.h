#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdio.h>
#include <sys/time.h>

/* ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼ */

//#define UTIL_DEBUG
//#define SERVER_DEBUG
//#define CLIENT_DEBUG

/*Network*/
#define LISTEN_PORT 23333
#define MAXLEN 4096

/*Kinematics*/
#define MAX_SPEED 2.0f
#define MAX_ACCEL 0.3f
#define DELTA_T 1
#define THRESHOLD 1
/*max number of players*/
#define MAX_PLAYER 2
/*how often CS communicate*/
#define FRAMES_PER_UPDATE 5

/*Message Types*/
#define CS_LOGIN	0x80 //[1 OP][1 user]
#define CS_UPDATE	0x81 //[1 OP][4 frame][1 user][keyevent]...[endevent]
#define SC_CONFIRM	0x82 //[1 OP]
#define SC_START	0x83 //[1 OP][1 num][1 user][8 cube]
#define SC_FLUSH	0x84 //[1 OP][4 frame][1 user][keyevent][1 user][keyevent]...[1 all][endevent]

/*Graphyics*/
#define MAX_HEIGHT 200
#define MAX_WIDTH 300
#define OBJ_WIDTH 10

/*====================================*/
typedef unsigned char byte;
typedef signed long int int32;
typedef unsigned long int uint32;

float max(float x, float y);
float min(float x, float y);
float mid(float x, float y, float z);
void printlog(FILE *fp, uint32 frame);//Not thread safe

/*====================================*/
struct __point{
	float x;
	float y;
};
typedef struct __point point;

point point_new(float x, float y);
void point_print(FILE *fp, point p);
float point_euclidean_distance(point p1, point p2, int base);//base currently not work, default 2
int point_intersect(point p1, point p2);

/*====================================*/
struct __cube{
	point position;
	point velocity;
	point accel;
	int32 valid;
	int32 reserved;
};
typedef struct __cube cube;

int cube_set_accel(cube *pkgptr, float x, float y);
void cube_stepforward(cube *c, int steps);
void cube_print(FILE *fp, cube cb);

/*====================================*/
struct __keyevent{
	uint32 frame;
	long int interval;
	uint32 key;
	int32 value;
};
typedef struct __keyevent keyevent;

struct __keyevent_queue{
	uint32 size;
	uint32 h;
	uint32 r;
	keyevent *queue;
};
typedef struct __keyevent_queue keyevent_queue;
typedef keyevent_queue *keyevent_queue_ptr;

keyevent_queue_ptr keyevent_queue_new(uint32 size);//size is at least 5
int keyevent_queue_delete(keyevent_queue_ptr queue);
int keyevent_queue_isempty(keyevent_queue_ptr queue);
int keyevent_queue_isfull(keyevent_queue_ptr queue);
int keyevent_queue_gethead(keyevent_queue_ptr queue, keyevent *event);
int keyevent_queue_enqueue(keyevent_queue_ptr queue, keyevent *event);
int keyevent_queue_dequeue(keyevent_queue_ptr queue, keyevent *event);

#endif
