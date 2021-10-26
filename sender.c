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

static pthread_t senderPID = 0;
static pthread_mutex_t *localMutex = NULL;
static pthread_mutex_t itemAvailMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  itemAvail = PTHREAD_COND_INITIALIZER;

static int socketDescriptor = 0 ;
static int REMOTEPORT = 0;
static char* REMOTEIP = NULL;
static LIST* send_list = NULL;
static LIST* receive_list = NULL;
static char* messageRx = NULL;
//for receiving
static pthread_t *receiverPID;
static pthread_mutex_t *TbufMutexR;
static pthread_cond_t  *TbufAvailR;
//for printing
static pthread_t *printerPID;
//static pthread_mutex_t *TbufMutexP;
//static pthread_cond_t  *TbufAvailP;

void free_item1(void *pItem)
{
	free(pItem);
}

void *sendThread(void *unused)
{
    struct sockaddr_in soutRemote;
    unsigned int sout_len = sizeof(soutRemote);
    soutRemote.sin_family = AF_INET;
    soutRemote.sin_addr.s_addr = inet_addr(REMOTEIP);
    soutRemote.sin_port = htons(REMOTEPORT);

    char end[MSG_MAX_LEN];
    end[0] = '!';
    end[strlen(end)] = '\0';

    while (1){
        if (ListCount(send_list) <= 0)
        {
            pthread_mutex_lock(&itemAvailMutex);
            {
                pthread_cond_wait(&itemAvail, &itemAvailMutex);
            }
            pthread_mutex_unlock(&itemAvailMutex);
        }

        pthread_mutex_lock(localMutex);
        {
            messageRx = ListTrim(send_list);
        }
        pthread_mutex_unlock(localMutex);
        
        int byterx = sendto(socketDescriptor, messageRx, MSG_MAX_LEN, 0, (struct sockaddr *)&soutRemote, sout_len);
        if(byterx == -1){
        printf("sending error to remote");
        exit(EXIT_FAILURE);
        }

        bool end2 =(messageRx[strlen(messageRx)-1] == 'n' && messageRx[strlen(messageRx)-2] == 92 && messageRx[0] == '!') && strlen(messageRx) == 3 ;

        if(strcmp(messageRx, end) == 0 || end2){  //messagerx has to be coreect dynamic index
            ListFree(send_list, free_item1);
            ListFree(receive_list, free_item1);
            keyboard_shutdown();
            //Screen_shutdown();
            pthread_cancel(*printerPID);
            //Receiver_shutdown();
            pthread_cancel(*receiverPID);
            Sender_shutdown();
        }
        free(messageRx);
        messageRx = NULL;
        signal_producer_keyboard();
        pthread_mutex_lock(TbufMutexR);
        pthread_cond_signal(TbufAvailR);
        pthread_mutex_unlock(TbufMutexR);

    
    }
    return NULL;
}

void Sender_init(int socketDescriptors,char* REMOTEIPs,int REMOTEPORTs,LIST* send_list_param,LIST* receive_list_param, pthread_mutex_t *localMutex_param,pthread_mutex_t *RMutex_param,pthread_cond_t *RAvail_param,pthread_t *receivePID,pthread_t *printPID){
    localMutex = localMutex_param;
    send_list = send_list_param;
    receive_list = receive_list_param;
    socketDescriptor = socketDescriptors;
    REMOTEIP = REMOTEIPs;
    REMOTEPORT = REMOTEPORTs;
    TbufAvailR = RAvail_param;
    TbufMutexR = RMutex_param;
    receiverPID = receivePID;
    printerPID = printPID;
    pthread_create(
        &senderPID, 
        NULL,       
        sendThread, 
        NULL);
}

void Sender_shutdown(void){
    pthread_cancel(senderPID);
}

void Sender_wait_to_finish(void){
    pthread_join(senderPID,NULL);
    pthread_mutex_destroy(&itemAvailMutex);
    pthread_cond_destroy(&itemAvail);
    if(messageRx){
        free(messageRx);
        messageRx = NULL;
    }
}

void signal_consumer_sender()
{
    pthread_mutex_lock(&itemAvailMutex);
    {
        pthread_cond_signal(&itemAvail);
    }
    pthread_mutex_unlock(&itemAvailMutex);
}
