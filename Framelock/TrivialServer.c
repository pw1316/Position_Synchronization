#include <arpa/inet.h>
#include <linux/input.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/util.h>

#include "util.h"

/*Sync data*/
struct bufferevent *user_bev[MAX_PLAYER];
cube cubelist[MAX_PLAYER];
keyevent_queue_ptr user_ev[MAX_PLAYER];
uint32 update_in = 0xFFFFFFFF;
uint32 user_in = 0;
uint32 frame = 0xFFFFFFFF;
long int interval = 33333;
/*Multi-thread*/
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_t tid;
/*Log*/
FILE *logfile = NULL;

void *pkgThread(void *arg){
	struct timeval tv, tvold, timeoutbase, timeout;
	int tmp = 0;
	char buf[MAXLEN];
	keyevent kev;

	pthread_mutex_lock(&mtx);
	printlog(logfile, frame, "tell all clients to start\n");
	buf[0] = SC_START;
	buf[1] = MAX_PLAYER;
	for(int i = 0; i < MAX_PLAYER; i++){
		buf[i * 33 + 2] = i;
		*((cube *)&buf[i * 33 + 3]) = cubelist[i];
	}
	for(int i = 0; i < MAX_PLAYER; i++){
		evbuffer_add(bufferevent_get_output(user_bev[i]), buf, 2 + 33 * MAX_PLAYER);
	}
	gettimeofday(&tvold, NULL);
	pthread_mutex_unlock(&mtx);
	while(1){
		usleep(1);
		gettimeofday(&tv, NULL);
		tmp = (tv.tv_sec - tvold.tv_sec) * 1000000;
		tmp = tmp + tv.tv_usec - tvold.tv_usec;
		interval = interval + tmp;
		tvold = tv;
		pthread_mutex_lock(&mtx);
		if((frame % 3 != 0 && interval >= 33333) || (frame % 3 == 0 && interval >= 33334)){
			if(frame %3 != 0){
				interval -= 33333;
			}
			else{
				interval -= 33334;
			}
			frame ++;
			if(frame % FRAMES_PER_UPDATE == 0){
				if(update_in == 0xFFFFFFFF){
					printlog(logfile, frame, "got all updates, flush to all clients ...\n");
					int j = 5;
					buf[0] = SC_FLUSH;
					*((uint32 *)&buf[1]) = frame;
					for(int i = 0; i < MAX_PLAYER; i++){
						keyevent kev;
						if(user_bev[i] == NULL) continue;
						while(keyevent_queue_isempty(user_ev[i]) == 0){
							keyevent_queue_dequeue(user_ev[i], &kev);
							buf[j] = i;
							*((keyevent *)&buf[j + 1]) = kev;
							j += 1 + sizeof(keyevent);
							switch(kev.key){
								case KEY_W:
									if(kev.value == 1){
										cubelist[i].velocity.y = mid(0, cubelist[i].velocity.y, MAX_SPEED);
										cubelist[i].accel.y = MAX_ACCEL;
									}
									else{
										if(cubelist[i].velocity.y > 0){
											cubelist[i].accel.y = -MAX_ACCEL;
										}
									}
									break;
								case KEY_S:
									if(kev.value == 1){
										cubelist[i].velocity.y = mid(-MAX_SPEED, cubelist[i].velocity.y, 0);
										cubelist[i].accel.y = -MAX_ACCEL;
									}
									else{
										if(cubelist[i].velocity.y < 0){
											cubelist[i].accel.y = MAX_ACCEL;
										}
									}
									break;
								case KEY_A:
									if(kev.value == 1){
										cubelist[i].velocity.x = mid(-MAX_SPEED, cubelist[i].velocity.x, 0);
										cubelist[i].accel.x = -MAX_ACCEL;
									}
									else{
										if(cubelist[i].velocity.x < 0){
											cubelist[i].accel.x = MAX_ACCEL;
										}
									}
									break;
								case KEY_D:
									if(kev.value == 1){
										cubelist[i].velocity.x = mid(0, cubelist[i].velocity.x, MAX_SPEED);
										cubelist[i].accel.x = MAX_ACCEL;
									}
									else{
										if(cubelist[i].velocity.x > 0){
											cubelist[i].accel.x = -MAX_ACCEL;
										}
									}
									break;
							}
						}
					}
					kev.value = 2;
					buf[j] = MAX_PLAYER;
					*((keyevent *)&buf[j + 1]) = kev;
					j += 1 + sizeof(keyevent);
					for(int i = 0; i < MAX_PLAYER; i++){
						if(user_bev[i] == NULL) continue;
						evbuffer_add(bufferevent_get_output(user_bev[i]), buf, j);
					}
					for(int i = 0; i < MAX_PLAYER; i++){
						if(user_bev[i] == NULL) continue;
						printlog(logfile, frame, "user %d: ", i);
						cube_print(logfile, cubelist[i]);
						fprintf(logfile, " -> ");
						cube_stepforward(&cubelist[i], 1);
						cube_print(logfile, cubelist[i]);
						fprintf(logfile, "\n");
					}
					update_in = 0xFFFFFFFC;
				}
				else{
					printlog(logfile, frame, "start timer ...\n");
					gettimeofday(&timeoutbase, NULL);
					while(1){
						pthread_mutex_unlock(&mtx);
						usleep(10);
						pthread_mutex_lock(&mtx);
						gettimeofday(&timeout, NULL);
						uint32 tv = timeout.tv_sec - timeoutbase.tv_sec;
						tv *= 1000000;
						tv = tv + timeout.tv_usec - timeoutbase.tv_usec;
						if((tv > 1000000) | (update_in == 0xFFFFFFFF)){
							break;
						}
					}
					if(update_in == 0xFFFFFFFF){
						printlog(logfile, frame, "got all updates, flush to all clients ...\n");
						gettimeofday(&tvold, NULL);
						int j = 5;
						buf[0] = SC_FLUSH;
						*((uint32 *)&buf[1]) = frame;
						for(int i = 0; i < MAX_PLAYER; i++){
							keyevent kev;
							if(user_bev[i] == NULL) continue;
							while(keyevent_queue_isempty(user_ev[i]) == 0){
								keyevent_queue_dequeue(user_ev[i], &kev);
								buf[j] = i;
								*((keyevent *)&buf[j + 1]) = kev;
								j += 1 + sizeof(keyevent);
								switch(kev.key){
									case KEY_W:
										if(kev.value == 1){
											cubelist[i].velocity.y = mid(0, cubelist[i].velocity.y, MAX_SPEED);
											cubelist[i].accel.y = MAX_ACCEL;
										}
										else{
											if(cubelist[i].velocity.y > 0){
												cubelist[i].accel.y = -MAX_ACCEL;
											}
										}
										break;
									case KEY_S:
										if(kev.value == 1){
											cubelist[i].velocity.y = mid(-MAX_SPEED, cubelist[i].velocity.y, 0);
											cubelist[i].accel.y = -MAX_ACCEL;
										}
										else{
											if(cubelist[i].velocity.y < 0){
												cubelist[i].accel.y = MAX_ACCEL;
											}
										}
										break;
									case KEY_A:
										if(kev.value == 1){
											cubelist[i].velocity.x = mid(-MAX_SPEED, cubelist[i].velocity.x, 0);
											cubelist[i].accel.x = -MAX_ACCEL;
										}
										else{
											if(cubelist[i].velocity.x < 0){
												cubelist[i].accel.x = MAX_ACCEL;
											}
										}
										break;
									case KEY_D:
										if(kev.value == 1){
											cubelist[i].velocity.x = mid(0, cubelist[i].velocity.x, MAX_SPEED);
											cubelist[i].accel.x = MAX_ACCEL;
										}
										else{
											if(cubelist[i].velocity.x > 0){
												cubelist[i].accel.x = -MAX_ACCEL;
											}
										}
										break;
								}
							}
						}
						kev.value = 2;
						buf[j] = MAX_PLAYER;
						*((keyevent *)&buf[j + 1]) = kev;
						j += 1 + sizeof(keyevent);
						for(int i = 0; i < MAX_PLAYER; i++){
							if(user_bev[i] == NULL) continue;
							evbuffer_add(bufferevent_get_output(user_bev[i]), buf, j);
						}
						for(int i = 0; i < MAX_PLAYER; i++){
							if(user_bev[i] == NULL) continue;
							printlog(logfile, frame, "user %d: ", i);
							cube_print(logfile, cubelist[i]);
							fprintf(logfile, " -> ");
							cube_stepforward(&cubelist[i], 1);
							cube_print(logfile, cubelist[i]);
							fprintf(logfile, "\n");
						}
						update_in = 0xFFFFFFFC;
					}
					else{
						printlog(logfile, frame, "time out ...\n");
						fflush(logfile);
						pthread_mutex_unlock(&mtx);
						return NULL;
					}
				}
				/*int j;
				buf[0] = SC_FLUSH;
				*((uint32 *)&buf[1]) = frame;
				*((long int *)&buf[5]) = interval;
				buf[9] = tmp & 0xFF;
				j = 10;
				for(int i = 0; i < MAX_PLAYER; i++){
					if(user_bev[i] == NULL) continue;
					buf[j] = i & 0xFF;
					*((cube *)&buf[j+1]) = cubelist[i];
					j += sizeof(cube) + 1;
				}
				printlog(logfile, frame);
				fprintf(logfile, "server flush to %d users", tmp);
				for(int i = 0; i < MAX_PLAYER; i++){
					if(user_bev[i] == NULL) continue;
					fprintf(logfile, " %d", i);
					//evbuffer_add(bufferevent_get_output(user_bev[i]), buf, j);
				}
				fprintf(logfile, "\n");*/
			}
			else{
				for(int i = 0; i < MAX_PLAYER; i++){
					if(user_bev[i] == NULL) continue;
					printlog(logfile, frame, "user %d: ", i);
					cube_print(logfile, cubelist[i]);
					fprintf(logfile, " -> ");
					cube_stepforward(&cubelist[i], 1);
					cube_print(logfile, cubelist[i]);
					fprintf(logfile, "\n");
				}
			}
			fflush(logfile);
		}
		pthread_mutex_unlock(&mtx);
	}
}

