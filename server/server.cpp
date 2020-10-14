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
#include<unordered_map>


const char * confirm = "CON";
const char * deny = "DEN";

struct userInfo {
	std::string UN;
	std::string PW;
};

std::unordered_map<std::string, std::string> onlineUserKeys;
std::unordered_map<std::string, int> onlineUserSockets;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

char *  parseArgs(int, char **);
int     getSock(std::string);
void *  connection_handler(void *cliSockIn);
void *  getCliMsg(int, int);
size_t  sendToCli(void *, int, int, int);
int     storeUserInfo(struct userInfo);
struct userInfo checkUserbase(std::string);
void mainBoard(char *, int, struct userInfo);
void handleBroadcast(int, struct userInfo);
void handlePrivate(int, struct userInfo);

int main(int argc, char ** argv){

	// Get CL Args
	std::string port;
	port = parseArgs(argc, argv);
	// Establish Socket To Listen To Connections
	int sockfd = getSock(port);

	std::cout << "Waiting For Connection on " << port << "...\n";

	// Accept Client Connections
	pthread_t thread_id[100];
	struct sockaddr_in client;
	int c = sizeof(struct sockaddr_in);
	int cliSock;
	int i = 0;
	while( (cliSock = accept(sockfd, (struct sockaddr *)&client, (socklen_t *)&c)) ) {

	//	std::cout << "Accepted Connection\n" ; // TODO debug info

		// Create New Thread
		if ( pthread_create( &thread_id[i], NULL, connection_handler, (void *)&cliSock) < 0) {
			std::cerr << "Could Not Create Thread" << std::endl;
			continue;
		}
		
		if (cliSock < 0){
			std::cerr << "Accept Failed\n";
			std::exit(-1);
		}
		i++;
	}

	

	return 0;
}

void mainBoard(char * cliPubKey, int cliSock, struct userInfo usr){

	// Receive Operation From Client
	bool exit = false;
	while(!exit){
		// Get The Command
		short int command = * ( (short int *) getCliMsg(cliSock, sizeof(short int)));

		// Handle Command
		switch (command) {

			case 1:            // BroadCast
				handleBroadcast(cliSock, usr);
				break;
			case 2:            // PM
				handlePrivate(cliSock, usr);
				break;
			case 3:
				exit = true;
				if ( pthread_mutex_lock(&lock) != 0 ) {
					std::cerr << "Mutex Error on Lock()\n";
					std::exit(-1); // TODO close stuff out
				} 
				// Clear Data Structures For That User
				onlineUserKeys.erase(usr.UN);
				onlineUserSockets.erase(usr.UN);
				if ( pthread_mutex_unlock(&lock) != 0 ) {
					std::cerr << "Mutex Error on unLock()\n";
					std::exit(-1); // TODO close stuff out
				}
				close(cliSock); 
				break;
		}
	}

}

