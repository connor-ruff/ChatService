// Ryan Wigglesworth - rwiggles
// Kelly Buchanan - kbuchana
// Connor Ruff - cruff
#include<iostream>
#include<string>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/time.h>
#include<netinet/in.h>
#include<netdb.h>
#include<cstdlib>
#include<cerrno>
#include<unistd.h>
#include<fstream>
#include<sstream>
#include<pthread.h>
#include<stdbool.h>
#include<queue>
#include<cstdint>
#include"../pg3lib.h"

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;


std::queue <char *> client_messages;

void help() {
	printf("Please enter a command (BM: Broadcast Messaging, PM: Private Messaging, EX: Exit\n");
}

void usage() {
	printf("./client [<host> <port> <username>]\n");
}

void *inbound_messager(void *arg){
	int servFD = *(int*)arg;
	char buf[BUFSIZ];
	char bufMiddle[BUFSIZ];
	char bufFinal[BUFSIZ-1];
	while(true){
		/* note: this is going to need to handle the padding properly */
		bzero(bufMiddle, sizeof(bufMiddle));
		int total = 0;
		while(total < BUFSIZ){
			total+= recv(servFD, buf, BUFSIZ, 0);
			strcat(bufMiddle, buf);
			bzero(buf, sizeof(buf));
		}
		std::cout << bufMiddle << std::endl;
		char flag = bufMiddle[0];
		
		bzero(bufFinal, sizeof(bufFinal));
		bufFinal[BUFSIZ-1];
		for (int i=1; i <= BUFSIZ; ++i)
			bufFinal[i-1] = bufMiddle[i];
		std::cout << "Final Buffer: " << bufFinal <<std::endl;
		if (flag == '1'){
			std::cout << "message added to queue: "<< bufFinal << std::endl;
			char * dummy = bufFinal;
			pthread_mutex_lock(&lock);
			client_messages.push(dummy);
			pthread_cond_signal(&cond);
			pthread_mutex_unlock(&lock);
		}
		else if (flag == '2'){
			std::cout << "******* message recieved " << bufFinal << std::endl;
		}
		else{
			std::cout << "Buf: " << bufMiddle << " with flag " << flag <<std::endl;
			std::cout << "an error ocurred reading in stuff\n";
				
		}
		
	}
	return 0;
}

char *get_server_message(){
	//if sizeMatters then we need to pop off twice(once for length of next thing once for the next thing)
	std::cout << "Getting server message\n";
	pthread_mutex_lock(&lock);
	while (client_messages.empty()){
		pthread_cond_wait(&cond, &lock);
	}

	//client_messages.front();
	//char buffer[BUFSIZ] = client_messages.pop();
	char * resp = client_messages.front();
	client_messages.pop();
	pthread_mutex_unlock(&lock);
	return resp;
	

}

int create_socket(char* host, char* portstr){
	struct hostent *hp; // host info
	struct sockaddr_in servaddr; // server address
	socklen_t addrlen = sizeof(servaddr);
	int port = atoi(portstr);

	int fd;
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		std::cerr << "Could not create socket\n";
	}

	memset((char*)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);

	// Get host name
	hp = gethostbyname(host);
	if(!hp){
		fprintf(stderr, "could not obtain address of %s\n", host);
	}
	// put host address into server address structure
	memcpy((void *)&servaddr.sin_addr, hp->h_addr_list[0], hp->h_length);

	// connect
	if (connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
		printf("\nConnection Failed \n");
		return -1;
	}

	return fd;
}



size_t sendToServ(int sockFD, void * toSend, int size){

//	std::cout << "in send function" << std::endl;
	int bytesSent;

	bytesSent = send(sockFD, toSend, size, 0) ;
	if (bytesSent == -1) {
		std::cerr << "Error Sending To Server: " << strerror(errno) << std::endl;
	}

//	std::cout << "Sent " << bytesSent << " bytes " << std::endl;
	return bytesSent;

}

void handle_user_connection(int servFD, char* username){
	// send length 
	short int userlen = strlen(username) + 1;
	sendToServ(servFD, (void*)&userlen, sizeof(userlen));
	// send username
	sendToServ(servFD, username, strlen(username)+1);
	
	// Receive Public Key Size and Public Key
	char pubKey[4096];
	strcpy(pubKey, get_server_message());
	std::cout << "pubkey: " << pubKey << std::endl;
	// determine if we need to create new password
	char *resp = get_server_message();
	std::cout << "Response: " << resp << std::endl;

	std::string password;
	if (strcmp("CON", resp) == 0){
		// username found
		bool accepted = false;
		while(accepted == false) {
			std::cout << "Existing user\n";
			std::cout << "Enter password: ";
			std::cin >> password;

						
			// Ecrypt Password
			char * encrPass = encrypt((char *)password.c_str(), pubKey);

			// send length (including null character)
			short int passlen = strlen(encrPass)+1;
		//	std::cout << "Password length to send: " << passlen << std::endl;
			sendToServ(servFD, (void *)&passlen, sizeof(short int));
			// send password
		//	std::cout << "Your Password: " << c_pass << std::endl;
			sendToServ(servFD, (void *)encrPass, passlen);
			// recieve confirmation if password was accepted
			resp = get_server_message();
			if (strcmp("CON", resp) == 0)
				accepted = true;
		}
	}
	else{
		// create new password
		std::cout << "Creating new user\n";
		std::cout << "Enter password: ";
		std::cin >> password;
		// encrypt password
		char * encrPass = encrypt((char *)password.c_str(), pubKey);
		// send length (including null character)
		short int passlen = strlen(encrPass)+1;
		// send length to server
		std::cout << "encr password: " << encrPass << " passslen: " << passlen << std::endl;
		sendToServ(servFD, (void *)&passlen, sizeof(short int));
		// send password
		std::cout << "length sent\n";
		sendToServ(servFD, (void *)encrPass, passlen);
		std::cout << "password sent\n";
	}
}


int main(int argc, char* argv[]){
	/* Get Input */
	char *host;
	char *port;
	char *username;
	if (argc < 4){ 
		usage();
		return EXIT_FAILURE;
	} else {
		host = argv[1];
		port = argv[2];
		username = argv[3];
	}
	/* create socket connection */
	int servFD = create_socket(host, port);
	if (servFD == -1){
		return EXIT_FAILURE;
	}

	/* Create thread */
	pthread_t inbound;
	if ((pthread_create(&inbound, NULL, inbound_messager, (void *)&servFD) < 0) < 0){
		std::cerr << "Could not create thread" << std::endl;
		return EXIT_FAILURE;
	}	
	std::cout << " Thread created" << std::endl;	
	/* determine if new user or not and handle that */
	handle_user_connection(servFD, username);
	std::cout << "user created successfully\n";



	
}




