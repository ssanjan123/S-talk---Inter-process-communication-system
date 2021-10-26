#ifndef _SCREEN_H_
#define _SCREEN_H_
#include "list.h"
#include <pthread.h> 

void Screen_init(int socketDescriptors, List* send_list_param, List* receive_list_param, pthread_mutex_t *remoteMutex_param,pthread_mutex_t *RMutex_param,pthread_cond_t *RAvail_param,pthread_t *receivePID);
void Screen_shutdown(void);
void Screen_wait_to_finish(void);
void signal_consumer_screen(void);

#endif