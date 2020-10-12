// Ryan Wigglesworth - rwiggles
// Kelly Buchanan - kbuchana
// Connor Ruff - cruff
#include "../pg3lib.h"
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
	std::string PW;
};

char *  parseArgs(int, char **);
int     getSock(std::string);
void *  connection_handler(void *cliSockIn);
void *  getCliMsg(int cliSock, int recSize);
size_t  sendToCli(void *, int, int);
int     storeUserInfo(struct userInfo);
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

		if ( pthread_create( &thread_id, NULL, connection_handler, (void *)&cliSock) < 0) {
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

	std::cout << "Received " << received << " bytes from client" << std::endl;

	buf[received] = '\0';
	void * ret = buf;
	return ret;
}

struct userInfo checkUserbase(char * userNameIn){

	struct userInfo user;
	user.UN = "";
	user.PW = "";

	std::string userName(userNameIn);
	// Search Through File
	std::ifstream ifs;
	ifs.open("users.csv");

	if (!ifs){
		std::cout << "Error Opening Userbase file" << std::endl;
		std::exit(-1);
	}

	std::string line;
	std::string delim = ",";
	std::string token;
	while(getline(ifs, line)) {
		token = line.substr(0, line.find(delim));
		if ( token.compare(userName) == 0 ){
			// set user info
			user.UN = userName ;
			user.PW = line.substr(line.find(delim)+1);
			break;
		}
	}

    ifs.close();
	
	return user;

	
}

int storeUserInfo(struct userInfo user) {
   
	std::ofstream ofs("users.csv", std::ios_base::app);
	if (!ofs){
		std::cout << "Error Opening Userbase file" << std::endl;
		return -1;
	}

	ofs << user.UN  << "," << user.PW << std::endl;

	ofs.close();
		 
    return 0;
}

size_t sendToCli(void * toSend, int len, int cliFD){
    size_t sent = send(cliFD, toSend, len, 0);
	if ( sent == -1 ) {
		std::cerr << "Error Sending Info To Client: " << strerror(errno) << std::endl;
		std::exit(-1);
	} 
	
	std::cout << "Sent " << sent << " bytes to client" << std::endl;

        return sent;
                
}

void * connection_handler(void * cliSockIn){

	int cliSock = *(int*)cliSockIn ;
	
	// Receive Size of UserName From Client
	short int usrNameSize;
	usrNameSize = * ((short int *) getCliMsg(cliSock, sizeof(short int)));
	// Receive UserName
	char *userName;
	userName = (char *) getCliMsg(cliSock, usrNameSize + 1);
	std::string userNameString(userName);
    std::cout <<  "Received UserName: " << userNameString << std::endl;
	
	// Create and Send Public Key
	char * cliPubKey = getPubKey();
	std::cout << "Client's Public Key: " << cliPubKey << std::endl;
	// Send Size of Public Key
	int keySizeToSend = strlen(cliPubKey)+1;
	sendToCli( (void *)&keySizeToSend, sizeof(int), cliSock);
	sendToCli( (void *)cliPubKey, keySizeToSend, cliSock);


	// Check If Exists
	struct userInfo usr = checkUserbase(userName);
//	std::cout << "User From File: " << usr.UN << ", " << usr.PW << std::endl;
	// User Does Not Exist Yet
	int confirm;
	if ( usr.UN.compare("") == 0) {
		std::cout << "User DNE\n";
		usr.UN = userNameString ;
		confirm = 1;
        sendToCli((void *)&confirm, sizeof(int), cliSock);   
                
	}
	// User Exists
	else {
		std::cout << "User Exists\n";
		confirm = 0;
        sendToCli((void *)&confirm, sizeof(int), cliSock);
	}

	
        
    // Receive Size of Password Hash From Client
	short int passSize;
	passSize = * ((short int *) getCliMsg(cliSock, sizeof(short int)));
	// Receive Password Hash
	char *passHash;
	passHash = (char *) getCliMsg(cliSock, passSize + 1);

        
    //TODO decrypt password
	char * decrPass = decrypt(passHash); 
    std::cout << "Decrypted Password: " << decrPass << std::endl;
	 
    // Check if Password Matches for existing user
    if(confirm == 0) {
        if( !strcmp(decrPass, usr.PW.c_str()) ) {
            sendToCli((void *)&confirm, sizeof(int), cliSock);
        } else {
            confirm = 1;
            sendToCli((void *)&confirm, sizeof(int), cliSock);  
        }
    } 
    // Set Password for new user 
    else {
		std::string temp(decrPass);
        usr.PW = temp;
        if (storeUserInfo(usr) == -1) {
            std::cerr << "Could not create new user: " << strerror(errno) << std::endl;   
        }
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
