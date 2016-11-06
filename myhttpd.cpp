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
	// Print usage if not enough arguments
  	if ( argc < 2 ) {
  	  fprintf( stderr, "%s", usage );
  	  exit( -1 );
  	}
  
  	// Get the port from the arguments
  	int port = atoi( argv[1] );
  	
  	// Set the IP address and port for this server
  	struct sockaddr_in serverIPAddress; 
  	memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
  	serverIPAddress.sin_family = AF_INET;
  	serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  	serverIPAddress.sin_port = htons((u_short) port);
  	
  	// Allocate a socket
  	int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
  	if ( masterSocket < 0) {
  	  perror("socket");
  	  exit( -1 );
  	}
	
	// Set socket options to reuse port. Otherwise we will
	// have to wait about 2 minutes before reusing the sae port number
	int optval = 1; 
	int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
			     (char *) &optval, sizeof( int ) );
	// Bind the socket to the IP address and port
	int error = bind( masterSocket,
			  (struct sockaddr *)&serverIPAddress,
			  sizeof(serverIPAddress) );
	if ( error ) {
	  perror("bind");
	  exit( -1 );
	}
	 
	// Put socket in listening mode and set the 
	// size of the queue of unprocessed connections
	error = listen( masterSocket, QueueLength);
	if ( error ) {
	  perror("listen");
	  exit( -1 );
	}
	
	while ( 1 ) {
	
	  // Accept incoming connections
	  struct sockaddr_in clientIPAddress;
	  int alen = sizeof( clientIPAddress );
	  int slaveSocket = accept( masterSocket,
				    (struct sockaddr *)&clientIPAddress,
				    (socklen_t*)&alen);
	
	  if ( slaveSocket < 0 ) {
	    perror( "accept" );
	    exit( -1 );
	  }
	
	  // Process request.
	  processRequest( slaveSocket );
	  // Close socket
	  close( slaveSocket );
	  printf("socket closed\n");
	}
}

void fourOhFour(int fd, int fileNotFound){
	const char * fof1 = "HTTP/1.0 404FileNotFound\n";
	const char * fof2 = "Server: CS 252 lab5\n";
	const char * fof3 = "Content-type: text/plain\n\n";
	const char * errMsg = "Invalid File Path. Do not navigate above root directory";
	const char * fnf = "Could not find the specified URL. The server returned an error";

	const char * m_file_not_found =   
	"HTTP/1.1 404 NOT FOUND\n"            
	"Server: myhttpd\n"                   
	"Content-Length: 270\n"               
	"Connection: close\n"                 
	"Content-Type: text/html; charset=iso-8859-1\n"            
	"\n"             
	"<DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"   
	"<html><head>\n" 
	"<title>404 File Not Found!</title>\n"
	"</head><body>\n"
	"<h1>File Not Found</h1>\n"           
	"<p>The file you requested cannot be found on the server.<\
	br />\n"         
	"</p>\n"         
	"<hr>\n"         
	"<address>CS252 Server at localhost</address>\n"           
	"</body></html>\r\n\r\n";



	int tempro = write(fd, m_file_not_found, sizeof(m_file_not_found));
	printf("after something\n%d\n", tempro);

	return;

	/*
	write(fd, "HTTP/1.0", 8);
	write(fd, " ", 1);
	printf("after something\n");
	write(fd, "404", 3);

	write(fd, fof1, sizeof(fof1));
	printf("after fof1\n");
	write(fd, fof2, sizeof(fof2));
	printf("after fof2\n");
	write(fd, fof3, sizeof(fof3));
	printf("after fof3\n");
	

	if(fileNotFound){
		write(fd, fnf, sizeof(fnf));
		printf("after fnf\n");
	} else {
		write(fd, errMsg, sizeof(errMsg));
		printf("after errmsg\n");
	}
	return;
	printf("after something\n");
	*/
}

void processRequest( int fd ){
	// Buffer to store file requested
	const int MaxLen = 1024;
	char docPath[MaxLen +1];
	char curString[MaxLen +1];
	int length = 0;
	int n;
	int crlf = 0;
	int get = 0;
	int seenDocPath = 0;
	int flag = 0;

	// Current char
	unsigned char newChar;

	// Last char read 
	unsigned char lastChar = 0;

	// Parse request
	// Looking for GET <Doc Requested> HTTP... <cr><lf><cr><lf>
	// <cr> = 13
	// <lf> = 10
	// or "\r\n"
	
	for(int i = 0; i < MaxLen; i++){
		docPath[i] = 0;
		curString[i] = 0;
	}

	while((n = read(fd, &newChar, sizeof(newChar))) > 0 && !flag){
		if(newChar == ' '){
			if(get < 1){
				get++;
			} else if(!seenDocPath){
				curString[length] = 0;
				strcpy(docPath, curString);
				
			}
		} else if(lastChar == 13 && newChar == 10){
			//printf("<crlf>\n");
			// <crlf>
			length--;
			crlf++;
			if(crlf > 1){
				flag = 1;
			}
			break;
		} else if(get) {
			crlf = 0;
			lastChar = newChar;
			curString[length] = newChar;
			length++;
		}
	}

	//printf("Request: %s\n", curString);
	printf("Requested Document: %s\n", docPath);


	// Grab correct document path
	char* cwd = (char*)calloc(256, sizeof(char));
	//char* tmp = (char*)calloc(256, sizeof(char));
	cwd = getcwd(cwd, 256);
	
	// Edit docpath
	strcat(cwd, "/http-root-dir");
	int rootLen = strlen(cwd);

	// if /icons
	if(!strncmp(docPath, "/icons", 6)){
		strcat(cwd, docPath);
	}
	// if /htdocs
	else if(!strncmp(docPath, "/htdocs", 7)){
		strcat(cwd, docPath);	
	}
	//if whole docpath is /
	else if(!strcmp(docPath, "/")){
		strcat(cwd, "/htdocs/index.html");
	}
	// else add htdocs
	else {
		strcat(cwd, "/htdocs");
		strcat(cwd, docPath);
	}

	printf("Final Docpath: %s\n", cwd);

	// Check if docpath is above /http-root-dir
	//if(strlen(cwd) < rootLen){
		fourOhFour(fd, 1);
	//}

	printf("After fof\n");
	return;

	/*
	
	// Generate response
	const char * Head = "HTTP/1.0 ";
	const char * ServCont = "Server: CS 252 lab5\nContent-type: ";
	const char * TwoHundo = "200 Document follows \n";
	const char * FourOhFour = "404 File Not Found\n";
	const char * StartTransmission = "\n";
	const char * ErrMsg = "Could not find the specified URL. The server returned an error.";
	const char * Plain = "text/plain\n";
	const char * Html = "text/html\n";

	const char * Response = "HTTP/1.0 200 File Not Found\nServer: cs252 lab5\nContent-type: text/plain\n\nCould not find specified URL. The server returned an error\n";

	// Send response
	
	FILE* fp = fopen ("./http-root-dir/htdocs/index.html", "r");
	char buffer[1024];
	fgets(buffer, 1024, fp);

	//write(fd, Response, sizeof(Response));

	
	write(fd, Head, sizeof(Head));
	write(fd, TwoHundo, sizeof(TwoHundo));
	write(fd, ServCont, sizeof(ServCont));
	write(fd, Html, sizeof(Html));
	write(fd, StartTransmission, sizeof(StartTransmission));

	//while((c = fgetc(fp)) != EOF){
//		write(fd, c, sizeof(c));
//	}
	write(fd, buffer, sizeof(buffer));

	fclose(fp);

	sleep(10);
	*/
}
