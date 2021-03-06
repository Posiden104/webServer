#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <errno.h>


using namespace std;

const char* usage =
"																	\n"
"Http server:														\n"
"For correct usage type: \n"
"	myhttpd [-f|-t|-p] [<port>]			\n"
"\n"
"Where 1024 < port < 65536 \n";

int QueueLength = 5;
int serverMode = 0;
int processes = 0;

void processRequest(int socket);
void threadAccept(int masterSocket);
void poolAccept(int masterSocket);

int main( int argc, char **argv) {
	int port = 1025;

	// Print usage if not enough arguments
  	if ( argc < 2 ) {
		//fprintf( stderr, "%s", usage );
		fprintf(stderr, "Using port 1025\n");
  	} else if(argc == 2 && *argv[1] != 45) {
		// Get the port from the arguments
		port = atoi( argv[1] );
	} else if(argc == 2 || argc == 3) {
		// find port
		if(argc == 2){
			// using - option without port	
			fprintf(stderr, "Using port 1025\n");
		} else {
			// using - option with port
			port = atoi(argv[2]);
		}

		// find option
		if(!strcmp(argv[1], "-f")){
			// using process
			serverMode = 1;
			printf("You are using a forking strategy\n");
		} else if(!strcmp(argv[1], "-t")){
			// using threads
			serverMode = 2;
			printf("You are using a thread strategy\n");
		} else if(!strcmp(argv[1], "-p")){
			// using thread pool
			serverMode = 3;
			printf("You are using a thread pool strategy\n");
		} else {
			// invalid
			port = 0;
		}
	} else {
		// invalid options
		port = 0;
	}

	if(port < 1024 || port > 65536){
		fprintf(stderr, "%s", usage);
		exit(-1);
	}


	signal(SIGPIPE, SIG_IGN);
  
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
	
	if(serverMode > 1){
		// switch to different method of socket distribution
		if(serverMode == 2){
			// Threads
			threadAccept(masterSocket);
		} else if(serverMode == 3){
			// Thread Pool
			poolAccept(masterSocket);
		} else {
			perror("invalid serverMode");
			exit(-1);
		}
	}

	while ( 1 ) {
	
	  // Accept incoming connections
	  struct sockaddr_in clientIPAddress;
	  int alen = sizeof( clientIPAddress );
	  
	  pid_t slave = 0;

	  // forking mode
	  if(serverMode){
		slave = fork();
	  }

	  if(slave < 0){
		perror("fork");
	  } else if(slave == 0){
		 // child or basic server 
		 int slaveSocket = accept( masterSocket,
				    (struct sockaddr *)&clientIPAddress,
				    (socklen_t*)&alen);
	
		  if ( slaveSocket == -1 && errno == EINTR ) {
			 continue;
		  }

		  processes ++;
		  processRequest(slaveSocket);

		  // shutdown and close
		  shutdown(slaveSocket, SHUT_WR);
		  close(slaveSocket);
		  processes --;
		  printf("thread socket closed. Processes left: %d\n", processes);
		  exit(EXIT_SUCCESS);
	  } else {
		// parent
		if(waitpid(slave, NULL, 0) < 0){
			perror("failed to collect child processes");
			exit(-1);
		}
	  }
	}
}

void processRequestThread(int socket){
	// process
	processRequest(socket);

	// shutdown and close
	shutdown(socket, SHUT_WR);
	close(socket);
}

void threadAccept(int masterSocket){
	while(1){
	  // Accept incoming connections
	  struct sockaddr_in clientIPAddress;
	  int alen = sizeof( clientIPAddress );
	  int slaveSocket = accept( masterSocket,
				    (struct sockaddr *)&clientIPAddress,
				    (socklen_t*)&alen);
	
	  if ( slaveSocket == -1 && errno == EINTR ) {
	    continue;
	  }

	  // init thread
	  pthread_attr_t attr;
	  pthread_attr_init(&attr);

	  pthread_t slaveThread;
	  pthread_create(&slaveThread, &attr, (void *(*)(void*))processRequestThread, (void*)slaveSocket);
	}
}

void poolSlave(int masterSocket){
	while(1){
	  // Accept incoming connections
	  struct sockaddr_in clientIPAddress;
	  int alen = sizeof( clientIPAddress );
	  int slaveSocket = accept( masterSocket,
				    (struct sockaddr *)&clientIPAddress,
				    (socklen_t*)&alen);
	
	  if ( slaveSocket == -1 && errno == EINTR ) {
	    continue;
	  }

	  // process
	  processRequest(slaveSocket);

	  // shutdown and close
	  shutdown(slaveSocket, SHUT_WR);
	  close(slaveSocket);
	}
}

