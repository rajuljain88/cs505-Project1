#include <iostream>
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

using namespace std;

#define SEND_BUF_SIZE 256
#define RECV_BUF_SIZE 256
#define PORT 8080
#define DEFAULT "retreat"

bool checkAndPopulateArgs(int args, char*  argv[], bool* loyal, int* port, char* hostfile, char* decision);
void waitAndProcessMessages(pthread_t*, int port);
void* recvMesgAndSendToAll(void *);
void sendMessage(char * mesg, char *dest_hostname, int port);
bool isGeneral(char* hostname, char* hostnames[], char* command);
bool isMessageFromGeneral(char* mesg);
void makeDecision();

int num_hosts = 0;
char* decisions[100];
char localhostname[30];
char* hostnames[100];
int decisionCount = 0;
pthread_t* threads;
pthread_mutex_t mutex;
int port;

int main(int args, char* argv[])
{	
	bool loyal;
	char* hostfile;
	char* command;
	char line[30];
	FILE* fr;
	gethostname(localhostname, 30);
 	hostfile = new char[30];
	command = new char[30];
	checkAndPopulateArgs(args, argv, &loyal, &port, hostfile, command);
	
	fr = fopen(hostfile, "r");
	if(fr==NULL){
		perror("file not found: ");
		exit(1);
	}
	while(fgets(line, 30, fr)!=NULL){
		hostnames[num_hosts] = new char[30];
		strncpy(hostnames[num_hosts++], line, 20);
	}
	threads= new pthread_t[num_hosts];
	if(isGeneral(localhostname, hostnames, command)){
		strcat(command, localhostname);
		for(int i=0; i<num_hosts; i++){
			if(strcmp(hostnames[i], localhostname)!=0){
				sendMessage(command, hostnames[i], port);
			}
		}
	}else{
		waitAndProcessMessages(threads, port);	
	}
	/*
	char mesg[30];
	strcpy(mesg, "rajul"); 
	gethostname(myhostname, 30);
	if(strcmp(argv[1],"server")==0){
		waitAndProcessMessages(threads, 8080);
	}else if(strcmp(argv[1],"client")==0){
		sendMessage(myhostname, mesg, "xinu01.cs.purdue.edu", 8080);
	}
	*/
	pthread_mutex_destroy(&mutex);
}

bool isGeneral(char* hostname, char* hostnames[], char* command){
	return (strcmp(command, "attack")==0 || strcmp(command, "retreat")==0);
}

bool isMessageFromGeneral(char* mesg){
	return !(strstr(mesg, hostnames[0]) == NULL);
}

void makeDecision(){
	int numAttack=0;
	int numRetreat=0;
	for(int i=0; i<num_hosts-1;i++){
		if(strcmp(decisions[i], "attack")==0){
			numAttack++;
		}else if(strcmp(decisions[i], "retreat")==0){
			numRetreat++;
		}
	}
	printf("Num Attack = %d Num Retreat=%d\n", numAttack, numRetreat);
	if(numAttack > numRetreat){
		printf("Decision: Attack\n");
	}else if(numAttack <= numRetreat){
		printf("Decision: Retreat\n");
	}
}

