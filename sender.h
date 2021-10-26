#ifndef _SENDER_H_
#define _SENDER_H_
#include "list.h"
#include <pthread.h> 

void Sender_init(int socketDescriptors,char* REMOTEIPs,int REMOTEPORTs,LIST* send_list_param,LIST* receive_list_param, pthread_mutex_t *localMutex_param,pthread_mutex_t *RMutex_param,pthread_cond_t *RAvail_param,pthread_t *receivePID,pthread_t *printPID);
void Sender_shutdown(void);
void Sender_wait_to_finish(void);
void signal_consumer_sender(void);

#endif
