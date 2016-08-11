#include "util.h"
#include <linux/input.h>
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

float max(float x, float y){
	assert(x == x);
	assert(y == y);
	return (x >= y)? x : y;
}

float min(float x, float y){
	assert(x == x);
	assert(y == y);
	return (x <= y)? x : y;
}

float mid(float x, float y, float z){
	assert(x == x);
	assert(y == y);
	assert(z == z);
	float tmp = x;
	x = min(x, z);
	z = max(tmp, z);
	return min(max(x, y), z);
}

void printlog(FILE *fp, uint32 frame, const char *format, ...){
	time_t t;
	char timestamp[MAXLEN];
	va_list args;

	assert(fp != NULL);
	va_start(args, format);
	time(&t);
	ctime_r(&t, timestamp);
	timestamp[strlen(timestamp) - 1] = '\0';
	fprintf(fp, "[%s] frame %08lX | ", timestamp, frame);
	vfprintf(fp, format, args);
	va_end(args);
}

/*====================================*/
int point_new(point * p, float x, float y){
	assert(p != NULL);
	p->x = x;
	p->y = y;
	return 0;
}

void point_print(FILE *fp, point p){
	fprintf(fp, "(%.2f,%.2f)", p.x, p.y);
}

float point_euclidean_distance(point p1, point p2, int base){
	float x = p1.x - p2.x, y = p1.y - p2.y;
	return sqrtf(x * x + y * y);
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
int cube_set_accel(cube *cb, int key, int value){
	switch(key){
		case KEY_W:
			if(value == 1){
				cb->velocity.y = mid(0, cb->velocity.y, MAX_SPEED);
				cb->accel.y = MAX_ACCEL;
			}
			else{
				if(cb->velocity.y > 0){
					cb->accel.y = -MAX_ACCEL;
				}
			}
			break;
		case KEY_S:
			if(value == 1){
				cb->velocity.y = mid(-MAX_SPEED, cb->velocity.y, 0);
				cb->accel.y = -MAX_ACCEL;
			}
			else{
				if(cb->velocity.y < 0){
					cb->accel.y = MAX_ACCEL;
				}
			}
			break;
		case KEY_A:
			if(value == 1){
				cb->velocity.x = mid(-MAX_SPEED, cb->velocity.x, 0);
				cb->accel.x = -MAX_ACCEL;
			}
			else{
				if(cb->velocity.x < 0){
					cb->accel.x = MAX_ACCEL;
				}
			}
			break;
		case KEY_D:
			if(value == 1){
				cb->velocity.x = mid(0, cb->velocity.x, MAX_SPEED);
				cb->accel.x = MAX_ACCEL;
			}
			else{
				if(cb->velocity.x > 0){
					cb->accel.x = -MAX_ACCEL;
				}
			}
			break;
		default:
			break;
	}
	return 0;
}

void cube_stepforward(cube *c, int steps){
	point vold;
	if(steps < 0) return;
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
		c->accel.x = c->accel.x * (1 - MAX_ACCEL * DELTA_T / (MAX_SPEED + 1));
		c->accel.y = c->accel.y * (1 - MAX_ACCEL * DELTA_T / (MAX_SPEED + 1));
		steps--;
	}
}

//#define INTERPOLATION_LINEAR
#define INTERPOLATION_SPLINE

void cube_interpolation(cube *src, cube *dst, int steps){
	#ifdef INTERPOLATION_LINEAR
	while(steps > 0){
		float dx, dy, t;
		cube_stepforward(dst, 1);
		dx = dst->position.x - src->position.x;
		dy = dst->position.y - src->position.y;
		t = max(dx / MAX_SPEED, dy / MAX_SPEED);
		if(t < 1){
			*src = *dst;
		}
		else{
			src->position.x = src->position.x * (1.0f - 1.0f / t) + dst->position.x * (1.0f / t);
			src->position.y = src->position.y * (1.0f - 1.0f / t) + dst->position.y * (1.0f / t);
			src->velocity = dst->velocity;
			src->accel = dst->accel;
		}
		steps--;
	}
	#endif

	#ifdef INTERPOLATION_SPLINE
	while(steps > 0){
		float ax, bx, cx, dx, xx1, xx2, vx1, vx2;
		float ay, by, cy, dy, xy1, xy2, vy1, vy2;
		float t;
		cube cb = *dst;
		cube_stepforward(&cb, 5);
		cube_stepforward(dst, 1);
		xx1 = src->position.x; xx2 = cb.position.x;
		vx1 = src->velocity.x; vx2 = cb.velocity.x;
		ax = 2 * (xx1 - xx2) + (vx1 - vx2); bx = 3 * (xx2 - xx1) + vx2 - 2 * vx1; cx = vx1; dx = xx1;
		xy1 = src->position.y; xy2 = cb.position.y;
		vy1 = src->velocity.y; vy2 = cb.velocity.y;
		ay = 2 * (xy1 - xy2) + (vy1 - vy2); by = 3 * (xy2 - xy1) + vy2 - 2 * vy1; cy = vy1; dy = xy1;
		t = 0.2;
		src->position.x = ax * t * t * t + bx * t * t + cx * t + dx;
		src->velocity.x = 3 * ax * t * t + 2 * bx * t + cx;
		src->position.y = ay * t * t * t + by * t * t + cy * t + dy;
		src->velocity.y = 3 * ay * t * t + 2 * by * t + cy;
		src->accel = dst->accel;
		steps--;
	}
	#endif
}

