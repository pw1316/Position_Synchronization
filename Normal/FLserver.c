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
#ifdef LOGFILE
FILE *logfile = NULL;
#endif

void *pkgThread(void *arg){
	struct timeval tv, tvold, timeoutbase, timeout;
	int tmp = 0;
	char buf[MAXLEN];
	keyevent kev;

	pthread_mutex_lock(&mtx);
	#ifdef LOGFILE
	printlog(logfile, frame, "tell all clients to start\n");
	#endif
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
		if(interval >= FRAME_LEN){
			interval -= FRAME_LEN;
			frame ++;
			if(frame % FRAMES_PER_UPDATE == 0){
				if(update_in == 0xFFFFFFFF){
					int j = 5;
					buf[0] = SC_FLUSH;
					*((uint32 *)&buf[1]) = frame;
					for(int i = 0; i < MAX_PLAYER; i++){
						if(user_bev[i] == NULL) continue;
						while(keyevent_queue_isempty(user_ev[i]) == 0){
							keyevent_queue_dequeue(user_ev[i], &kev);
							buf[j] = i;
							*((keyevent *)&buf[j + 1]) = kev;
							j += 1 + sizeof(keyevent);
							cube_set_accel(&cubelist[i], kev.key, kev.value);
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
					#ifdef LOGFILE
					printlog(logfile, frame, "got all updates, flush to all clients ...\n");
					#endif

					for(int i = 0; i < MAX_PLAYER; i++){
						if(user_bev[i] == NULL) continue;
						cube_stepforward(&cubelist[i], 1);
						#ifdef LOGFILE
						printlog(logfile, frame, "user %d: ", i);
						cube_print(logfile, cubelist[i]);
						fprintf(logfile, "\n");
						#endif
					}
					update_in = 0xFFFFFFFC;
				}
				else{
					#ifdef LOGFILE
					printlog(logfile, frame, "start timer ...\n");
					#endif
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
								cube_set_accel(&cubelist[i], kev.key, kev.value);
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
						#ifdef LOGFILE
						printlog(logfile, frame, "got all updates, flush to all clients ...\n");
						#endif

						for(int i = 0; i < MAX_PLAYER; i++){
							if(user_bev[i] == NULL) continue;
							cube_stepforward(&cubelist[i], 1);
							#ifdef LOGFILE
							printlog(logfile, frame, "user %d: ", i);
							cube_print(logfile, cubelist[i]);
							fprintf(logfile, "\n");
							#endif
						}
						update_in = 0xFFFFFFFC;
					}
					else{
						#ifdef LOGFILE
						printlog(logfile, frame, "time out ...\n");
						fflush(logfile);
						#endif
						for(int i = 0; i < MAX_PLAYER; i++){
							int fd, flag;
							if(user_bev[i] == NULL) continue;
							bufferevent_free(user_bev[i]);
							user_bev[i] = NULL;
							fd = open("FL.data", O_WRONLY);
							lseek(fd, i * sizeof(cube), SEEK_SET);
							flag = write(fd, (char *)&cubelist[i], sizeof(cube));
							printf("%d bytes written\n", flag);
							close(fd);
						}
						user_in = 0;
						update_in = 0xFFFFFFFF;
						pthread_mutex_unlock(&mtx);
						return NULL;
					}
				}
			}
			else{
				for(int i = 0; i < MAX_PLAYER; i++){
					if(user_bev[i] == NULL) continue;
					cube_stepforward(&cubelist[i], 1);
					#ifdef LOGFILE
					printlog(logfile, frame, "user %d: ", i);
					cube_print(logfile, cubelist[i]);
					fprintf(logfile, "\n");
					#endif
				}
			}
			#ifdef LOGFILE
			fflush(logfile);
			#endif
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
					#ifdef LOGFILE
					printlog(logfile, frame, "user %d: already in\n", cuser);
					fflush(logfile);
					#endif
					bufferevent_free(bev);
				}
				else{
					user_bev[cuser] = bev;
					user_in ++;
					bufferevent_enable(bev, EV_READ|EV_WRITE);
					buf[0] = SC_CONFIRM;
					#ifdef LOGFILE
					printlog(logfile, frame, "user %d: in ", cuser);
					cube_print(logfile, cubelist[cuser]);
					fprintf(logfile, "\n");
					fflush(logfile);
					#endif
					evbuffer_add(bufferevent_get_output(bev), buf, 1);
					if(user_in == MAX_PLAYER){
						#ifdef LOGFILE
						printlog(logfile, frame, "all user in starting...\n");
						fflush(logfile);
						#endif
						/*Thread to broadcast status*/
						pthread_create(&tid, NULL, pkgThread, arg);
					}
				}
				pthread_mutex_unlock(&mtx);
				break;
			case CS_UPDATE:
				evbuffer_remove(input, buf, 5);
				pthread_mutex_lock(&mtx);
				cframe = *((uint32 *)&buf[0]);
				cuser = buf[4];
				assert(cuser < MAX_PLAYER);
				update_in |= 1 << cuser;
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
				#ifdef LOGFILE
				printlog(logfile, frame, "update from user %d: cframe %08lX\n", cuser, cframe);
				fflush(logfile);
				#endif
				pthread_mutex_unlock(&mtx);
				break;
		}
	}
}

