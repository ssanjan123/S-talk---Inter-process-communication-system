#include <stdlib.h>
#include <netdb.h>
#include "list.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h> 
#include "keyboard.h"
#include "sender.h"
//#include "screen.h"
//#include "receiver.h"

#define MSG_MAX_LEN 1024*4

static pthread_t keyboardPID;

static pthread_mutex_t *localMutex;
static pthread_mutex_t *producersMutex;
static pthread_mutex_t bufAvailMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvail = PTHREAD_COND_INITIALIZER;

static List* send_list;
static List* receive_list;

static char* messageRx = NULL; 
 //remeber has to be dynamically allocated otherwise does not make sense

void *keyboardThread(void *unused)
{
    while (1){
        messageRx = malloc(MSG_MAX_LEN);
        fgets(messageRx,MSG_MAX_LEN, stdin); //messagerx has to be coreect dynamic index
        messageRx[strlen(messageRx)-1] = '\0';
        pthread_mutex_lock(producersMutex);
        {
            if (List_count(send_list) + List_count(receive_list) == 100){
                    pthread_mutex_lock(&bufAvailMutex);
                    {
                        pthread_cond_wait(&bufAvail,&bufAvailMutex);
                    }
                    pthread_mutex_unlock(&bufAvailMutex);
            }
        }
        pthread_mutex_unlock(producersMutex);

        pthread_mutex_lock(localMutex);
        {

            List_prepend(send_list,messageRx);
        }
        pthread_mutex_unlock(localMutex); 
        signal_consumer_sender();
        
    }
    return NULL;
}

void keyboard_init(List* send_list_param,List* receive_list_param,pthread_mutex_t *localMutex_param,pthread_mutex_t *producersMutex_param){
    localMutex = localMutex_param;
    producersMutex = producersMutex_param;
    send_list = send_list_param;
    receive_list = receive_list_param;
    pthread_create(
        &keyboardPID, 
        NULL,       
        keyboardThread, 
        NULL);
}

void keyboard_shutdown(void){
    //remeber to delete char messageRx[MSG_MAX_LEN] dynamically allocated otherwise does not make sense
    //may be list trim in consumer can also do that ?
    pthread_cancel(keyboardPID);
}

void keyboard_wait_to_finish(void){
    pthread_join(keyboardPID,NULL);
    pthread_mutex_destroy(&bufAvailMutex);
    pthread_cond_destroy(&bufAvail);
    if(messageRx){
        free(messageRx);
        messageRx = NULL;
    }
}

void signal_producer_keyboard()
{
    pthread_mutex_lock(&bufAvailMutex);
    {
        pthread_cond_signal(&bufAvail);
    }
    pthread_mutex_unlock(&bufAvailMutex);
}