void on_read(struct bufferevent *bev, void *arg){
	struct evbuffer *input = bufferevent_get_input(bev);
	char buf[MAXLEN];
	int flag = 0;
	uint32 cframe;
	int cuser;

	while(1){
		flag = evbuffer_remove(input, buf, 1);
		if(flag <= 0) break;
		switch((byte)buf[0]){
			case CS_LOGIN:
				evbuffer_remove(input, buf, 1);
				cuser = buf[0];
				pthread_mutex_lock(&mtx);
				if(user_bev[cuser]){
					printlog(logfile, frame, "user %d: already in\n", cuser);
					bufferevent_free(bev);
				}
				else{
					printlog(logfile, frame, "user %d: in ", cuser);
					user_bev[cuser] = bev;
					user_in ++;
					bufferevent_enable(bev, EV_READ|EV_WRITE);
					buf[0] = SC_CONFIRM;
					cube_print(logfile, cubelist[cuser]);
					fprintf(logfile, "\n");
					evbuffer_add(bufferevent_get_output(bev), buf, 1);
					if(user_in == MAX_PLAYER){
						printlog(logfile, frame, "all user in starting...\n");
						/*Thread to broadcast status*/
						pthread_create(&tid, NULL, pkgThread, NULL);
					}
				}
				fflush(logfile);
				pthread_mutex_unlock(&mtx);
				break;
			case CS_UPDATE:
				evbuffer_remove(input, buf, 5);
				pthread_mutex_lock(&mtx);
				cframe = *((uint32 *)&buf[0]);
				cuser = buf[4];
				assert(cuser < MAX_PLAYER);
				update_in |= 1 << cuser;
				printlog(logfile, frame, "update from user %d: cframe %08lX\n", cuser, cframe);
				while(1){
					keyevent kev;
					evbuffer_remove(input, buf, sizeof(keyevent));
					kev = *((keyevent *)&buf[0]);
					if(kev.value != 2){
						keyevent_queue_enqueue(user_ev[cuser], &kev);
					}
					else{
						break;
					}
				}
				fflush(logfile);
				pthread_mutex_unlock(&mtx);
				break;
		}
	}
}

