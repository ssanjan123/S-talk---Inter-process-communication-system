#include <stdlib.h>
#include <netdb.h>
#include "list.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h> 
#include "keyboard.h"
#include "sender.h"
#include "screen.h"
#include "receiver.h"

#define MSG_MAX_LEN 1024*4

static pthread_t receiverPID;
static int socketDescriptor;

static pthread_mutex_t *remoteMutex;
static pthread_mutex_t *producersMutex;
static pthread_mutex_t bufAvailMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvail = PTHREAD_COND_INITIALIZER;

static List* send_list;
static List* receive_list;

static char* messageRx = NULL; 

void *receiverThread(void *unused)
{
    struct sockaddr_in sinRemote;
    unsigned int sin_len = sizeof(sinRemote);
    int byterx;
    while (1){
        messageRx = malloc(MSG_MAX_LEN);
        byterx = recvfrom(socketDescriptor,messageRx, MSG_MAX_LEN, 0,(struct sockaddr *)&sinRemote, &sin_len);
        if(byterx == -1){
            printf("receive error from remote");
            exit(EXIT_FAILURE);
        }
        messageRx[byterx] = '\0';
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

        pthread_mutex_lock(remoteMutex);
        {
            List_prepend(receive_list,messageRx);
        }
        pthread_mutex_unlock(remoteMutex); 
        signal_consumer_screen();

    }
    return NULL;
}

void Receiver_init(int socketDescriptor_param,List* send_list_param,List* receive_list_param,pthread_mutex_t *remoteMutex_param,pthread_mutex_t *producersMutex_param){
    socketDescriptor = socketDescriptor_param;
    remoteMutex = remoteMutex_param;
    producersMutex = producersMutex_param;
    send_list = send_list_param;
    receive_list = receive_list_param;
    pthread_create(
        &receiverPID,  
        NULL,          
        receiverThread, 
        NULL);
}

void Receiver_shutdown(void){
    pthread_cancel(receiverPID);
}

void Receiver_wait_to_finish(void){
    pthread_join(receiverPID, NULL);
    pthread_mutex_destroy(&bufAvailMutex);
    pthread_cond_destroy(&bufAvail);
    if(messageRx){
    free(messageRx);
    messageRx = NULL;
}
}

void signal_producer_receiver()
{
    pthread_mutex_lock(&bufAvailMutex);
    {
        pthread_cond_signal(&bufAvail);
    }
    pthread_mutex_unlock(&bufAvailMutex);
}