#include <stdlib.h>
#include <netdb.h>
#include "list.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h> 

#define MSG_MAX_LEN 1024
#define LIST_MAX_SIZE 100

static int LOCALPORT;
static int REMOTEPORT;
static char* REMOTEIP;
int sDr;
LIST *outMsg;
LIST *inMsg;


pthread_mutex_t outMutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t inMutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t onMutex = PTHREAD_MUTEX_INITIALIZER; 

static char* messagePrint = NULL;
static char* messageRecieve = NULL;
static char* messageSend = NULL;
static char* messageRead = NULL; 

//For Receiving
static pthread_t receivePID;
static pthread_mutex_t bufMutexR = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvailR = PTHREAD_COND_INITIALIZER;

//For Printing
static pthread_t printPID;
static pthread_mutex_t bufMutexP = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvailP = PTHREAD_COND_INITIALIZER;

//For Reading
static pthread_t readingPID;
static pthread_mutex_t bufMutexRead = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvailRead = PTHREAD_COND_INITIALIZER;

//For sending
static pthread_t senderPID;
static pthread_mutex_t bufMutexS = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvailS = PTHREAD_COND_INITIALIZER;


void freeHelper(void *item){
    free(item);
}
void* printThread(){
    while(1){
        if(ListCount(inMsg) == 0){
            pthread_mutex_lock(&bufMutexP);
            pthread_cond_wait(&bufAvailP,&bufMutexP);
            pthread_mutex_unlock(&bufMutexP);
        }
        pthread_mutex_lock(&inMutex);
        messagePrint = ListTrim(inMsg);
        pthread_mutex_unlock(&inMutex);

        //Signalling Receiver
        pthread_mutex_lock(&bufMutexR);
        pthread_cond_signal(&bufAvailR);
        pthread_mutex_unlock(&bufMutexR);

        //Signalling read
        pthread_mutex_lock(&bufMutexRead);
        pthread_cond_signal(&bufAvailRead);
        pthread_mutex_unlock(&bufMutexRead);
        
        puts(messagePrint);

        //End condition
        if(strcmp(messagePrint,"!") == 0){
            ListFree(outMsg,freeHelper);
            ListFree(inMsg,freeHelper);

            // Shutdown the pthreads
            pthread_cancel(senderPID);
            pthread_cancel(readingPID);
            pthread_cancel(receivePID);
            pthread_cancel(printPID);

        }
        free(messagePrint);
        messagePrint = NULL;

    }
    return NULL;
}

void* receiveThread(){
    struct sockaddr_in sinRemote;
    unsigned int sin_len = sizeof(sinRemote);
    while(1){      
        messageRecieve = malloc(MSG_MAX_LEN);
        int bytes = recvfrom(sDr, messageRecieve, 
        MSG_MAX_LEN, 0, (struct sockaddr *) &sinRemote, &sin_len);
        //Check for error
        if(bytes < 0){//Error check for Receiver
            printf("Error in receiving!\n");
            exit(EXIT_FAILURE);
        }

        //NULL terminate the message received
        messageRecieve[bytes] = '\0';

        //Critical section for receive
        pthread_mutex_lock(&onMutex);
        if((ListCount(outMsg) + ListCount(inMsg)) == LIST_MAX_SIZE){
            pthread_mutex_lock(&bufMutexR);
            pthread_cond_wait(&bufAvailR,&bufMutexR);
            pthread_mutex_unlock(&bufMutexR);
        }
        pthread_mutex_unlock(&onMutex);

        pthread_mutex_lock(&inMutex);
        ListPrepend(inMsg,messageRecieve);
        pthread_mutex_unlock(&inMutex);

        //Signalling Print    
        pthread_mutex_lock(&bufMutexP);
        pthread_cond_signal(&bufAvailP);
        pthread_mutex_unlock(&bufMutexP);
    }
    return NULL;
}

