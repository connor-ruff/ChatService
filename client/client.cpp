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
#include<stdbool.h>

/*typedef struct MessageQueue MessageQueue;
struct MessageQueue {
	char	name[BUFSIZ];
	char 	host[BUFSIZ];
	char	port[BUFSIZ];

	Queue* outgoing;
	Queue* incoming;
	bool shutdown;

	Thread pushThread;
	Thread pullThread;

};*/

void help() {
	printf("Please enter a command (BM: Broadcast Messaging, PM: Private Messaging, EX: Exit\n");
}

void usage() {
	printf("./client [<host> <port> <username>]\n");
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
	// determine if we need to create new password
	int resp;
	recv(servFD, (void *)&resp, sizeof(int), 0);
	std::cout << "Response: " << resp << std::endl;

	std::string password;
	if (resp == 0){
		// username found
		bool accepted = false;
		while(accepted == false) {
			std::cout << "Existing user\n";
			std::cout << "Enter password: ";
			std::cin >> password;
			// send length (including null character)
			short int passlen = password.length()+1;
		//	std::cout << "Password length to send: " << passlen << std::endl;
			sendToServ(servFD, (void *)&passlen, sizeof(short int));
			// send password
			const char * c_pass = password.c_str();
		//	std::cout << "Your Password: " << c_pass << std::endl;
			sendToServ(servFD, (void *)c_pass, passlen);
			// recieve confirmation if password was accepted
			recv(servFD, (void *)&resp, sizeof(int), 0);
			if (resp == 0)
				accepted = true;
		}
	}
	else{
		// create new password
		std::cout << "Creating new user\n";
		std::cout << "Enter password: ";
		std::cin >> password;
		// send length
		short int passlen = password.length()+1;
		sendToServ(servFD, (void *)&passlen, sizeof(short int));
		// send password
		const char * c_pass = password.c_str();
		sendToServ(servFD, (void *)c_pass, passlen);
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
	/* determine if new user or not and handle that */
	handle_user_connection(servFD, username);

	/* Create thread */
	pthread_t inbound;

	
}




