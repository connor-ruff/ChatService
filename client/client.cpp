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
	/* the prompt */
	printf("Please enter a command (BM: Broadcast Messaging, PM: Private Messaging, EX: Exit\n");
}

void usage() {
	/* usage function */
	printf("./client [<host> <port> <username>]\n");
}

void *inbound_messager(void *arg){
	/* This is the command that recieves all inbound messages from the server
	 * and routes accordingly. If it is meant for the other thread, it will
	 * put the message on a stack in a threadsafe manner */
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
		char flag = bufMiddle[0];
		
		bzero(bufFinal, sizeof(bufFinal));
		bufFinal[BUFSIZ-1];
		for (int i=1; i <= BUFSIZ; ++i)
			bufFinal[i-1] = bufMiddle[i];
		if (flag == '1'){
			char * dummy = bufFinal;
			pthread_mutex_lock(&lock);
			client_messages.push(dummy);
			pthread_cond_signal(&cond);
			pthread_mutex_unlock(&lock);
		}
		else if (flag == '2'){// unencrypted BM
			std::cout << "\n******* incoming public message ********* " << bufFinal << "\n>";
			help();
			std::cout << ">";
			fflush(stdout);
		}	else if (flag == '3'){// encrypted PM
			char * result = decrypt(bufFinal);
			std::cout << "\n******* incoming private message ******** " << result << "\n>";
			help();
			std::cout << ">";
			fflush(stdout);
		}
		else{
			std::cout << "Buf: " << bufMiddle << " with flag " << flag <<std::endl;
			std::cout << "an error ocurred reading in stuff\n";
				
		}
		
	}
	return 0;
}

char *get_server_message(){
	/* abstraction to get a message from the server. It pops off the queue and will yield
	 * the lock when the queue is empty and wait */
	pthread_mutex_lock(&lock);
	while (client_messages.empty()){
		pthread_cond_wait(&cond, &lock);
	}

	char * resp = client_messages.front();
	client_messages.pop();
	pthread_mutex_unlock(&lock);
	return resp;
	

}

int create_socket(char* host, char* portstr){
	/* returns a socket connection */
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
	/* abstraction to send data to the server */

	int bytesSent;

	bytesSent = send(sockFD, toSend, size, 0) ;
	if (bytesSent == -1) {
		std::cerr << "Error Sending To Server: " << strerror(errno) << std::endl;
	}

	return bytesSent;

}

void handle_user_connection(int servFD, char* username){
	/* establish the users connection with server as identified user */
	// send length 
	short int userlen = strlen(username) + 1;
	sendToServ(servFD, (void*)&userlen, sizeof(userlen));
	// send username
	sendToServ(servFD, username, strlen(username)+1);
	
	// Receive Public Key Size and Public Key
	char pubKey[4096];
	strcpy(pubKey, get_server_message());

	// determine if we need to create new password
	char *resp = get_server_message();

	std::string password;
	if (strcmp("CON", resp) == 0){
		// username found
		bool accepted = false;
		std::cout << "Existing user\n";
		while(accepted == false) {
			std::cout << "Enter password: ";
			std::cin >> password;

						
			// Ecrypt Password
			char * encrPass = encrypt((char *)password.c_str(), pubKey);

			// send length (including null character)
			short int passlen = strlen(encrPass)+1;

			sendToServ(servFD, (void *)&passlen, sizeof(short int));
			// send password

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
		sendToServ(servFD, (void *)&passlen, sizeof(short int));
		// send password
		sendToServ(servFD, (void *)encrPass, passlen);
	}

	// create and send pubkey
	char * servPubKey = getPubKey();
	// send size of pubkey
	int keySize = strlen(servPubKey)+1;
	sendToServ(servFD, (void *)&keySize, sizeof(int));
	sendToServ(servFD, (void *)servPubKey, keySize);
}

void handle_bm(int servFD){
	/* handle BM. Basically sends broadcast message to every online user */

	short int signal = 1;
	sendToServ(servFD, (void *)&signal, sizeof(short int));
	// recieve acknowledgement
	char *ack = get_server_message();
	std::cout << "Enter the public message: ";
	std::string message;
	std::cin.ignore();
	getline(std::cin, message);

	// send the message
	short int messageSize = strlen(message.c_str())+1;
	sendToServ(servFD, (void *)&messageSize, sizeof(short int));
	sendToServ(servFD, (void *)message.c_str(), messageSize);

	// recieve confirmation
	char * confirmation = get_server_message();
}

void handle_pm(int servFD){
	/* handle PM command. Send a private message */
	short int signal = 2;
	sendToServ(servFD, (void *)&signal, sizeof(short int));
	char * users = get_server_message();
	std::cout << "Peers Online:\n" << users;
	std::cout << "Peer to message: ";
	std::string input;
	std::cin >> input;

	// send name to server
	short int size = strlen(input.c_str()) + 1;
	sendToServ(servFD, (void *)&size, sizeof(short int));
	sendToServ(servFD, (void *)input.c_str(), size);

	// recive pubKey or denial
	char * pubKey = get_server_message();
	while (strcmp(pubKey, "DEN") == 0){
		std::cout << "Invalid user. Please enter again: ";
		std::cin >> input;

		size = strlen(input.c_str()) + 1;
		sendToServ(servFD, (void *)&size, sizeof(short int));
		sendToServ(servFD, (void *)input.c_str(), size);
		pubKey = get_server_message();
	}

	// get and send the private message
	std::cout << "Enter the private message: ";
	std::string rawMessage;
	std::cin.ignore();
	getline(std::cin, rawMessage);
	char buf[4096];
	strcpy(buf, rawMessage.c_str());

	char * message = encrypt(buf, pubKey);
	size = strlen(message)+1;
	sendToServ(servFD, (void *)&size, sizeof(short int));
	sendToServ(servFD, (void *)message, size);

	// get the confirmation
	
	char * conf = get_server_message();
	if (strcmp(conf, "CON")==0){
		std::cout << "Private message sent\n";
	}
	else if (strcmp(conf, "DEN") == 0){
		std::cout << "The user is not available\n";
	}
	else
		std::cout << "Something went wrong. conf is: " << conf << std::endl;

}


void handle_ex(int servFD){
	/* handle the EX command. Basically just quit */
	short int signal = 3;
	sendToServ(servFD, (void *)&signal, sizeof(short int));
	close(servFD);
	std::cout << "Bye!\n";
	exit(EXIT_SUCCESS);

}


void get_user_instructions(int servFD){
	/* Endless loop to get user instructions and call handlers */
	std::string input;
	while (true){
		std::cout << ">";
		help();
		std::cout << ">";
		std::cin >> input;
		if (input.compare("BM") == 0){
			handle_bm(servFD);
		} else if (input.compare("PM") == 0) {
			handle_pm(servFD);
		} else if (input.compare("EX") == 0) {
			handle_ex(servFD);
		} else {
			std::cout << "The function you mentioned does not exist\n";
		}
	}

}


int main(int argc, char* argv[]){
	/* main driver function */
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
	
	/* determine if new user or not and handle that */
	handle_user_connection(servFD, username);	// this establishes socket connections

	get_user_instructions(servFD);	// This has the logic for getting and routing instructions



	
}




