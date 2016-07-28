#include "util.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

float max(float x, float y){
	return (x >= y)? x : y;
}

float min(float x, float y){
	return (x <= y)? x : y;
}

float mid(float x, float y, float z){
	return min(max(x, y), z);
}

void printlog(FILE *fp, uint32 frame){
	struct timeval tv;
	char timestamp[MAXLEN];

	gettimeofday(&tv, NULL);
	ctime_r(&tv.tv_sec, timestamp);
	timestamp[strlen(timestamp) - 1] = '\0';
	fprintf(fp, "[%s] frame %08lX | ", timestamp, frame);
	fflush(fp);
}

/*====================================*/
point point_new(float x, float y){
	point p;
	p.x = x;
	p.y = y;
	return p;
}

void point_print(point p){
	printf("(%.2f,%.2f)\n", p.x, p.y);
}

float point_euclidean_distance(point p1, point p2, int base){
	float x = p1.x - p2.x;
	float y = p1.y - p2.y;
	x = x * x;
	y = y * y;
	x = x + y;
	return sqrtf(x);
}

int point_intersect(point p1, point p2){
	float l, r, t, b;
	l = max(p1.x, p2.x);
	r = min(p1.x + OBJ_WIDTH, p2.x + OBJ_WIDTH);
	b = max(p1.y, p2.y);
	t = min(p1.y + OBJ_WIDTH, p2.y + OBJ_WIDTH);
	if(l >= r || b >= t) return 0;
	return 1;
}

/*====================================*/
int cube_set_accel(cube *pkgptr, float x, float y){
	if(!pkgptr) return 1;
	pkgptr->accel.x = x;
	pkgptr->accel.y = y;
	return 0;
}

void cube_stepforward(cube *c, int steps){
	point vold;
	if(steps < 0){
		return;
	}
	while(steps){
		/*position change*/
		c->position.x += c->velocity.x * DELTA_T;
		c->position.x = mid(-MAX_WIDTH, c->position.x, MAX_WIDTH - OBJ_WIDTH);
		c->position.y += c->velocity.y * DELTA_T;
		c->position.y = mid(-MAX_HEIGHT, c->position.y, MAX_HEIGHT - OBJ_WIDTH);
		/*velocity change*/
		vold = c->velocity;
		c->velocity.x += c->accel.x * DELTA_T;
		c->velocity.y += c->accel.y * DELTA_T;
		if(vold.x > 0){
			c->velocity.x = mid(0, c->velocity.x, MAX_SPEED);
			if(c->velocity.x == MAX_SPEED || c->velocity.x == 0){
				c->accel.x = 0;
			}
		}
		else if(vold.x < 0){
			c->velocity.x = mid(-MAX_SPEED, c->velocity.x, 0);
			if(c->velocity.x == -MAX_SPEED || c->velocity.x == 0){
				c->accel.x = 0;
			}
		}
		else{
			if(c->accel.x > 0){
				c->velocity.x = mid(0, c->velocity.x, MAX_SPEED);
				if(c->velocity.x == MAX_SPEED || c->velocity.x == 0){
					c->accel.x = 0;
				}
			}
			else if(c->accel.x < 0){
				c->velocity.x = mid(-MAX_SPEED, c->velocity.x, 0);
				if(c->velocity.x == -MAX_SPEED || c->velocity.x == 0){
					c->accel.x = 0;
				}
			}
		}
		if(vold.y > 0){
			c->velocity.y = mid(0, c->velocity.y, MAX_SPEED);
			if(c->velocity.y == MAX_SPEED || c->velocity.y == 0){
				c->accel.y = 0;
			}
		}
		else if(vold.y < 0){
			c->velocity.y = mid(-MAX_SPEED, c->velocity.y, 0);
			if(c->velocity.y == -MAX_SPEED || c->velocity.y == 0){
				c->accel.y = 0;
			}
		}
		else{
			if(c->accel.y > 0){
				c->velocity.y = mid(0, c->velocity.y, MAX_SPEED);
				if(c->velocity.y == MAX_SPEED || c->velocity.y == 0){
					c->accel.y = 0;
				}
			}
			else if(c->accel.y < 0){
				c->velocity.y = mid(-MAX_SPEED, c->velocity.y, 0);
				if(c->velocity.y == -MAX_SPEED || c->velocity.y == 0){
					c->accel.y = 0;
				}
			}
		}
		/*accel change*/
		c->accel.x = c->accel.x * (1 - MAX_ACCEL / (MAX_SPEED + 1));
		c->accel.y = c->accel.y * (1 - MAX_ACCEL / (MAX_SPEED + 1));
		steps--;
	}
}

int cube_add_to_buffer(char *buf, size_t size, cube cb){
	if(size < sizeof(cube)){
		return 1;
	}
	memset(buf, 0, sizeof(cube));
	*((cube *)&buf[0]) = cb;
	return 0;
}