void on_event(struct bufferevent *bev, short int event, void *arg){
	int user;
	pthread_mutex_lock(&mtx);
	for(int i = 0; i < MAX_PLAYER; i++){
		if(bev == user_bev[i]){
			user = i;
			break;
		}
	}
	pthread_mutex_unlock(&mtx);
	if(event & (BEV_EVENT_EOF | BEV_EVENT_ERROR)){
		int fd, flag;
		pthread_mutex_lock(&mtx);
		printlog(logfile, frame, "user %d: out last status ", user);
		cube_print(logfile, cubelist[user]);
		fprintf(logfile, "\n");
		fflush(logfile);
		bufferevent_free(bev);
		user_bev[user] = NULL;
		user_in --;
		fd = open("data.db", O_WRONLY);
		lseek(fd, user * sizeof(cube), SEEK_SET);
		flag = write(fd, (char *)&cubelist[user], sizeof(cube));
		printf("%d\n", flag);
		close(fd);
		pthread_mutex_unlock(&mtx);
	}
	if(event & BEV_EVENT_TIMEOUT){
		printf("  event:Timeout!\n");
	}
	if(event & BEV_EVENT_CONNECTED){
		printf("  event:Connected.\n");
	}
}

/*Connection Accepted*/
void on_acc(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *addr, int len, void *arg){
	struct event_base *base = evconnlistener_get_base(listener);
	struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	struct sockaddr_in *sin = (struct sockaddr_in *)addr;
	char str[MAXLEN];

	inet_ntop(AF_INET, &sin->sin_addr, str, MAXLEN);
	pthread_mutex_lock(&mtx);
	printlog(logfile, frame, "ACK from %s: %d\n", str, ntohs(sin->sin_port));
	fflush(logfile);
	pthread_mutex_unlock(&mtx);
	bufferevent_setcb(bev, on_read, NULL, on_event, arg);
	bufferevent_enable(bev, EV_READ);
}

