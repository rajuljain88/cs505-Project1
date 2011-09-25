#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>


using namespace std;

#define SEND_BUF_SIZE 256
#define RECV_BUF_SIZE 256
#define PORT 8080
#define DEFAULT_DECISION 0
#define NPROC 101


bool checkAndPopulateArgs(int args, char* argv[], bool* loyal, int* port,
		char* hostfile, char* decision);
void waitAndProcessMessages(pthread_t*, int port);
void* recvMesgAndSendToAll(void *);
void sendMessage(struct mesgPackage mesg, char *dest_hostname, int port);
bool isGeneral(char* command);
bool isMessageFromGeneral(char* mesg);
void makeDecision();
void signMessage(int message, char* keyfile, unsigned char* signature);
bool verifySignature(int message, unsigned char* signature, char* certfile);
bool verifySignatures(struct mesgPackage);

int num_hosts = 0;
int attacknum = 0;
int retreatnum = 0;
char localhostname[30];
int localhostindex;
char* hostnames[101];
int decisionCount = 0;
pthread_t threads[2*NPROC];
int connectSockets[2*NPROC];
pthread_mutex_t mutex;
int port;
bool loyal;
int traitordecision;	//0: send mesg like loyal, 1 to forge and 2 to not send the message at all

struct mesgPackage {
	int message;
	unsigned char signatures[NPROC][256];
	bool pid[NPROC];
};
