#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>

const char* usage =
"																	\n"
"Http server:														\n"
"For correct usage type: \n"
"	myhttpd <port>			\n"
"\n"
"Where 1024 < port < 65536 \n";

int QueueLength = 5;

void processRequest(int socket);

int main( int argc, char **argv) {
	// Print usage
	if(argc < 2){
		fprintf( stderr, "%s", usage);
		exit(-1);
	}

	// Get port
	int port = atoi(argv[1]);

	// Set IP addr and port for server
	struct sockaddr_in serverIPAddress;
	memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
	serverIPAddress.sin_family = AF_INET;
	serverIPAddress.sin_addr.s_addr = INADDR_ANY;
	serverIPAddress.sin_port = htons((u_short)port);

	// Allocate a socket
	int masterSocket = socket(PF_INET, SOCK_STREAM, 0);
	if(masterSocket < 0){
		perror("socket");
		exit(-1);
	}

	// Set socket options to reuse port to avoid 
	// 2 min cooldown
	int optval = 1;
	int error = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, (char *) & optval, sizeof(int));
	if(error) {
		perror("bind");
		exit(-1);
	}

	// Put socket in listening mode
	// set size of queue
	error = listen(masterSocket, QueueLength);
	if(error){
		perror("listen");
		exit(-1);
	}

	while(1){
		// Accept incoming connections
		struct sockaddr_in clientIPAddress;
		int alen = sizeof(clientIPAddress);
		int slaveSocket = accept(masterSocket, (struct sockaddr*)&clientIPAddress, (socklen_t*)&alen);

		if(slaveSocket < 0){
			perror("accept");
			exit(-1);
		}

		// Process request
		processRequest(slaveSocket);

		// Close socket
		close(slaveSocket);
	}

}

void processRequest( int fd ){
	// Buffer to store file requested
	const int MaxLen = 1024;
	char file[MaxLen +1];
	int fileLen = 0;
	int n;
	int crlf = 0;
	int get = 0;

	// Current char
	unsigned char newChar;

	// Last char read 
	unsigned char lastChar = 0;

	// Parse request
	// Looking for GET <Doc Requested> HTTP... <cr><lf><cr><lf>
	// <cr> = 13
	// <lf> = 10
	// or "\n\n"
	
	while (fileLen < MaxLen && (n = read(fd, &newChar, sizeof(newChar)) > 0 )) {
		
		// If <cr><lf> (in octal)
		if(lastChar == '\015' && newChar == '\012'){
			// Discard <CR> from feed
			fileLen--;
			crlf++;

			// TODO: Handle 2 crlf in a row

			break;
		}

		// Previous chars != crlf
		crlf = 0;

		file[fileLen] = newChar;
		fileLen++;

		lastChar = newChar;
	}

	// Add null char at end of string
	file[fileLen] = 0;
	printf("Request = %s\n", file);

	// Generate response
	const char * Head = "HTTP/1.0 ";
	const char * ServCont = "Server: CS 252 lab5\nContent-type: ";
	const char * TwoHundo = "200 Document follows \n";
	const char * FourOhFour = "404 File Not Found\n";
	const char * StartTransmission = "\n";
	const char * ErrMsg = "Could not find the specified URL. The server returned an error.";
	const char * Plain = "text/plain\n";
	const char * Html = "text/html\n";

	// Send response
	
	write(fd, Head, sizeof(Head));
	write(fd, FourOhFour, sizeof(FourOhFour));
	write(fd, ServCont, sizeof(ServCont));
	write(fd, Plain, sizeof(Plain));
	write(fd, StartTransmission, sizeof(StartTransmission));
	write(fd, ErrMsg, sizeof(ErrMsg));
}