int cube_remove_from_buffer(char *buf, size_t size, cube *cb){
	if(size < sizeof(cube)){
		return 1;
	}
	*cb = *((cube *)&buf[0]);
	return 0;
}

void cube_print(FILE *fp, cube cb){
	fprintf(fp, "p(%.2f,%.2f) ", cb.position.x, cb.position.y);
	fprintf(fp, "v(%.2f,%.2f) ", cb.velocity.x, cb.velocity.y);
	fprintf(fp, "a(%.2f,%.2f)", cb.accel.x, cb.accel.y);
}

/*====================================*/
cube_ptr cube_create_queue(){
	cube_ptr head;
	head = (cube_ptr)malloc(sizeof(cube_node));
	if(!head) return NULL;
	memset(head, 0, sizeof(cube_node));
	head->next = NULL;
	return head;
}

void cube_destroy_queue(cube_ptr head){
	cube_ptr p = head;
	while(head){
		p = head;
		head = head->next;
		free(p);
	}
}

int cube_node_enqueue(cube_ptr head, cube *pkgptr){
	cube_ptr p, q;
	if(!head) return -1;
	p = head;
	while(p->next) p = p->next;
	q = (cube_ptr)malloc(sizeof(cube_node));
	if(!q) return -2;
	q->pkg = *pkgptr;
	q->next = NULL;
	p->next = q;
	return 0;
}

int cube_node_dequeue(cube_ptr head, cube *pkgptr){
	cube_ptr p, q;
	if(!head) return -1;
	if(!head->next) return -2;
	p = head->next;
	q = p->next;
	head->next = q;
	*pkgptr = p->pkg;
	free(p);
	return 0;
}

void cube_print_queue(cube_ptr head){
	if(!head){
		printf("Queue not exists!\n");
	}
	else{
		cube_ptr p = head->next;
		if(!p){
			printf("Empty queue!\n");
		}
		else{
			printf("Head->");
			while(p){
				if(p->next) printf("[(%.1f,%.1f),(%.1f,%.1f)]->", p->pkg.position.x, p->pkg.position.y, p->pkg.velocity.x, p->pkg.velocity.y);
				else printf("[(%.1f,%.1f),(%.1f,%.1f)]\n", p->pkg.position.x, p->pkg.position.y, p->pkg.velocity.x, p->pkg.velocity.y);
				p = p->next;
			}
		}
	}
}

/*====================================*/
keyevent_queue_ptr keyevent_queue_new(uint32 size){
	keyevent_queue_ptr queue = (keyevent_queue_ptr)malloc(sizeof(keyevent_queue));
	if(!queue) return NULL;
	if(size < 5){
		size = 5;
	}
	queue->size = size;
	queue->h = queue->r = 0;
	queue->queue = (keyevent *)malloc(sizeof(keyevent) * (size + 1));
	if(!queue->queue){
		free(queue);
		return NULL;
	}
	return queue;
}

int keyevent_queue_delete(keyevent_queue_ptr queue){
	if(!queue) return 0;
	if(queue->queue){
		free(queue->queue);
	}
	free(queue);
	return 0;
}

int keyevent_queue_isempty(keyevent_queue_ptr queue){
	return (queue->h == queue->r)? 1:0;
}

int keyevent_queue_isfull(keyevent_queue_ptr queue){
	return ((queue->r + 1) % (queue->size + 1) == queue->h)? 1:0;
}

int keyevent_queue_gethead(keyevent_queue_ptr queue, keyevent *event){
	if(!queue) return 1;
	if(!queue->queue) return 1;
	if(keyevent_queue_isempty(queue)) return 1;
	*event = queue->queue[queue->h];
	return 0;
}

int keyevent_queue_enqueue(keyevent_queue_ptr queue, keyevent *event){
	if(!queue) return 1;
	if(!queue->queue) return 1;
	if(keyevent_queue_isfull(queue)) return 1;
	queue->queue[queue->r] = *event;
	queue->r = (queue->r + 1) % (queue->size + 1);
	return 0;
}

int keyevent_queue_dequeue(keyevent_queue_ptr queue, keyevent *event){
	int flag;
	flag = keyevent_queue_gethead(queue, event);
	if(flag) return 1;
	queue->h = (queue->h + 1) % (queue->size + 1);
	return 0;
}

#ifdef UTIL_DEBUG

int main(){
	time_t t = time(NULL);
	float a, b, c;
	point p, q;

	srand(t);
	a = rand() * 1.0f / RAND_MAX * 10;
	b = rand() * 1.0f / RAND_MAX * 10;
	p = point_new(5, 0);
	q = point_new(0, 12);
	point_print(p);
	point_print(q);
	printf("%.2f\n", point_euclidean_distance(p, q, 2));
}

#endif