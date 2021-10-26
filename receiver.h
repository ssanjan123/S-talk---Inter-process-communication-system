#ifndef _RECEIVER_H_
#define _RECEIVER_H_
#include "list.h"
#include <pthread.h> 

void Receiver_init(int socketDescriptor_param,List* send_list_param,List* receive_list_param,pthread_mutex_t *remoteMutex_param,pthread_mutex_t *producersMutex_param);
void Receiver_shutdown(void);
void Receiver_wait_to_finish(void);
void signal_producer_receiver(void);
#endif