void poolAccept(int masterSocket){
	// init thread
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	
	// create thread pool
	pthread_t tid[5];
	for(int i=0; i < 5; i++){
		pthread_create(&(tid[i]), &attr, (void*(*)(void*))poolSlave, (void*)masterSocket); 
	}
}

void fourOhFour(int fd, int fileNotFound){
/*	
 	const char * fof1 = "HTTP/1.0 404FileNotFound\n";
	const char * fof2 = "Server: CS 252 lab5\n";
	const char * fof3 = "Content-type: text/html\n";
	const char * fof4 = "Content-Length: ";
	const char * errMsg = "Invalid File Path. Do not navigate above root directory";
	const char * fnf = "Could not find the specified URL. The server returned an error";
	
	write(fd, fof1, strlen(fof1));
	write(fd, fof2, strlen(fof2));
	write(fd, fof3, strlen(fof3));
	write(fd, fof4, strlen(fof4));	

	if(fileNotFound){
		write(fd, "62\n" , 4);
		write(fd, "\n", 2);
		write(fd, fnf, strlen(fnf));
	} else {
		write(fd, "55\n", 4);
		write(fd, errMsg, strlen(errMsg));
	}
	sleep(2);
*/

	const char * m_file_not_found =   
		"HTTP/1.1 404 NOT FOUND\n"            
		"Server: myhttpd\n"                   
		"Content-Length: 270\n"               
		"Connection: close\n"                 
		"Content-Type: text/html; charset=iso-8859-1\n"            
		"\n"             
		"<DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"   
		"<html><head>\n"; 
	const char * m_file_not_found2 = 
		"<title>404 File Not Found!</title>\n"
		"</head><body>\n"
		"<h1>File Not Found</h1>\n"           
		"<p>The file you requested cannot be found on the server.<br />\n"         
		"</p>\n"         
		"<hr>\n"         
		"<address>CS252 Server at localhost</address>\n"           
		"</body></html>\r\n\r\n";
	const char * m_file_not_found3 = 
		"<title>404 Navigated Too High!</title>\n"
		"</head><body>\n"
		"<h1>File Navigation Error</h1>\n"           
		"<p>The file you requested cannot be found on the server.  "
		"Please do not navigate above the root directory.<br />\n"         
		"</p>\n"         
		"<hr>\n"         
		"<address>CS252 Server at localhost</address>\n"           
		"</body></html>\r\n\r\n";


	write(fd, m_file_not_found, strlen(m_file_not_found));

	if(fileNotFound){
		write(fd, m_file_not_found2, strlen(m_file_not_found2));
	} else {
		write(fd, m_file_not_found3, strlen(m_file_not_found3));
	}

	return;
}

int endsWith(const char* str, const char* suffix){
	if(!str || !suffix){
		return 0;
	}
	size_t lenstr = strlen(str);
	size_t lensuffix = strlen(suffix);
	if(lensuffix > lenstr)
		return 0;
	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
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

	if(cwd[strlen(cwd) - 1] == '/'){
		printf("ended with /");
		cwd[strlen(cwd) - 1] = 0;
	}

	printf("Final Docpath: %s\n", cwd);

	// Check if docpath is above /http-root-dir
	if(strlen(cwd) < rootLen){
		fourOhFour(fd, 0);
	}

	// Generate response
	const char * Head = "HTTP/1.0 ";
	const char * TwoHundo = "200 Document follows \n";
	const char * ServCont = "Server: CS 252 lab5\nContent-type: ";
	//const char * FourOhFour = "404 File Not Found\n";
	const char * StartTransmission = "\n";
	//const char * ErrMsg = "Could not find the specified URL. The server returned an error.";
	const char * Plain = "text/plain\n";
	const char * Html = "text/html\n";
	const char * Gif = "image/gif\n";

	// open document
	//ifstream file (cwd);
	int file = open(cwd, O_RDONLY);

	if(file != -1){
		// write protocol
		write(fd, Head, strlen(Head));
		write(fd, TwoHundo, strlen(TwoHundo));
		write(fd, ServCont, strlen(ServCont));

		// write content type
		if(endsWith(cwd, ".html")){
			write(fd, Html, strlen(Html));
		} else if(endsWith(cwd, ".gif")){
			write(fd, Gif, strlen(Gif));
		} else {
			write(fd, Plain, strlen(Plain));
		}
		write(fd, StartTransmission, strlen(StartTransmission));
		
		// send the file
		struct stat stat_buff;
		fstat(file, &stat_buff);

		int rc = sendfile(fd, file, 0, stat_buff.st_size);
	
		write(fd, "\n\n", 2);

		//file.close();
		close(file);
	} else {
		// File Not Found
		fourOhFour(fd, 1);
	}
}
