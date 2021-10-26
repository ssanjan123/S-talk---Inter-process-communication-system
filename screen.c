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
#include "screen.h"
//#include "receiver.h"

#define MSG_MAX_LEN 1024*4

static pthread_t screenPID;

static pthread_mutex_t *remoteMutex;
static pthread_mutex_t itemAvailMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  itemAvail = PTHREAD_COND_INITIALIZER;

static int socketDescriptor;

static List* receive_list;
static List* send_list;
static char* messageRx = NULL;
//For receiving
static pthread_t *receiverPID;
static pthread_mutex_t *TbufMutexR;
static pthread_cond_t  *TbufAvailR;
void free_item2(void *pItem)
{
	free(pItem);
}

void *screenThread(void *unused)
{
    char end[MSG_MAX_LEN];
    end[0] = '!';
    end[strlen(end)] = '\0';

    while (1){
        if (List_count(receive_list) <= 0)
        {
            pthread_mutex_lock(&itemAvailMutex);
            {
                pthread_cond_wait(&itemAvail, &itemAvailMutex);
            }
            pthread_mutex_unlock(&itemAvailMutex);
        }

        pthread_mutex_lock(remoteMutex);
        {
            messageRx = List_trim(receive_list);
        }
        pthread_mutex_unlock(remoteMutex);

        pthread_mutex_lock(TbufMutexR);
        pthread_cond_signal(TbufAvailR);
        pthread_mutex_unlock(TbufMutexR);
        signal_producer_keyboard();
        puts(messageRx);
        
        bool end2 =(messageRx[strlen(messageRx)-1] == 'n' && messageRx[strlen(messageRx)-2] == 92 && messageRx[0] == '!') && strlen(messageRx) == 3 ;
        if(strcmp(messageRx, end) == 0 || end2){  //messagerx has to be coreect dynamic index
            List_free(send_list, free_item2);
            List_free(receive_list, free_item2);
            keyboard_shutdown();
            Sender_shutdown();
            //Receiver_shutdown();
            pthread_cancel(*receiverPID);
            Screen_shutdown();
        }
        free(messageRx);
        messageRx = NULL;
    }
    return NULL;
}

void Screen_init(int socketDescriptors, List* send_list_param, List* receive_list_param, pthread_mutex_t *remoteMutex_param,pthread_mutex_t *RMutex_param,pthread_cond_t *RAvail_param,pthread_t *receivePID){
    remoteMutex = remoteMutex_param;
    send_list = send_list_param;
    receive_list = receive_list_param;
    socketDescriptor = socketDescriptors;
    TbufAvailR = RAvail_param;
    TbufMutexR = RMutex_param;
    receiverPID = receivePID;
    pthread_create(
        &screenPID, 
        NULL,       
        screenThread, 
        NULL);
}

void Screen_shutdown(void){
    pthread_cancel(screenPID);
}

void Screen_wait_to_finish(void){
    pthread_join(screenPID,NULL);
    pthread_mutex_destroy(&itemAvailMutex);
    pthread_cond_destroy(&itemAvail);
    if(messageRx){
        free(messageRx);
        messageRx = NULL;
    }
}

void signal_consumer_screen()
{
    pthread_mutex_lock(&itemAvailMutex);
    {
        pthread_cond_signal(&itemAvail);
    }
    pthread_mutex_unlock(&itemAvailMutex);
}