void handlePrivate(int cliSock, struct userInfo usr){

	// Get List of All Users
	std::string userList = "";

	// Lock Data Struct
	if ( pthread_mutex_lock(&lock) != 0 ) {
		std::cerr << "Mutex Error on Lock()\n";
		std::exit(-1); // TODO close stuff out
	}
	
	// Get List of Users
	for( std::pair<std::string, int> elem : onlineUserSockets ){
		
		if ( elem.first.compare(usr.UN) == 0) {
			continue;
		}
		userList = userList  + elem.first + "\n";
	}

	// Unlock	
	if ( pthread_mutex_unlock(&lock) != 0 ) {
		std::cerr << "Mutex Error on unLock()\n";
		std::exit(-1); // TODO close stuff out
	} 

	// Send Full List To Client
	sendToCli( (void *)userList.c_str(), BUFSIZ, cliSock, 1) ;


	// Get PubKey For That User and Send
	bool foundUser = false;
	std::string receiverPubKey;
	short int userNameSize ;
	char * usrToSendTo ;
	std::string usrToSendStr; 
	int pubKeySize;
	while(!foundUser){
		// Get UserName To Send To
		userNameSize = * ((short int *) getCliMsg(cliSock, sizeof(short int))) ;
		usrToSendTo = (char *) getCliMsg(cliSock, userNameSize);
		usrToSendStr = usrToSendTo;
		receiverPubKey = "";
		receiverPubKey = receiverPubKey + onlineUserKeys[usrToSendStr] ;
		pubKeySize = receiverPubKey.length() + 1;
		if ( pubKeySize == 1){
			receiverPubKey = "DEN";
			sendToCli( (void *)receiverPubKey.c_str(), BUFSIZ, cliSock, 1);
			continue;
		}
		foundUser = true;
		sendToCli( (void *)receiverPubKey.c_str(), BUFSIZ, cliSock, 1);
	}

	// Receive Message To Be Sent
	short int msgSize = * ((short int *) getCliMsg(cliSock, sizeof(short int)));
	char * msgToSend = (char *) getCliMsg(cliSock, msgSize);
	// Lock Data Struct
	if ( pthread_mutex_lock(&lock) != 0 ) {
		std::cerr << "Mutex Error on Lock()\n";
		std::exit(-1); // TODO close stuff out
	}
	 
	// Check If User Exists
	bool valid = false;
	int receiverSocket;
	for( std::pair<std::string, int> elem : onlineUserSockets ){
		
		if ( elem.first.compare(usr.UN) == 0) {
			continue;
		}
		if (elem.first.compare(usrToSendStr) == 0){
			valid = true;
			receiverSocket = onlineUserSockets[usrToSendStr] ;
		}
	}
	// Unlock	
	if ( pthread_mutex_unlock(&lock) != 0 ) {
		std::cerr << "Mutex Error on unLock()\n";
		std::exit(-1); // TODO close stuff out
	} 
	
	
	// Send Message to User
	if (valid) {
		char toSend[BUFSIZ];
		memset(toSend, '\0', BUFSIZ);
		strcpy(toSend, msgToSend);
		sendToCli( (void *)toSend, BUFSIZ, receiverSocket, 3) ;
		// Notify Sender Client that it sent
		sendToCli( (void *)confirm, BUFSIZ, cliSock, 1);
	}
	
	else {
		// Tell Sender Client the user didn't exist
		sendToCli( (void *)deny, BUFSIZ, cliSock, 1);
	}
}

void handleBroadcast(int cliSock, struct userInfo usr){

	// Send Acknowledgement
	sendToCli(  (void *)confirm, BUFSIZ, cliSock, 1);

	// Receive Message To Send
	short int msgSize = * ((short int *) getCliMsg(cliSock, sizeof(short int))) ;
	char * broadMsg ;
	broadMsg = (char *) getCliMsg(cliSock, msgSize);

	// Lock Data Structure
	
	if ( pthread_mutex_lock(&lock) != 0 ) {
		std::cerr << "Mutex Error on Lock()\n";
		std::exit(-1); // TODO close stuff out
	} 
	// Send To All Other Clients
	char toSend[BUFSIZ];
	memset(toSend, '\0', BUFSIZ);
	strcpy(toSend, broadMsg);

	for( std::pair<std::string, int> elem : onlineUserSockets ){
		
		if ( elem.first.compare(usr.UN) == 0) {
			continue;
		}
		sendToCli( (void *) toSend, BUFSIZ, elem.second, 2) ;	
	}
	
	// Unlock Data Structure		
	if ( pthread_mutex_unlock(&lock) != 0 ) {
		std::cerr << "Mutex Error on unLock()\n";
		std::exit(-1); // TODO close stuff out
	} 

	// Acknowledge User
	sendToCli( (void *)confirm, BUFSIZ, cliSock, 1);
}

void * getCliMsg(int cliSock, int recSiz){

	char buf[BUFSIZ];
	int received;
	bzero(&buf, sizeof(buf));
	
	if( (received = recv(cliSock, buf, recSiz, 0)) == -1){
		std::cerr << "Server Failed on recv(): " << std::endl;
		std::exit(-1);
	}

//	std::cout << "Received " << received << " bytes from client" << std::endl;

	buf[received] = '\0';
	void * ret = buf;
	return ret;
}