/*Accept Failed*/
void on_acc_error(struct evconnlistener *listener, void *arg){
	struct event_base *base = evconnlistener_get_base(listener);
	int err = EVUTIL_SOCKET_ERROR();
	pthread_mutex_lock(&mtx);
	printlog(logfile, frame, "error on ACK %d(%s)\n", err, evutil_socket_error_to_string(err));
	fflush(logfile);
	pthread_mutex_unlock(&mtx);
	event_base_loopexit(base, NULL);
}

int main(){
	struct event_base *base;
	struct evconnlistener *listener;
	struct sockaddr_in sin;

	memset(user_bev, 0, sizeof(user_bev));
	logfile = fopen("server.log", "w");
	if(!logfile){
		printf("Server Down!\n");
		return 1;
	}
	/*reading data from file*/
	{
		int fd,res,i;
		char buf[MAXLEN];
		printlog(logfile, frame, "opening ./data.db ... ");
		fd = open("data.db", O_RDONLY);
		if(fd < 0){
			fprintf(logfile, "failed[No such file!]\n");
			printlog(logfile, frame, "closing ...\n");
			fclose(logfile);
			return 1;
		}
		fprintf(logfile, "successful\n");

		printlog(logfile, frame, "reading from file ... ");
		i = 0;
		while(1){
			res = read(fd, buf, sizeof(cube));
			if(res == sizeof(cube)){
				cubelist[i] = *((cube *)buf);
				i++;
				if(i >= MAX_PLAYER) break;
			}
			else{
				break;
			}
		}
		if(i < MAX_PLAYER){
			fprintf(logfile, "failed[No enough valid data!]\n");
			fclose(logfile);
			close(fd);
			return 1;
		}
		fprintf(logfile, "successful\n");
		close(fd);
	}

	/*new event queue*/
	for(int i = 0; i < MAX_PLAYER; i++){
		printlog(logfile, frame, "creating keybord queue %d ... ", i);
		user_ev[i] = keyevent_queue_new(1024);
		if(!user_ev[i]){
			fprintf(logfile, "failed\n");
			fclose(logfile);
			return 1;
		}
		fprintf(logfile, "successful\n");
	}

	/*new event base*/
	printlog(logfile, frame, "creating event base ... ");
	base = event_base_new();
	if(!base){
		fprintf(logfile, "failed\n");
		fclose(logfile);
		return 1;
	}
	fprintf(logfile, "successful\n");

	/*new listen address*/
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(LISTEN_PORT);

	/*new event listener*/
	printlog(logfile, frame, "creating event listener ... ");
	listener = evconnlistener_new_bind(base, on_acc, NULL, LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, MAX_PLAYER, (struct sockaddr *)&sin, sizeof(sin));
	if(!listener){
		fprintf(logfile, "failed\n");
		fclose(logfile);
		return 1;
	}
	fprintf(logfile, "successful[Listening fd: %d]\n", evconnlistener_get_fd(listener));
	fflush(logfile);
	evconnlistener_set_error_cb(listener, on_acc_error);
	evconnlistener_enable(listener);
	printf("Server Started...\n");

	/*start polling*/
	event_base_dispatch(base);
	return 0;
}