void waitAndProcessMessages(pthread_t*  threads, int port){
	int sock, connectSockFd;
	int true1=1;
	char* sendCmd;
	struct sockaddr_in serv_address, cli_address;
	int sin_size;
	int recv_len;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock==-1){
		perror("Error creating socket");
		exit(1);
	}
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &true1, sizeof(int))==-1){
		perror("Error setting socket options");
		exit(1);
	} 
	serv_address.sin_family = AF_INET;
	serv_address.sin_port = htons(port);
	serv_address.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_address.sin_zero), 8);
	
	if(bind(sock, (struct sockaddr *)&serv_address, sizeof(struct sockaddr))==-1){
		perror("Error binding to the socket");
		exit(1);
	}
	if(listen(sock, 5)==-1){
		perror("Error listening to the socket");
		exit(1);
	}
	
	for(int i=0; i<num_hosts-1; i++){
		connectSockFd = accept(sock, (struct sockaddr *)&cli_address,(socklen_t *)&sin_size);
		if(connectSockFd==-1){
			perror("Error in accept");
			break;
		}
		pthread_create(&threads[i], NULL, recvMesgAndSendToAll, (void *)&connectSockFd);
	}
	for(int j=0; j<num_hosts-1; j++){
		pthread_join(threads[j], NULL);
	}
	makeDecision();
}	
void *recvMesgAndSendToAll(void* connectSockFd){
	printf("Entering recvMesgAndSendToAll\n");
	bool isMesgFromGen = false;
	char recvCmd[RECV_BUF_SIZE];
	int x = recv(*(int *)connectSockFd, recvCmd, RECV_BUF_SIZE, 0);
	cout<<x<<endl;
	isMesgFromGen = isMessageFromGeneral(recvCmd);
	char mesgRecv[RECV_BUF_SIZE];
	if(strstr(mesgRecv,"attack")){
		strncpy(mesgRecv, recvCmd, 6);
	}else{
		strncpy(mesgRecv, recvCmd, 7);
	}

	pthread_mutex_lock(&mutex);
	decisions[decisionCount] = new char[30];
	strcpy(decisions[decisionCount++], mesgRecv);
	pthread_mutex_unlock(&mutex);					

	if(isMesgFromGen){
		for(int i=0;i<num_hosts;i++){
			if(strcmp(hostnames[i], localhostname)!=0 && i!=0){
				sendMessage(mesgRecv, hostnames[i], port);
			}
		}					
	}
	printf("Leaving recvMesgAndSendToAll\n");
}

void sendMessage(char* mesg, char* dest_hostname, int port){
	printf("Entering sendMessage\n");
	int true1 = 1;
	int sock;  
        char send_data[256],recv_data[256];
        struct hostent *host;
        struct sockaddr_in serv_address;  
        host = gethostbyname(dest_hostname);
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("Error in Socket");
            exit(1);
        }
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &true1, sizeof(int))==-1){
		perror("Error setting socket options");
		exit(1);
	} 
        serv_address.sin_family = AF_INET;     
        serv_address.sin_port = htons(port);   
        serv_address.sin_addr = *((struct in_addr *)host->h_addr);
        bzero(&(serv_address.sin_zero),8); 

        if (connect(sock, (struct sockaddr *)&serv_address, sizeof(struct sockaddr)) == -1) 
        {
            perror("Error in Connect");
            exit(1);
        }
	printf("Sending Message: %s\n", mesg);
	send(sock, mesg, strlen(mesg)+1, 0);
	cout<<"Num hosts = "<<num_hosts<<endl;
	printf("exiting sendMessage\n");
	return;

}

bool checkAndPopulateArgs(int args, char* argv[], bool* loyal, int* port, char* hostfile, char* command){
	printf("Entering checkargs\n");
	int i = 1;
	int argcount=0;
	args--;
	if(args !=5 && args!=6){
		printf("Invalid number of arguments\n");
		exit(1);
	}
	int maxargs = args;
	while(i<=args){
		if(strcmp(argv[i],"-loyal")==0){
			*loyal=true;
			argcount++;	
		}else if(strcmp(argv[i], "-traitor")==0){
			*loyal=false;
			argcount++;
		}
		if(strcmp(argv[i],"-port")==0){
			*port = atoi(argv[i+1]);
			if(*port == 0){
				printf("Specify port number after -port arg\n");
				exit(1);
			}
			argcount=argcount+2;
		}
		if(strcmp(argv[i],"-hostfile")==0){
			strcpy(hostfile, argv[i+1]);
			argcount = argcount + 2;
		}
		if(strcmp(argv[i],"attack")==0){
			strcpy(command,"attack");
			argcount++;
		}else if(strcmp(argv[i],"retreat")==0){
			strcpy(command, "retreat");
			argcount++;
		}
		i++;
	}
	if(argcount!=maxargs){
		printf("Error is argument list.\n");
		exit(1);
	}
	printf("Exiting checkargs\n");
	return true;
}