void *sendThread()
{
    struct sockaddr_in soutRemote;
    soutRemote.sin_family = AF_INET;
    soutRemote.sin_addr.s_addr = inet_addr(REMOTEIP);
    soutRemote.sin_port = htons(REMOTEPORT);

    while (1){
        if (ListCount(outMsg) == 0){
            pthread_mutex_lock(&bufMutexS);
            pthread_cond_wait(&bufAvailS, &bufMutexS);
            pthread_mutex_unlock(&bufMutexS);
        }

        pthread_mutex_lock(&outMutex);
        messageRead = ListTrim(outMsg);
        pthread_mutex_unlock(&outMutex); 
        
        if(sendto(sDr, messageRead, MSG_MAX_LEN, 0, (struct sockaddr *)&soutRemote, sizeof(soutRemote)) == -1){
            printf("Could not send");
            exit(EXIT_FAILURE);
        }

        if(strcmp(messageRead, "!") == 0){ 
            ListFree(outMsg, freeHelper);
            ListFree(inMsg, freeHelper);
            pthread_cancel(readingPID);
            pthread_cancel(printPID);
            pthread_cancel(receivePID);
            pthread_cancel(senderPID);
        }
        free(messageRead);
        messageRead = NULL;
        //Signalling read
        pthread_mutex_lock(&bufMutexRead);
        pthread_cond_signal(&bufAvailRead);
        pthread_mutex_unlock(&bufMutexRead);

        //Signaling reciever
        pthread_mutex_lock(&bufMutexR);
        pthread_cond_signal(&bufAvailR);
        pthread_mutex_unlock(&bufMutexR);
    
    }
    return NULL;
}

void *readThread()
{
    while (1){
        messageSend = malloc(MSG_MAX_LEN);
        // fgets(messageSend,MSG_MAX_LEN, stdin); 
        // messageSend[strlen(messageSend)-1] = '\0';
        // Testing with read() instead of fgets
        int numBytes = read(STDIN_FILENO, messageSend, MSG_MAX_LEN);
        messageSend[numBytes-1] = '\0';

        pthread_mutex_lock(&onMutex); 
        if (ListCount(outMsg) + ListCount(inMsg) == 100){ 
            pthread_mutex_lock(&bufMutexRead);
            pthread_cond_wait(&bufAvailR,&bufMutexRead);
            pthread_mutex_unlock(&bufMutexRead);
        }
        pthread_mutex_unlock(&onMutex);

        pthread_mutex_lock(&outMutex); 
        ListPrepend(outMsg,messageSend);
        pthread_mutex_unlock(&outMutex);

        //Signalling sender
        pthread_mutex_lock(&bufMutexS);
        pthread_cond_signal(&bufAvailS);
        pthread_mutex_unlock(&bufMutexS);
        
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

    outMsg = ListCreate();
    inMsg = ListCreate();
    
    // Creating main 4 threads
    pthread_create(&readingPID, NULL, readThread, NULL);
    pthread_create(&senderPID, NULL, sendThread, NULL);
    pthread_create(&receivePID,NULL,receiveThread,NULL);
    pthread_create(&printPID,NULL,printThread,NULL);

    // read
    pthread_join(readingPID,NULL);
    pthread_mutex_destroy(&bufMutexR);
    pthread_cond_destroy(&bufAvailR);
    if(messageSend != NULL){
        free(messageSend);
        messageSend = NULL;
    }

    //Sender
    pthread_join(senderPID, NULL);
    pthread_mutex_destroy(&bufMutexS);
    pthread_cond_destroy(&bufAvailS);
    if(messageRead != NULL){
        free(messageRead);
        messageRead = NULL;
    }

    //Recieve
    pthread_join(receivePID,NULL);
    pthread_mutex_destroy(&bufMutexR);
    pthread_cond_destroy(&bufAvailR);
    if(messageRecieve != NULL){
        free(messageRecieve);
        messageRecieve = NULL;
    }

    //Screen print
    pthread_join(printPID,NULL);
    pthread_mutex_destroy(&bufMutexP);
    pthread_cond_destroy(&bufAvailP);
    if(messagePrint != NULL){
        free(messagePrint);
        messagePrint = NULL;
    }

    pthread_mutex_destroy(&outMutex);
    pthread_mutex_destroy(&inMutex);
    pthread_mutex_destroy(&onMutex);
    close(sDr);
    return 0;
}