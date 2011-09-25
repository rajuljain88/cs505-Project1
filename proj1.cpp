#include "globals.h"

int main(int args, char* argv[]) {

	char hostfile[50];
	char command[100];
	string line;
	struct mesgPackage mesg;

	pthread_mutex_init(&mutex, NULL);
	gethostname(localhostname, 30);
	checkAndPopulateArgs(args, argv, &loyal, &port, hostfile, command);

	if (strlen(command) != 0) {
		if (strcmp(command, "attack") == 0) {
			mesg.message = 1;
		} else if (strcmp(command, "retreat") == 0) {
			mesg.message = 0;
		}
	}
	ifstream fp(hostfile);
	if (fp.is_open()) {
		while (fp.good()) {
			getline(fp, line);
			if (line.empty())
				break;

			hostnames[++num_hosts] = new char[30];
			strncpy(hostnames[num_hosts], line.c_str(), strlen(line.c_str()));

			if (strncmp(line.c_str(), localhostname, strlen(localhostname))
					== 0) {
				localhostindex = num_hosts;
			}
		}
		fp.close();
	} else {
		perror("file not found: ");
		exit(1);
	}
	printf("localhostindex = %d\n", localhostindex);
	if (isGeneral(command)) {
		for (int i = 1; i < NPROC; i++) {
			mesg.pid[i] = 0;
		}
		char keyfile[100];
		strcpy(keyfile, "./keys/");
		strcat(keyfile, hostnames[1]);
		strcat(keyfile, ".key");
		for (int i = 1; i <= num_hosts; i++) {
			if (strcmp(hostnames[i], localhostname) != 0) {
				if(!loyal){
					mesg.message = !mesg.message;
				}
				printf("Signing message\n");
				printf("keyfile is %s\n", keyfile);
				signMessage(mesg.message, keyfile, mesg.signatures[1]);
				mesg.pid[1] = 1;
				sendMessage(mesg, hostnames[i], port);
			}
		}
	} else {
		waitAndProcessMessages(threads, port);
	}

	pthread_mutex_destroy(&mutex);
}

bool isGeneral(char* command) {
	return (strcmp(command, "attack") == 0 || strcmp(command, "retreat") == 0);
}

bool isMessageFromGeneral(char* mesg) {
	return !(strstr(mesg, hostnames[1]) == NULL);
}

void makeDecision() {
	if ((attacknum == 1 && retreatnum == 1)
			|| (attacknum == 0 && retreatnum == 0)
			|| (attacknum == 0 && retreatnum == 1)) {
		printf("RETREATING\n");
	} else if (attacknum == 1 && retreatnum == 0) {
		printf("ATTACKING\n");
	}
}

