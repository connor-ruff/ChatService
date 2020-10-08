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

char * parseArgs(int, char **);
int getSock(std::string);

int main(int argc, char ** argv){

	// Get CL Args
	std::string port;
	port = parseArgs(argc, argv);
	// Establish Socket To Listen To Connections
	int sockfd = getSock(port);

	return 0;
}

int getSock(std::string port){

	int sockfd;
	struct sockaddr_in sin;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	struct addrinfo * results;

	// Load Host Address Info Into results
	int status;
	if ( (status = getaddrinfo(NULL, port.c_str(), &hints, &results)) != 0){
		std::cerr << "Server Failure on getaddrinfo(): " << gai_strerror(status) << std::endl;
		std::exit(-1);
	}
	// Get the Socket FD
	if ( (sockfd = socket(results->ai_family, results->ai_socketype, 0)) < 0){
		std::cerr << "Server Error on socket(): " << strerror(errno) << std::endl;
		std::exit(-1);
	}
	// Bind
	int bindRes;
	if( bindRes = bind(sockfd, results->ai_addr, results->ai_addrlen) == -1) {
		std::cerr << "Server Error on Bind(): " << strerror(errno) << std::endl;
		close(sockfd);
		std::exit(-1);
	}
	
	freeaddrinfo(results);

	// Listen
	if( listen(sockfd, 5) < 0){
		std::cerr << "Server Failure on listen(): " << std::endl;
		std::exit(-1);
	}

	return sockfd;
}



}

char * parseArgs(int argc, char ** argv){
	if (argc != 2){
		std::cerr << "Wrong Number of Arguments\n";
		std::exit(-1);
	}
	return argv[1];
}
