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

struct userInfo {
	std::string UN;
	std::string passHash;
}

char * parseArgs(int, char **);
int getSock(std::string);
void * connection_handler(void *cliSockIn);
void * getCliMsg(int cliSock, int recSize);
struct userInfo checkUserbase(char * userNameIn);

int main(int argc, char ** argv){

	// Get CL Args
	std::string port;
	port = parseArgs(argc, argv);
	// Establish Socket To Listen To Connections
	int sockfd = getSock(port);

	std::cout << "Waiting For Connection on " << port << "...\n";

	// Accept Client Connections
	pthread_t thread_id;
	struct sockaddr_in client;
	int c = sizeof(struct sockaddr_in);
	int cliSock;
	while( (cliSock = accept(sockfd, (struct sockaddr *)&client, (socklen_t *)&c)) ) {

		std::cout << "Accepted Connection\n" ; // TODO debug info

		if ( pthread_create( &thread_id, NULL, connection_handler, (void *)&client_sock) < 0) {
			std::cerr << "Could Not Create Thread" << std::endl;
			continue;
		}

		pthread_join(thread_id, NULL);

		if (cliSock < 0){
			std::cerr << "Accept Failed\n";
			std::exit(-1);
		}

	}

	return 0;
}

void * getCliMsg(int cliSock, int recSiz){

	char buf[BUFSIZ];
	int received;
	bzero(&buf, sizeof(buf));
	
	if( (received = recv(cliSock, buf, recSiz, 0)) == -1){
		std::cerr << "Server Failed on recv(): " << std::endl;
		std::exit(-1);
	}

	buf[received] = '\0';
	void * ret = buf;
	return ret;
}

std::string checkUserbase(char * userNameIn){

	struct userInfo user;
	user.UN = "";
	user.passHash = "";

	std::string userName(userNameIn);
	// Search Through File
	std::ifstream ifs;
	ifs.open("userBase.csv");

	if (!ifs){
		std::cout << "Error Opening Userbase file" << std::endl;
		std::exit(-1);
	}

	std::string line;
	std::string delim = ",";
	std::string token;
	while(getline(ifs, line) {
		token = line.substr(0, line.find(delim));
		if ( token.compare(userName) == 0 ){
			// set user info
			user.UN = token;
			user.passHash = line.substr(line.find(delim)+1);
			break;
		}
	}
	
	return user;

	
}

size_t sendToCli(
void * connection_handler(void * cliSockIn){

	int cliSock = *(int*)cliSockIn ;
	
	// Receive Size of UserName From Client
	int usrNameSize;
	usrNameSize = * ((int *) getCliMsg(cliSock, sizeof(int)));
	// Receive UserName
	char userName[usrNameSize+1];
	userName = (char *) getCliMsg(cliSock, usrNameSize) ;
	// Check If Exists
	struct userInfo usr = checkUserbase(userName);
	// User Does Not Exist Yet
	int confirm;
	if ( usr.UN.compare("") == 0) {
		confirm = 1;

	}
	// User Exists
	else{
		confirm = 0;
	}


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
	if ( (sockfd = socket(results->ai_family, results->ai_socktype, 0)) < 0){
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
		close(sockfd);
		std::exit(-1);
	}

	return sockfd;
}





char * parseArgs(int argc, char ** argv){
	if (argc != 2){
		std::cerr << "Wrong Number of Arguments\n";
		std::exit(-1);
	}
	return argv[1];
}
