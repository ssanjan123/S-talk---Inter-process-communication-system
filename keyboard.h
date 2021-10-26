#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_
#include "list.h"
#include <pthread.h> 

void keyboard_init(List* send_list_param,List* receive_list_param,pthread_mutex_t *localMutex_param,pthread_mutex_t *producersMutex_param);
void keyboard_shutdown(void);
void keyboard_wait_to_finish(void);
void signal_producer_keyboard(void);

#endif