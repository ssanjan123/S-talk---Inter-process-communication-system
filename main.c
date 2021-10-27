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

//Ports and IP of peers
static int myPort;
static int theirPort;
static char* theirIP;

//Socket Descriptor for sockets
int sDr;

//List for sending and receiving messages
LIST *outMsg;
LIST *inMsg;

//Global Mutex for each threads (when in use) 
pthread_mutex_t outMutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t inMutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t onMutex = PTHREAD_MUTEX_INITIALIZER; 

//Strings of messages for each thread in use
static char* messagePrint = NULL;
static char* messageRecieve = NULL;
static char* messageSend = NULL;
static char* messageRead = NULL; 

//Thread initialization
//For Receiving
static pthread_t receivePID;
static pthread_mutex_t bufMutexR = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvailR = PTHREAD_COND_INITIALIZER;
//Thread initialization
//For Printing
static pthread_t printPID;
static pthread_mutex_t bufMutexP = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvailP = PTHREAD_COND_INITIALIZER;
//Thread initialization
//For Reading
static pthread_t readingPID;
static pthread_mutex_t bufMutexRead = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvailRead = PTHREAD_COND_INITIALIZER;
//Thread initialization
//For sending
static pthread_t senderPID;
static pthread_mutex_t bufMutexS = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvailS = PTHREAD_COND_INITIALIZER;


void freeHelper(void *item){
    free(item);
}
void* printThread(){
    while(1){
        //Critical Section
        //Check to see if input list is empty 
        if(ListCount(inMsg) == 0){
            pthread_mutex_lock(&bufMutexP);
            pthread_cond_wait(&bufAvailP,&bufMutexP);
            pthread_mutex_unlock(&bufMutexP);
        }

        //Global Mutex (in use)
        pthread_mutex_lock(&inMutex);
        messagePrint = ListTrim(inMsg);
        pthread_mutex_unlock(&inMutex);

        //Signalling receiver
        pthread_mutex_lock(&bufMutexR);
        pthread_cond_signal(&bufAvailR);
        pthread_mutex_unlock(&bufMutexR);

        //Signalling read
        pthread_mutex_lock(&bufMutexRead);
        pthread_cond_signal(&bufAvailRead);
        pthread_mutex_unlock(&bufMutexRead);
        
        printf("%s\n",messagePrint);

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
        //Check to see if lists are full
        pthread_mutex_lock(&onMutex);
        if((ListCount(outMsg) + ListCount(inMsg)) == LIST_MAX_SIZE){
            pthread_mutex_lock(&bufMutexR);
            pthread_cond_wait(&bufAvailR,&bufMutexR);
            pthread_mutex_unlock(&bufMutexR);
        }
        pthread_mutex_unlock(&onMutex);

        //Global Mutex (in use)
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
    soutRemote.sin_addr.s_addr = inet_addr(theirIP);
    soutRemote.sin_port = htons(theirPort);

    while (1){

        //Critical Section
        //Check to see if output list is empty 
        if (ListCount(outMsg) == 0){
            pthread_mutex_lock(&bufMutexS);
            pthread_cond_wait(&bufAvailS, &bufMutexS);
            pthread_mutex_unlock(&bufMutexS);
        }
        //Global Mutex (in use)
        pthread_mutex_lock(&outMutex);
        messageRead = ListTrim(outMsg);
        pthread_mutex_unlock(&outMutex); 
        
        if(sendto(sDr, messageRead, MSG_MAX_LEN, 0, (struct sockaddr *)&soutRemote, sizeof(soutRemote)) == -1){
            printf("Could not send");
            exit(EXIT_FAILURE);
        }
        //Checking for termination
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
        int numBytes = read(STDIN_FILENO, messageSend, MSG_MAX_LEN);
        messageSend[numBytes-1] = '\0';
        
        //Critical Section
        //Check to see if lists are full
        pthread_mutex_lock(&onMutex); 
        if (ListCount(outMsg) + ListCount(inMsg) == LIST_MAX_SIZE){ 
            pthread_mutex_lock(&bufMutexRead);
            pthread_cond_wait(&bufAvailR,&bufMutexRead);
            pthread_mutex_unlock(&bufMutexRead);
        }
        pthread_mutex_unlock(&onMutex);
        //Global Mutex (in use) 
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
    sin.sin_port = htons(myPort); 
    sDr = socket(PF_INET, SOCK_DGRAM, 0);
    int binding = bind(sDr, (struct sockaddr *)&sin, sizeof(sin));

    if (sDr == -1 || binding != 0)
    {
        perror("Binding Socket Failed!");
        exit(EXIT_FAILURE);
    }

    return;
}

//Sets the IP to a value that
static void setIP(char *remote_name, char *remote_port)
{
	struct addrinfo hints; 
    struct addrinfo *result, *rp;
    struct sockaddr_in *sockinfo;
    char *ip;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
    
    //Check for error
	if (getaddrinfo(remote_name, remote_port, &hints, &result) != 0)
	{
		perror("Wrong IP or port numbers!\n");
        exit(EXIT_FAILURE);
	}
    //Iterating through to get the correct IP address with the hostname provided
	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		if (rp->ai_addr->sa_family == AF_INET)
		{
			sockinfo = (struct sockaddr_in *)rp->ai_addr;
			ip = inet_ntoa(sockinfo->sin_addr);
            theirIP = ip;
			freeaddrinfo(result);
			return;
		}
	}
    printf("error");
    exit(EXIT_FAILURE);
    return;
}

int main(int argc, char **argv)
{
    
    myPort = atoi(argv[1]);
    theirPort = atoi(argv[3]);
    //Sets the IP to a value that corresponds to the machine name
    setIP(argv[2],argv[3]);
    bindSocket();

    outMsg = ListCreate();
    inMsg = ListCreate();
    
    // Creating main 4 threads
    pthread_create(&readingPID, NULL, readThread, NULL);
    pthread_create(&senderPID, NULL, sendThread, NULL);
    pthread_create(&receivePID,NULL,receiveThread,NULL);
    pthread_create(&printPID,NULL,printThread,NULL);

    //Joining and waiting for other threads
    //Read
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

    //Print
    pthread_join(printPID,NULL);
    pthread_mutex_destroy(&bufMutexP);
    pthread_cond_destroy(&bufAvailP);
    if(messagePrint != NULL){
        free(messagePrint);
        messagePrint = NULL;
    }

    //Kill all threads currently in use
    pthread_mutex_destroy(&outMutex);
    pthread_mutex_destroy(&inMutex);
    pthread_mutex_destroy(&onMutex);
    close(sDr);
    return 0;
}