void on_event(struct bufferevent *bev, short int event, void *arg){
	int user = MAX_PLAYER;
	pthread_mutex_lock(&mtx);
	for(int i = 0; i < MAX_PLAYER; i++){
		if(bev == user_bev[i]){
			user = i;
			break;
		}
	}
	assert(user < MAX_PLAYER);
	pthread_mutex_unlock(&mtx);
	if(event & (BEV_EVENT_EOF | BEV_EVENT_ERROR)){
		int fd, flag;
		pthread_mutex_lock(&mtx);
		#ifdef LOGFILE
		printlog(logfile, frame, "user %d: out last status ", user);
		cube_print(logfile, cubelist[user]);
		fprintf(logfile, "\n");
		fflush(logfile);
		#endif
		bufferevent_free(bev);
		user_bev[user] = NULL;
		user_in --;
		fd = open("FL.data", O_WRONLY);
		lseek(fd, user * sizeof(cube), SEEK_SET);
		flag = write(fd, (char *)&cubelist[user], sizeof(cube));
		printf("%d bytes written\n", flag);
		close(fd);
		pthread_mutex_unlock(&mtx);
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
	#ifdef LOGFILE
	printlog(logfile, frame, "ACK from %s: %d\n", str, ntohs(sin->sin_port));
	fflush(logfile);
	#else
	printf("ACK from %s: %d\n", str, ntohs(sin->sin_port));
	#endif
	pthread_mutex_unlock(&mtx);
	bufferevent_setcb(bev, on_read, NULL, on_event, arg);
	bufferevent_enable(bev, EV_READ);
}

/*Accept Failed*/
void on_acc_error(struct evconnlistener *listener, void *arg){
	struct event_base *base = evconnlistener_get_base(listener);
	#ifdef LOGFILE
	int err = EVUTIL_SOCKET_ERROR();
	pthread_mutex_lock(&mtx);
	printlog(logfile, frame, "error on ACK %d(%s)\n", err, evutil_socket_error_to_string(err));
	fflush(logfile);
	pthread_mutex_unlock(&mtx);
	#endif
	event_base_loopexit(base, NULL);
}

int main(){
	struct event_base *base;
	struct evconnlistener *listener;
	struct sockaddr_in sin;

	memset(user_bev, 0, sizeof(user_bev));
	#ifdef LOGFILE
	logfile = fopen("server.log", "w");
	if(!logfile){
		printf("Server Down!\n");
		return 1;
	}
	#endif
	/*reading data from file*/
	{
		int fd,res,i;
		char buf[MAXLEN];
		fd = open("FL.data", O_RDONLY);
		if(fd < 0){
			#ifdef LOGFILE
			printlog(logfile, frame, "opening ./FL.data ... failed[No such file!]\n");
			printlog(logfile, frame, "closing ...\n");
			fclose(logfile);
			#endif
			return 1;
		}
		#ifdef LOGFILE
		printlog(logfile, frame, "opening ./FL.data ... successful\n");
		#endif

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
			#ifdef LOGFILE
			printlog(logfile, frame, "reading from file ... failed[No enough valid data!]\n");
			fclose(logfile);
			#endif
			close(fd);
			return 1;
		}
		#ifdef LOGFILE
		printlog(logfile, frame, "reading from file ... successful\n");
		#endif
		close(fd);
	}

	/*new event queue*/
	for(int i = 0; i < MAX_PLAYER; i++){
		user_ev[i] = keyevent_queue_new(1024);
		if(!user_ev[i]){
			#ifdef LOGFILE
			printlog(logfile, frame, "creating keybord queue %d ... failed\n", i);
			fclose(logfile);
			#endif
			return 1;
		}
		#ifdef LOGFILE
		printlog(logfile, frame, "creating keybord queue %d ... successful\n", i);
		#endif
	}

	/*new event base*/
	base = event_base_new();
	if(!base){
		#ifdef LOGFILE
		printlog(logfile, frame, "creating event base ... failed\n");
		fclose(logfile);
		#endif
		return 1;
	}
	#ifdef LOGFILE
	printlog(logfile, frame, "creating event base ... successful\n");
	#endif

	/*new listen address*/
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(LISTEN_PORT);

	/*new event listener*/
	listener = evconnlistener_new_bind(base, on_acc, base, LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, MAX_PLAYER, (struct sockaddr *)&sin, sizeof(sin));
	if(!listener){
		#ifdef LOGFILE
		printlog(logfile, frame, "creating event listener ... failed\n");
		fclose(logfile);
		#endif
		return 1;
	}
	#ifdef LOGFILE
	printlog(logfile, frame, "creating event listener ... successful[Listening fd: %d]\n", evconnlistener_get_fd(listener));
	fflush(logfile);
	#endif
	evconnlistener_set_error_cb(listener, on_acc_error);
	evconnlistener_enable(listener);
	printf("Server Started...\n");

	/*start polling*/
	event_base_dispatch(base);
	return 0;
}