void waitAndProcessMessages(pthread_t* threads, int port) {

	fd_set master, readfds;
	int sock, connectSockFd;
	int true1 = 1;
	char* sendCmd;
	struct sockaddr_in serv_address, cli_address;
	int sin_size;
	int recv_len;
	struct timeval tv;
	FD_ZERO(&readfds);
	FD_ZERO(&master);
	tv.tv_sec = 20;
	tv.tv_usec = 0;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("Error creating socket");
		exit(1);
	}
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &true1, sizeof(int)) == -1) {
		perror("Error setting socket options");
		exit(1);
	}
	FD_SET(sock, &master);
	serv_address.sin_family = AF_INET;
	serv_address.sin_port = htons(port);
	serv_address.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_address.sin_zero), 8);

	if (bind(sock, (struct sockaddr *) &serv_address, sizeof(struct sockaddr))
			== -1) {
		perror("Error binding to the socket");
		exit(1);
	}
	if (listen(sock, 100) == -1) {
		perror("Error listening to the socket");
		exit(1);
	}
	int i = 1;
	while (true) {

		readfds = master;
		int val = select(sock + 1, &readfds, NULL, NULL, &tv);
		if (val == -1) {
			perror("Error Occured in Select");
			exit(1);
		} else if (val == 0) {
			break;
		} else {
			if (FD_ISSET(sock, &readfds)) {
				connectSockets[i] = accept(sock,
						(struct sockaddr *) &cli_address,
						(socklen_t *) &sin_size);
				if (connectSockets[i] == -1) {
					perror("Error in accept");
					break;
				}
				pthread_create(&threads[i], NULL, recvMesgAndSendToAll,
						(void *) &connectSockets[i]);
				i++;
			}
		}
	}
	for (int j = 1; j < i; j++) {
		pthread_join(threads[j], NULL);
	}
	if (!loyal) {
		printf("Traitor random decision is %d\n", traitordecision);
		printf("I am a traitor. My decision doesnt matter!!!\n");
	} else {
		makeDecision();
	}
}
void *recvMesgAndSendToAll(void* connectSockFd) {
	printf("Entering recvMesgAndSendToAll\n");
	bool isMesgFromGen = false;
	struct mesgPackage recvMesg;

	char *start = (char *) &recvMesg;
	int recvbytes = sizeof(struct mesgPackage);
	int recvd = 0;
	while (recvd != sizeof recvMesg) {
		int temp = recv(*(int *) connectSockFd, start,
				sizeof(struct mesgPackage), 0);
		start += temp;
		recvbytes -= temp;
		recvd += temp;
	}

	printf("Message recieved is %d, received from: ", recvMesg.message);
	for (int i = 1; i < NPROC; i++) {
		if (recvMesg.pid[i] == 1) {
			printf("%d, ", i);
		}
	}
	printf("\n");
	pthread_mutex_lock(&mutex);
	int ignoreverify = 0;
	if (!loyal) {
		traitordecision = rand() % 3;
		traitordecision = 1;
		if (traitordecision == 2) {
			printf("Ignoring the message\n");
			return NULL;
		} else if (traitordecision == 1) {
			printf("Forging message\n");

			recvMesg.message = !recvMesg.message;
			ignoreverify = 1;
		}
	}

	if (!ignoreverify) {
		if (!verifySignatures(recvMesg)) {
			pthread_mutex_unlock(&mutex);
			return NULL;
		}
	}

	if (recvMesg.message == 1) {
		if (attacknum == 1) {
			pthread_mutex_unlock(&mutex);
			return NULL;
		} else {
			attacknum = 1;
		}
	} else if (recvMesg.message == 0) {
		if (retreatnum == 1) {
			pthread_mutex_unlock(&mutex);
			return NULL;
		} else {
			retreatnum = 1;
		}
	}
	pthread_mutex_unlock(&mutex);

	recvMesg.pid[localhostindex] = 1;

	char keyfile[50];
	strcpy(keyfile, "./keys/");
	strcat(keyfile, localhostname);
	strcat(keyfile, ".key");
	printf("Signing message for hostindex = %d using keyfile=%s\n",
			localhostindex, keyfile);
	signMessage(recvMesg.message, keyfile, recvMesg.signatures[localhostindex]);

	for (int i = 1; i <= num_hosts; i++) {
		//Check for signature, if signature is wrong discard the message and exit the loop
		//If signature is right, then send it to everybody except yourself and general
		if (strcmp(hostnames[i], localhostname) != 0 && i != 1
				&& recvMesg.pid[i] == 0) {
			sendMessage(recvMesg, hostnames[i], port);
		}
	}
	printf("Leaving recvMesgAndSendToAll\n");
	return NULL;
}

void sendMessage(struct mesgPackage mesg, char* dest_hostname, int port) {
	printf("Entering sendMessage\n");
	printf("destination host name is %s\n", dest_hostname);
	int true1 = 1;
	int sock;
	struct hostent *host;
	struct sockaddr_in serv_address;
	host = gethostbyname(dest_hostname);
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Error in Socket");
		exit(1);
	}
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &true1, sizeof(int)) == -1) {
		perror("Error setting socket options");
		exit(1);
	}
	serv_address.sin_family = AF_INET;
	serv_address.sin_port = htons(port);
	serv_address.sin_addr = *((struct in_addr *) host->h_addr);
	bzero(&(serv_address.sin_zero), 8);

	if (connect(sock, (struct sockaddr *) &serv_address,
			sizeof(struct sockaddr)) == -1) {
		perror("Error in Connect");
		exit(1);
	}

	printf("Sending message to %s\n", dest_hostname);

	if (-1
			== send(sock, (struct mesgPackage*) &mesg,
					sizeof(struct mesgPackage), 0)) {
		perror("error in send:");
	}

	printf("exiting sendMessage\n");
	return;

}

