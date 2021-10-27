#include <stdlib.h>
#include <netdb.h>
#include "list.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
//#include "keyboard.h"
//#include "sender.h"
//#include "screen.h"
//#include "receiver.h"
#include <pthread.h> 


#define MSG_MAX_LEN 1024
#define LIST_MAX_SIZE 100
//static int endOfMsg = 0;


static int LOCALPORT;
static int REMOTEPORT;
static char* REMOTEIP;
int sDr;
LIST *outMsg;
LIST *inMsg;


pthread_mutex_t outMutex = PTHREAD_MUTEX_INITIALIZER; // local
pthread_mutex_t inMutex = PTHREAD_MUTEX_INITIALIZER; // remote
pthread_mutex_t onMutex = PTHREAD_MUTEX_INITIALIZER; // producer

static char* messageRec = NULL;
//For Receiving
static pthread_t receivePID;
static pthread_mutex_t bufMutexR = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvailR = PTHREAD_COND_INITIALIZER;

//For Printing
static pthread_t printPID;
static pthread_mutex_t bufMutexP = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvailP = PTHREAD_COND_INITIALIZER;

//For Reading
static pthread_t keyboardPID;
static pthread_mutex_t bufMutexB = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvailB = PTHREAD_COND_INITIALIZER;

//For sending
static pthread_t senderPID;
static pthread_mutex_t bufMutexT = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bufAvailT = PTHREAD_COND_INITIALIZER;


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
        messageRec = ListTrim(inMsg);
        pthread_mutex_unlock(&inMutex);

        //Signalling Receiver
        pthread_mutex_lock(&bufMutexR);
        pthread_cond_signal(&bufAvailR);
        pthread_mutex_unlock(&bufMutexR);

        //signal_producer_keyboard();
        pthread_mutex_lock(&bufMutexB);
        pthread_cond_signal(&bufAvailB);
        pthread_mutex_unlock(&bufMutexB);
        
        //puts(messageRec);
        printf("Message: %s\n", messageRec);

        //End condition
        if(strcmp(messageRec,"!") == 0){
            ListFree(outMsg,freeHelper);
            ListFree(inMsg,freeHelper);
            //keyboard_shutdown();
            //Sender_shutdown();
            //Receiver_shutdown();
            pthread_cancel(senderPID);
            pthread_cancel(keyboardPID);
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
        ///////////////////////////messageRec[strlen(messageRec)] = '\0'; 
        messageRec[bytes] = '\0';

        //Critical section for receive
        pthread_mutex_lock(&onMutex);
        if((ListCount(outMsg) + ListCount(inMsg)) == LIST_MAX_SIZE){
            pthread_mutex_lock(&bufMutexR);
            pthread_cond_wait(&bufAvailR,&bufMutexR);
            pthread_mutex_unlock(&bufMutexR);
        }
        pthread_mutex_unlock(&onMutex);

        pthread_mutex_lock(&inMutex);
        ListPrepend(inMsg,messageRec);// was inMsg instead of outMsg
        pthread_mutex_unlock(&inMutex);

        //Signalling Print    
        pthread_mutex_lock(&bufMutexP);
        pthread_cond_signal(&bufAvailP);
        pthread_mutex_unlock(&bufMutexP);
    }
    return NULL;
}

void *sendThread2()
{
    struct sockaddr_in soutRemote;
    soutRemote.sin_family = AF_INET;
    soutRemote.sin_addr.s_addr = inet_addr(REMOTEIP);
    soutRemote.sin_port = htons(REMOTEPORT);

    while (1){
        if (ListCount(outMsg) == 0){
            pthread_mutex_lock(&bufMutexT);
            pthread_cond_wait(&bufAvailT, &bufMutexT);
            pthread_mutex_unlock(&bufMutexT);
        }

        pthread_mutex_lock(&outMutex);
        messageRec = ListTrim(outMsg);
        pthread_mutex_unlock(&outMutex); 
        
        //int returner = sendto(sDr, messageRec, MSG_MAX_LEN, 0, (struct sockaddr *)&soutRemote, sout_len);
        if(sendto(sDr, messageRec, MSG_MAX_LEN, 0, (struct sockaddr *)&soutRemote, sizeof(soutRemote)) == -1){
            printf("Could not send");
            exit(EXIT_FAILURE);
        }

        if(strcmp(messageRec, "!") == 0){ 
            ListFree(outMsg, freeHelper);
            ListFree(inMsg, freeHelper);
            pthread_cancel(keyboardPID);
            pthread_cancel(printPID);
            pthread_cancel(receivePID);
            pthread_cancel(senderPID);
        }
        free(messageRec);
        messageRec = NULL;
        //signal_producer_keyboard();
        pthread_mutex_lock(&bufMutexB);
        pthread_cond_signal(&bufAvailB);
        pthread_mutex_unlock(&bufMutexB);

        //signal_producer_receiver();
        pthread_mutex_lock(&bufMutexR);
        pthread_cond_signal(&bufAvailR);
        pthread_mutex_unlock(&bufMutexR);
    
    }
    return NULL;
}