void cube_print(FILE *fp, cube cb){
	fprintf(fp, "p(%.2f,%.2f) ", cb.position.x, cb.position.y);
	fprintf(fp, "v(%.2f,%.2f) ", cb.velocity.x, cb.velocity.y);
	fprintf(fp, "a(%.2f,%.2f)", cb.accel.x, cb.accel.y);
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
	assert(queue != NULL);
	return (queue->h == queue->r)? 1:0;
}

int keyevent_queue_isfull(keyevent_queue_ptr queue){
	assert(queue != NULL);
	return ((queue->r + 1) % (queue->size + 1) == queue->h)? 1:0;
}

int keyevent_queue_gethead(keyevent_queue_ptr queue, keyevent *event){
	assert(queue != NULL);
	assert(queue->queue != NULL);
	if(keyevent_queue_isempty(queue)) return 1;
	*event = queue->queue[queue->h];
	return 0;
}

int keyevent_queue_enqueue(keyevent_queue_ptr queue, keyevent *event){
	assert(queue != NULL);
	assert(queue->queue != NULL);
	if(keyevent_queue_isfull(queue)) return 1;
	queue->queue[queue->r] = *event;
	queue->r = (queue->r + 1) % (queue->size + 1);
	return 0;
}

int keyevent_queue_dequeue(keyevent_queue_ptr queue, keyevent *event){
	assert(queue != NULL);
	assert(queue->queue != NULL);
	if(keyevent_queue_isempty(queue)) return 1;
	*event = queue->queue[queue->h];
	queue->h = (queue->h + 1) % (queue->size + 1);
	return 0;
}

/*====================================*/
ring_buffer_ptr ring_buffer_new(uint32 size){
	ring_buffer_ptr buf = (ring_buffer_ptr)malloc(sizeof(ring_buffer));
	if(!buf) return NULL;
	if(size < 5){
		size = 5;
	}
	buf->size = size;
	buf->h = buf->r = 0;
	buf->buf = (char *)malloc(sizeof(char) * (size + 1));
	if(!buf->buf){
		free(buf);
		return NULL;
	}
	return buf;
}

int ring_buffer_delete(ring_buffer_ptr buf){
	if(!buf) return 0;
	if(buf->buf){
		free(buf->buf);
	}
	free(buf);
	return 0;
}

int ring_buffer_isempty(ring_buffer_ptr buf){
	assert(buf != NULL);
	return (buf->h == buf->r)? 1:0;
}

int ring_buffer_isfull(ring_buffer_ptr buf){
	assert(buf != NULL);
	return ((buf->r + 1) % (buf->size + 1) == buf->h)? 1:0;
}

uint32 ring_buffer_used(ring_buffer_ptr buf){
	assert(buf != NULL);
	return (buf->r + buf->size + 1 - buf->h) % (buf->size + 1);
}

uint32 ring_buffer_left(ring_buffer_ptr buf){
	assert(buf != NULL);
	return buf->size - ring_buffer_used(buf);
}

int ring_buffer_enqueue(ring_buffer_ptr buf, char *src, uint32 size){
	assert(buf != NULL);
	assert(buf->buf != NULL);
	assert(src != NULL);
	if(ring_buffer_left(buf) >= size){
		while(size--){
			buf->buf[buf->r++] = *src++;
			if(buf->r > buf->size){
				buf->r = 0;
			}
		}
		return 0;
	}
	else{
		return 1;
	}
}

int ring_buffer_dequeue(ring_buffer_ptr buf, char *dst, uint32 size){
	assert(buf != NULL);
	assert(buf->buf != NULL);
	assert(dst != NULL);
	if(ring_buffer_used(buf) >= size){
		while(size--){
			*dst++ = buf->buf[buf->h++];
			if(buf->h > buf->size){
				buf->h = 0;
			}
		}
		return 0;
	}
	else{
		return 1;
	}
}

void ring_buffer_print(FILE *fp, ring_buffer_ptr buf){
	assert(fp != NULL);
	assert(buf != NULL);
	assert(buf->buf != NULL);
	uint32 t = buf->h;
	while(t != buf->r){
		fprintf(fp, "%02X ", buf->buf[t++]);
		if(t > buf->size){
			t = 0;
		}
	}
	fprintf(fp, "\nh = %ld, r = %ld\n", buf->h, buf->r);
}

#ifdef UTIL_DEBUG

int main(){
	ring_buffer_ptr pbuffer = ring_buffer_new(MAXLEN);
	char buf[MAXLEN];
	memset(buf, 0, sizeof(buf));
	ring_buffer_print(stdout, pbuffer);
	ring_buffer_enqueue(pbuffer, buf, 6);
	ring_buffer_print(stdout, pbuffer);
	ring_buffer_enqueue(pbuffer, buf, 6);
	ring_buffer_print(stdout, pbuffer);
	ring_buffer_dequeue(pbuffer, buf, 6);
	ring_buffer_print(stdout, pbuffer);
}

#endif