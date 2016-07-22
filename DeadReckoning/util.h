#ifndef __UTIL_H__
#define __UTIL_H__

/* ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼ */

//#define DEBUG
//#define SERVER_DEBUG
//#define CLIENT_DEBUG
#define LISTEN_PORT 6666
#define MAXLEN 4096

#define MAX_HEIGHT 30
#define MAX_WIDTH 30
#define OBJ_WIDTH 2

typedef unsigned char byte;
typedef signed long int int32;
typedef unsigned long int uint32;

int32 max(int32 x, int32 y);
int32 min(int32 x, int32 y);

struct __point{
	float x;
	float y;
};
typedef struct __point point;

float point_euclidean_distance(point p1, point p2, int base);//base currently not work, default 2
int point_intersect(point p1, point p2);

struct __package{
	uint32 id;
	point position;
	point velocity;
	int32 valid;
};
typedef struct __package package;

struct __package_node;
typedef struct __package_node package_node;
typedef package_node* package_ptr;
struct __package_node{
	package pkg;
	package_ptr next;
};

int package_set_velocity(package *pkgptr, int32 x, int32 y);
package strtopkg(char* ptr, char** endptr, int base);
void package_print(package pkg);

package_ptr package_create_queue();
void package_destroy_queue(package_ptr head);
int package_node_enqueue(package_ptr head, package *pkgptr);
int package_node_dequeue(package_ptr head, package *pkgptr);
void package_print_queue(package_ptr head);

#endif