void *keyboardThread2()
{
    while (1){
        messageRec = malloc(MSG_MAX_LEN);
        // fgets(messageRec,MSG_MAX_LEN, stdin); 

        // messageRec[strlen(messageRec)-1] = '\0';

////////////
        char typeMsg[MSG_MAX_LEN];
        // Read from keyboard
        int numBytes = read(STDIN_FILENO, &typeMsg, MSG_MAX_LEN);
        typeMsg[numBytes-1] = '\0';
////////////
        pthread_mutex_lock(&onMutex); //producersMutex
        if (ListCount(outMsg) + ListCount(inMsg) == 100){ // recieve_list = inmSG
            pthread_mutex_lock(&bufMutexB);
            pthread_cond_wait(&bufAvailB,&bufMutexB);
            pthread_mutex_unlock(&bufMutexB);
        }
        pthread_mutex_unlock(&onMutex);// producersMutex

        pthread_mutex_lock(&outMutex); 
        ListPrepend(outMsg,typeMsg);// replaced messageRec with typeMsg
        pthread_mutex_unlock(&outMutex);

        //signal_consumer_sender();
        pthread_mutex_lock(&bufMutexT);
        pthread_cond_signal(&bufAvailT);
        pthread_mutex_unlock(&bufMutexT);
        
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
    

    //keyboard_init(outMsg,inMsg,&outMutex,&onMutex);    
    //Sender_init(sDr,REMOTEIP,REMOTEPORT,outMsg,inMsg,&outMutex,&bufMutexR);
    //Receiver_init(sDr,outMsg,inMsg,&inMutex,&onMutex);
    //Screen_init(sDr,outMsg,inMsg,&inMutex,&bufMutexR,&bufAvailR,&receivePID);
    pthread_create(&keyboardPID, NULL, keyboardThread2, NULL);
    pthread_create(&senderPID, NULL, sendThread2, NULL);
    pthread_create(&receivePID,NULL,receiveThread,NULL);
    pthread_create(&printPID,NULL,printThread,NULL);

     //Keyboard_wait_to_finish();
    pthread_join(keyboardPID,NULL);
    pthread_mutex_destroy(&bufMutexB);
    pthread_cond_destroy(&bufAvailB);
    if(messageRec != NULL){
        free(messageRec);
        messageRec = NULL;
    }

    //Sender_wait_to_finish(); // :
    pthread_join(senderPID, NULL);
    pthread_mutex_destroy(&bufMutexT);
    pthread_cond_destroy(&bufAvailT);
    // if(messageRec != NULL){
    //     free(messageRec);
    //     messageRec = NULL;
    // }

    //Receiver_wait_to_finish();
    pthread_join(receivePID,NULL);
    pthread_mutex_destroy(&bufMutexR);
    pthread_cond_destroy(&bufAvailR);
    // if(messageRec != NULL){
    //     free(messageRec);
    //     messageRec = NULL;
    // }

    //Screen_wait_to_finish();
    //Screen print
    pthread_join(printPID,NULL);
    pthread_mutex_destroy(&bufMutexP);
    pthread_cond_destroy(&bufAvailP);
    // if(messageRec != NULL){
    //     free(messageRec);
    //     messageRec = NULL;
    // }



    pthread_mutex_destroy(&outMutex);
    pthread_mutex_destroy(&inMutex);
    pthread_mutex_destroy(&onMutex);
    close(sDr);
    return 0;
}