struct userInfo checkUserbase(std::string userName){

	struct userInfo user;
	user.UN = "";
	user.PW = "";

	// Search Through File
	std::ifstream ifs;
	ifs.open("users.csv");

	if (!ifs){
		std::cout << "Error Opening Userbase file" << std::endl;
		std::exit(-1);
	}

	// Look for username
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

size_t sendToCli(void * toSend, int len, int cliFD, int flag){


	char buf[BUFSIZ];
	memset(buf, '\0', BUFSIZ);

	if (flag == 1)
		strcpy(buf, "1");
	else if (flag == 2)
		strcpy(buf, "2");
	else if (flag == 3)
		strcpy(buf, "3");
	else 
		strcpy(buf, "0");

	strcat(buf, (const char *) toSend);

    size_t sent = send(cliFD, (void *) buf, BUFSIZ, 0);
	if ( sent == -1 ) {
		std::cerr << "Error Sending Info To Client: " << strerror(errno) << std::endl;
		std::exit(-1);
	} 
	
//	std::cout << "Sent " << sent << " bytes to client" << std::endl;

    return sent;
                
}

void * connection_handler(void * cliSockIn){

	int cliSock = *(int*)cliSockIn ;
	
	// Receive Size of UserName From Client
	short int usrNameSize;
	usrNameSize = * ((short int *) getCliMsg(cliSock, sizeof(short int)));
	// Receive UserName
	char *userName;
	userName = (char *) getCliMsg(cliSock, usrNameSize);
	std::string userNameString(userName);
	
	// Create and Send Public Key
	char * cliPubKey = getPubKey();
	sendToCli( (void *)cliPubKey, BUFSIZ, cliSock, 1);


	// Check If Exists
	struct userInfo usr = checkUserbase(userNameString);
	// User Does Not Exist Yet
	bool userExists;

	if ( usr.UN.compare("") == 0) {
		userExists = false;
		usr.UN = userNameString ;
        sendToCli((void *)deny, BUFSIZ, cliSock, 1);   
                
	}
	// User Exists
	else {
		userExists = true;
        sendToCli((void *)confirm, BUFSIZ, cliSock, 1);
	}
        
    // Receive Size of Password Hash From Client
	short int passSize;
	char *passHash;
	char * decrPass;
	bool passwordGood = false;
	while(!passwordGood){
		passSize = * ((short int *) getCliMsg(cliSock, sizeof(short int)));
		// Receive Password Hash
		passHash = (char *) getCliMsg(cliSock, passSize);

		// decrypt password
		decrPass = decrypt(passHash); 
	 
		// Check if Password Matches for existing user
		if(userExists) {
			if( strcmp(decrPass, usr.PW.c_str())  == 0 ) {
				passwordGood = true;
				sendToCli((void *)confirm, BUFSIZ, cliSock, 1);
		 } else {
				sendToCli((void *)deny, BUFSIZ, cliSock, 1);  
		 }
		} 
		// Set Password for new user 
		else {
			std::string temp(decrPass);
			usr.PW = temp;
			if (storeUserInfo(usr) == -1) {
			 std::cerr << "Could not create new user: " << strerror(errno) << std::endl;   
		 }
			passwordGood = true;
		}
	}

	// Receive Client's Public Key
	int keySize = *  ((int *) getCliMsg(cliSock, sizeof(int)));
	char * userPubKey = (char *) getCliMsg(cliSock, keySize);
	
	// Add Client's Public Key and UserName to Current Active Users
	std::string usrPubKeyStr(userPubKey);
	if ( pthread_mutex_lock(&lock) != 0 ) {
		std::cerr << "Mutex Error on Lock()\n";
		std::exit(-1); // TODO close stuff out
	} 
	onlineUserKeys.insert({usr.UN, usrPubKeyStr});
	onlineUserSockets.insert({usr.UN, cliSock});
	if ( pthread_mutex_unlock(&lock) != 0 ) {
		std::cerr << "Mutex Error on Unlock()\n";
		std::exit(-1); // TODO close stuff out
	} 

	mainBoard(userPubKey, cliSock, usr);
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
