#include <stdlib.h>
#include <netdb.h>
#include "list.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "keyboard.h"
#include "sender.h"
//#include "screen.h"
//#include "receiver.h"
#include <pthread.h> 


#define MSG_MAX_LEN 1024
#define LIST_MAX_SIZE 100


static int LOCALPORT;
static int REMOTEPORT;
static char* REMOTEIP;
int sDr;
List *outMsg;
List *inMsg;


pthread_mutex_t outMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t inMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t onMutex = PTHREAD_MUTEX_INITIALIZER;

static char* messageRec = NULL;
//For Receiving
static pthread_t receivePID;
static pthread_mutex_t bufMutexR = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvailR = PTHREAD_COND_INITIALIZER;

//For Printing
static pthread_t printPID;
static pthread_mutex_t bufMutexP = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvailP = PTHREAD_COND_INITIALIZER;

void freeHelper(void *item){
    free(item);
}
void* printThread(){
    while(1){
        if(List_count(inMsg) <= 0){
            pthread_mutex_lock(&bufMutexP);
            pthread_cond_wait(&bufAvailP,&bufMutexP);
            pthread_mutex_unlock(&bufMutexP);
        }
        pthread_mutex_lock(&inMutex);
        messageRec = List_trim(inMsg);
        pthread_mutex_unlock(&inMutex);
        //Signalling Receiver
        pthread_mutex_lock(&bufMutexR);
        pthread_cond_signal(&bufAvailR);
        pthread_mutex_unlock(&bufMutexR);
        signal_producer_keyboard();
        puts(messageRec);

        //End condition
        if(strcmp(messageRec,"!") == 0){
            List_free(outMsg,freeHelper);
            List_free(inMsg,freeHelper);
            keyboard_shutdown();
            Sender_shutdown();
            //Receiver_shutdown();
            pthread_cancel(receivePID);
            pthread_cancel(printPID);

        }
        free(messageRec);
        messageRec = NULL;

    }
    return NULL;
}

void* receiveThread(){
//////////////////////////////////////////////////////
 //Check this  

    struct sockaddr_in sinRemote;
    unsigned int sin_len = sizeof(sinRemote);
    while(1){
        
        messageRec = malloc(MSG_MAX_LEN);
        int bytes = recvfrom(sDr, messageRec, 
        MSG_MAX_LEN, 0, (struct sockaddr *) &sinRemote, &sin_len);
        //Check for error
        if(bytes < 0){//Error check for Receiver
            printf("Error in receiving!\n");
            exit(EXIT_FAILURE);
        }

        //NULL terminate the message received
        messageRec[strlen(messageRec)] = '\0';

        //Critical section for receive
        pthread_mutex_lock(&onMutex);
        if((List_count(outMsg) + List_count(inMsg)) == LIST_MAX_SIZE){
            pthread_mutex_lock(&bufMutexR);
            pthread_cond_wait(&bufAvailR,&bufMutexR);
            pthread_mutex_unlock(&bufMutexR);
        }
        pthread_mutex_unlock(&onMutex);

        pthread_mutex_lock(&inMutex);
        List_prepend(inMsg,messageRec);
        pthread_mutex_unlock(&inMutex);

        //Signalling Print    
        pthread_mutex_lock(&bufMutexP);
        pthread_cond_signal(&bufAvailP);
        pthread_mutex_unlock(&bufMutexP);


    }

    return NULL;
}

static void bindSocket()
{
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET; 
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(LOCALPORT); 
    sDr = socket(PF_INET, SOCK_DGRAM, 0);
    int binding = bind(sDr, (struct sockaddr *)&sin, sizeof(sin));

    if (sDr < 0)
    {
        perror("Socket Creation Failed!");
        exit(EXIT_FAILURE);
    }

    if (binding == -1)
    {
        perror("Binding Failed!");
        exit(EXIT_FAILURE);
    }
    return;
}

// Hint to get the IP https://stackoverflow.com/questions/20115295/how-to-print-ip-address-from-getaddrinfo
static void getIp(char *remote_name, char *remote_port)
{
	struct addrinfo hints; 
    struct addrinfo *result, *res;
    struct sockaddr_in *sockinfo;
    char *ip;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if (getaddrinfo(remote_name, remote_port, &hints, &result) != 0)
	{
		fprintf(stderr, "Bad ip name please double check\n");
        exit(EXIT_FAILURE);
	}

	for (res = result; res != NULL; res = res->ai_next)
	{
		if (res->ai_addr->sa_family == AF_INET)
		{
			sockinfo = (struct sockaddr_in *)res->ai_addr;
			ip = inet_ntoa(sockinfo->sin_addr);
            REMOTEIP = ip;
			freeaddrinfo(result);
			return;
		}
	}
    printf("address error");
    exit(EXIT_FAILURE);
    return;
}

int main(int argc, char **args)
{
    char *localPort = args[1];
    char *remoteIp = args[2];
    char *remotePort = args[3];
    LOCALPORT = atoi(localPort);
    REMOTEPORT = atoi(remotePort);
    getIp(remoteIp,remotePort);
    bindSocket();

    outMsg = List_create();
    inMsg = List_create();
    
    

    keyboard_init(outMsg,inMsg,&outMutex,&onMutex);
    Sender_init(sDr,REMOTEIP,REMOTEPORT,outMsg,inMsg,&outMutex,&bufMutexR,&bufAvailR,&receivePID,&printPID);
    //Receiver_init(sDr,outMsg,inMsg,&inMutex,&onMutex);
    pthread_create(&receivePID,NULL,receiveThread,NULL);
    pthread_create(&printPID,NULL,printThread,NULL);
    //Screen_init(sDr,outMsg,inMsg,&inMutex,&bufMutexR,&bufAvailR,&receivePID);

    keyboard_wait_to_finish();
    Sender_wait_to_finish();
    //Receiver_wait_to_finish();
    pthread_join(receivePID,NULL);
    pthread_mutex_destroy(&bufMutexR);
    pthread_cond_destroy(&bufAvailR);
    if(messageRec != NULL){
        free(messageRec);
        messageRec = NULL;
    }

    //Screen_wait_to_finish();
    //Screen
    pthread_join(printPID,NULL);
    pthread_mutex_destroy(&bufMutexP);
    pthread_cond_destroy(&bufAvailP);
    if(messageRec != NULL){
        free(messageRec);
        messageRec = NULL;
    }



    pthread_mutex_destroy(&outMutex);
    pthread_mutex_destroy(&inMutex);
    pthread_mutex_destroy(&onMutex);
    close(sDr);
    return 0;
}