#ifndef __UTIL_H__
#define __UTIL_H__

//#define DEBUG
#define LISTEN_PORT 6666
#define MAXLEN 4096

typedef unsigned char byte;
typedef unsigned long int uint32;

struct __point{
	uint32 x;
	uint32 y;
};
typedef struct __point point;

struct __package{
	point position;
	point velocity;
};
typedef struct __package package;

struct __package_node;
typedef struct __package_node package_node;
typedef package_node* package_ptr;
struct __package_node{
	package pkg;
	package_ptr next;
};

package strtopkg(char* ptr, char** endptr, int base);
void package_print(package pkg);

package_ptr package_create_queue();
void package_destroy_queue(package_ptr head);
int package_node_enqueue(package_ptr head, package *pkgptr);
int package_node_dequeue(package_ptr head, package *pkgptr);
void package_print_queue(package_ptr head);

#endif