bool checkAndPopulateArgs(int args, char* argv[], bool* loyal, int* port,
		char* hostfile, char* command) {
	printf("Entering checkargs\n");
	int i = 1;
	int argcount = 0;
	args--;
	if (args != 5 && args != 6) {
		printf("Invalid number of arguments\n");
		exit(1);
	}
	int maxargs = args;
	while (i <= args) {
		if (strcmp(argv[i], "-loyal") == 0) {
			*loyal = true;
			argcount++;
		} else if (strcmp(argv[i], "-traitor") == 0) {
			*loyal = false;
			argcount++;
		}
		if (strcmp(argv[i], "-port") == 0) {
			*port = atoi(argv[i + 1]);
			if (*port == 0) {
				printf("Specify port number after -port arg\n");
				exit(1);
			}
			argcount = argcount + 2;
		}
		if (strcmp(argv[i], "-hostfile") == 0) {
			strcpy(hostfile, argv[i + 1]);
			argcount = argcount + 2;
		}
		if (strcmp(argv[i], "attack") == 0) {
			strcpy(command, "attack");
			argcount++;
		} else if (strcmp(argv[i], "retreat") == 0) {
			strcpy(command, "retreat");
			argcount++;
		}
		i++;
	}
	if (argcount != maxargs) {
		printf("Error is argument list.\n");
		exit(1);
	}
	printf("Exiting checkargs\n");
	return true;
}

void signMessage(int message, char* keyfile, unsigned char* signature) {
	int err;
	unsigned int sig_len;
	static char data[100];
	EVP_MD_CTX md_ctx;
	EVP_PKEY* pkey;
	FILE * fp;
	X509 * x509;
	memset(&data, '\0', sizeof(data));
	sprintf(data, "%d", message);
	ERR_load_crypto_strings();
	/* Read private key */
	fp = fopen(keyfile, "r");
	if (fp == NULL) {
		perror("File error");
		printf("Key file not found\n");
		exit(1);
	}
	pkey = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
	fclose(fp);

	if (pkey == NULL) {
		printf("Error in claiming private key\n");
		ERR_print_errors_fp (stderr);
		exit(1);
	}

	/* Do the signature */

	EVP_SignInit(&md_ctx, EVP_sha1());
	EVP_SignUpdate(&md_ctx, data, strlen(data));
	sig_len = sizeof(signature);
	err = EVP_SignFinal(&md_ctx, signature, &sig_len, pkey);
	if (err != 1) {
		printf("Error in signing\n");
		ERR_print_errors_fp(stderr);
		exit(1);
	}

	EVP_PKEY_free(pkey);

}

bool verifySignature(int message, unsigned char* signature, char* certfile) {
	int errorNum;
	unsigned int signature_length;
	//unsigned char sig_buf[4096];
	static char data[100];
	EVP_MD_CTX md_ctx;
	EVP_PKEY* publickey;
	FILE * fileptr;
	X509 * x509;

	memset(&data, '\0', sizeof(data));
	sprintf(data, "%d", message);

	ERR_load_crypto_strings();

	fileptr = fopen(certfile, "r");
	if (fileptr == NULL) {
		printf("certfile not found\n");
		exit(1);
	}
	x509 = PEM_read_X509(fileptr, NULL, NULL, NULL);
	fclose(fileptr);

	if (x509 == NULL) {
		printf("Error in certificate file x509\n");
		ERR_print_errors_fp (stderr);
		exit(1);
	}

	publickey = X509_get_pubkey(x509);
	if (publickey == NULL) {
		printf("Error in claiming the public key\n");
		ERR_print_errors_fp (stderr);
		exit(1);
	}
	signature_length = 128;

	EVP_VerifyInit(&md_ctx, EVP_sha1());
	EVP_VerifyUpdate(&md_ctx, data, strlen((char*) data));
	errorNum = EVP_VerifyFinal(&md_ctx, signature, signature_length, publickey);
	EVP_PKEY_free(publickey);

	if (errorNum != 1) {
		ERR_print_errors_fp (stderr);
		return 0;
	}
	return 1;
}

bool verifySignatures(struct mesgPackage mesg) {
	char certfile[50];
	bool verify;
	for (int i = 1; i <= num_hosts; i++) {
		if (mesg.pid[i] == 1) {
			printf("verifying message for pid = %d\n", i);
			strcpy(certfile, "./keys/");
			strcat(certfile, hostnames[i]);
			strcat(certfile, ".crt");
			printf("certfile is %s\n", certfile);

			verify = verifySignature(mesg.message, mesg.signatures[i],
					certfile);

			if (!verify) {
				printf("Verification Failed for pid=%d\n", i);
				return verify;
			} else {
				printf("Signature Verified OK! for pid=%d\n", i);
			}
		}
	}
	return verify;
}
