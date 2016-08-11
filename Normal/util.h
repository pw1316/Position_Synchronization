#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdio.h>

/* ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼ */

//#define UTIL_DEBUG
//#define SERVER_DEBUG
//#define CLIENT_DEBUG
//#define LOGFILE

/*Network*/
#define LISTEN_PORT 23333
#define MAXLEN 4096

/*Kinematics*/
#define MAX_SPEED 5.0f
#define MAX_ACCEL 1.0f
#define DELTA_T 1.0f
#define THRESHOLD 1
#define FRAME_LEN 33333
/*max number of players*/
#define MAX_PLAYER 2
/*how often CS communicate*/
#define FRAMES_PER_UPDATE 5

/*Message Types*/
#define CS_LOGIN	0x80
/*DR [1 OP][1 user]*/
/*FL [1 OP][1 user]*/
#define CS_UPDATE	0x81
/*DR [1 OP][4 frame][1 user][32 cube]*/
/*FL [1 OP][4 frame][1 user][keyevent]...[keyevent][endevent]*/

#define SC_CONFIRM	0x82
/*DR [1 OP][4 frame][4 interval]*/
/*FL [1 OP]*/

#define SC_START	0x83
/*DR unused*/
/*FL [1 OP][1 num][1 user][32 cube]...[1 user][32 cube]*/

#define SC_FLUSH	0x84
/*DR [1 OP][4 frame][4 interval][1 num][1 user][8 cube]...[1 user][8 cube]*/
/*FL [1 OP][4 frame][1 user][keyevent]...[1 user][keyevent][1 all][endevent]*/

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
void printlog(FILE *fp, uint32 frame, const char *format, ...);//Not thread safe

/*====================================*/
struct __point{
	float x;
	float y;
};
typedef struct __point point;

int point_new(point * p, float x, float y);
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

int cube_set_accel(cube *cb, int key, int value);
void cube_stepforward(cube *c, int steps);
void cube_interpolation(cube *src, cube *dst, int steps);
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

/*====================================*/
struct __ring_buffer{
	uint32 size;
	uint32 h;
	uint32 r;
	char *buf;
};
typedef struct __ring_buffer ring_buffer;
typedef ring_buffer *ring_buffer_ptr;

ring_buffer_ptr ring_buffer_new(uint32 size);//size is at least 5
int ring_buffer_delete(ring_buffer_ptr buf);
int ring_buffer_isempty(ring_buffer_ptr buf);
int ring_buffer_isfull(ring_buffer_ptr buf);
uint32 ring_buffer_used(ring_buffer_ptr buf);
uint32 ring_buffer_left(ring_buffer_ptr buf);
int ring_buffer_enqueue(ring_buffer_ptr buf, char *src, uint32 size);
int ring_buffer_dequeue(ring_buffer_ptr buf, char *dst, uint32 size);
void ring_buffer_print(FILE *fp, ring_buffer_ptr buf);

#endif
