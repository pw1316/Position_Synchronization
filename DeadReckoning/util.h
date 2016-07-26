#ifndef __UTIL_H__
#define __UTIL_H__

#include <time.h>

/* ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼ */

//#define UTIL_DEBUG
//#define SERVER_DEBUG
//#define CLIENT_DEBUG

/*server port*/
#define LISTEN_PORT 23333
/*socket buffer size*/
#define MAXLEN 4096
/*max number of players*/
#define MAX_PLAYER 4
/*how often CS communicate*/
#define FRAMES_PER_UPDATE 50
/*time 1 frame takes for game*/
#define DELTA_T 1
/*max speed*/
#define MAX_SPEED 5

#define CS_LOGIN	0x80 //[1 OP][1 user]
#define CS_UPDATE	0x81 //[1 OP][4 frame][1+cube entity]
#define SC_CONFIRM	0x82 //[1 OP][4 frame][4 interval]
#define SC_FLUSH	0x83 //[1 OP][4 frame][4 interval][1 num][entities]

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

/*====================================*/
struct __point{
	float x;
	float y;
};
typedef struct __point point;

point point_new(float x, float y);
void point_print(point p);
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
int cube_add_to_buffer(char *buf, size_t size, cube cb);
int cube_remove_from_buffer(char *buf, size_t size, cube *cb);
void cube_print(cube pkg);

/*====================================*/
struct __cube_node;
typedef struct __cube_node cube_node;
typedef cube_node* cube_ptr;
struct __cube_node{
	cube pkg;
	cube_ptr next;
};

cube_ptr cube_create_queue();
void cube_destroy_queue(cube_ptr head);
int cube_node_enqueue(cube_ptr head, cube *pkgptr);
int cube_node_dequeue(cube_ptr head, cube *pkgptr);
void cube_print_queue(cube_ptr head);

#endif