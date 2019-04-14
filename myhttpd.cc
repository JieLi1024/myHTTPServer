#include <stdio.h>
#include <sys/resource.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>
#include<stdlib.h> 
#include <cstring>
#include <string>
#include <dirent.h>
#include <math.h>
#include <algorithm>
#include <vector> 
#include <sys/stat.h>
#include <unordered_map> 
#include <dlfcn.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h> 
#include <fcntl.h>



const char* usage = 
"                                                           \n"
"usage: myhttpd [Options] [<port>]                          \n"
"                                                           \n"
"Options:                                                   \n"
"[none]: a basic server with no concurrency                \n"
"-f: create a new process for each request                  \n"
"-t: create a new thread for each request                   \n"
"-p: pool of threads                                        \n"
"                                                           \n"
"Choose 1024 <= port <= 65536                                 \n"
"If port is not specified, then a default port 1999 is used \n";


int debug = 0; //enable debug message
int Authentication = 0;
int DEFAULT_port = 43999;
int DEFAULT_mode = 0;
const char* KEY = "bGkyODkxOjg4OA==";
const int NA = 0;
const int ND = 1;
const int MA = 2;
const int MD = 3;
const int SA = 4;
const int SD = 5;
const int DA = 6;
const int DD = 7;

std::unordered_map<std::string,void*> openlibrary;
int QueueLength = 5;
int mode;    //0 for none; 1 for -f; 2 for -t; 3 for -p
int port;
time_t startime;
int num_req = 0;
double min_time=10000;
double max_time=0;

pthread_mutex_t mt;
pthread_mutexattr_t mattr;


void processRequestThread(int socket);
void poolSlave(int socket);
void poolOfThreads(int masterSocket);
void processRequest(int socket);

bool isAuthorized(char* name, int socket);
void sendOkHeader(int socket, char* contentType);
void send404Header(int socket);
int recognizeCO(char* docPath);
void displayDir(int socket, char* cwd, char* docPath,int typeCO);
void write_parent(int socket, const char * menu);
void cgiBinCase(int socket, char* cwd);
void loadModule(int socket, char* fileName, char* query);
void Log(int socket, char * cwd);
void send_stats(int socket);


struct fileInfo{
	std::string type;
	std::string path;
	std::string name;
	std::string time;
	std::string size;
};

std::vector<fileInfo> sort(std::vector<fileInfo> itemVec, int typeCO);

bool compNameA(fileInfo f1, fileInfo f2){
	return(f1.name<f2.name);
}
bool compTimeA(fileInfo f1, fileInfo f2){
	return(f1.time<f2.time);
}
bool compSizeA(fileInfo f1, fileInfo f2){
	return(f1.size<f2.size);
}
bool compNameD(fileInfo f1, fileInfo f2){
	return(f1.name>f2.name);
}
bool compTimeD(fileInfo f1, fileInfo f2){
	return(f1.time>f2.time);
}
bool compSizeD(fileInfo f1, fileInfo f2){
	return(f1.size>f2.size);
}
extern "C" void killzombie(int sig)
{
  int pid = 1;
  while(waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char** argv){
    time(&startime);

    port = DEFAULT_port;
    mode = DEFAULT_mode;

/************* Get mode and port from input arguments START **********/
    //print usage if input in incorrect format
    if(argc > 3){
        fprintf(stderr, "%s", usage);
    }
    if(argc == 3 && argv[1][0] != '-'){
        fprintf(stderr, "%s", usage);
    }

    if(argc == 1){
        //keep port and mode as DEFAULT
    }
    else if(argc > 1 && argv[1][0] == '-'){
        if(argv[1][1] == 'f'){
            mode = 1;
            if(debug == 1) printf("mode: 1\n");
        }
        else if(argv[1][1] == 't'){
            mode = 2;
            if(debug == 1) printf("mode: 2\n");
        }
        else if(argv[1][1] == 'p'){
            mode = 3;
            if(debug == 1) printf("mode: 3\n");
        }
        else{
            fprintf(stderr, "%s", usage);
            exit(1);
        }
    }
    //get port from arguments or use default port
    //|| ( argc == 2 && isdigit(*argv[1]) )
    if(argc == 3  ){
        int temp = atoi(argv[2]);
        if(temp == 0){
            fprintf(stderr, "%s", usage);
            exit(1);
        }else{
            port = temp;
        }
    }else if( argc == 2 && isdigit(*argv[1])){
        int temp = atoi(argv[1]);
        if(temp == 0){
            fprintf(stderr, "%s", usage);
            exit(1);
        }else{
            port = temp;
        }
    }else{
        port = DEFAULT_port;
    }
    
    //check number of port
    if(port < 1024 || port > 65535){
        fprintf(stderr, "%s", usage);
        exit(1);
    }

    if(debug == 1) printf("port: %d\n", port);
    if(debug == 1) printf("mode: %d\n", mode);

/************* Get mode and port from input arguments END **********/

/************* Deal with zombie processes START **********/
    struct sigaction sigAction;
   
    sigAction.sa_handler = killzombie;
    sigemptyset(&sigAction.sa_mask);
    sigAction.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD,&sigAction, NULL))
    {
        perror("sigaction");
        exit(2);
    }

/************* Deal with zombie processes END   **********/

    if (debug == 1) printf("Launching web server on port %d... \n", port);

    //Set IP address and port for this server
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
        exit(1);
    }

    if (debug == 1) printf("Socket is listening...\n\n");

    if(mode == 3){
        poolOfThreads(masterSocket);
    }else{
        while(1){
            //Accept incoming connections
            struct sockaddr_in clientIPAddress;
            int Alen = sizeof(clientIPAddress);
            int slaveSocket = accept(masterSocket,
                                (struct sockaddr*)&clientIPAddress,
                                (socklen_t*)&Alen);
            
            if(slaveSocket < 0){
                perror("SlaveSocket Accept Error");
                exit(1);
            }

            num_req++;



            if(mode == 0){
                time_t begin_t;
                time_t end_t;
                time(&begin_t);

                processRequest(slaveSocket);
                close(slaveSocket);

                time(&end_t);
                double diff_time = difftime(end_t, begin_t);
                if(diff_time > max_time){
                    max_time = diff_time;
                }
                if((diff_time) < min_time){
                    min_time = diff_time;
                }
            }else if(mode == 1){
                pid_t slave = fork();
                if(slave == 0){
                    processRequest(slaveSocket);
                    close(slaveSocket);
                    exit(1);
                }
                close(slaveSocket);
            }else if(mode == 2){
                pthread_t tid;
                pthread_attr_t attr;

                pthread_attr_init(&attr);
                pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

                pthread_create(&tid, &attr, (void* (*)(void*))processRequestThread, (void*)slaveSocket);
            }
        }
    }
}


void processRequestThread(int socket){
    processRequest(socket);
    close(socket);
}

void poolSlave(int socket){
    while(1){
        pthread_mutex_lock(&mt);

        //Accept incoming connections
        struct sockaddr_in clientIPAddress;
        int Alen = sizeof(clientIPAddress);
        int slaveSocket = accept(socket, 
                        (struct sockaddr *)&clientIPAddress,
                        (socklen_t*)&Alen);
        
        pthread_mutex_unlock(&mt);

        if ( slaveSocket < 0 ) {
            //EINTR error code if a signal occurred 
            //while the system call was in progress
            //ignore this interrupt
            if(errno == EINTR){
                continue;
            }
            perror( "accept" );
            exit( -1 );
        }

        // Process request.
        processRequest( slaveSocket );

        // Close socket
        close( slaveSocket );   
    }
}


void poolOfThreads(int masterSocket){
    pthread_mutexattr_init(&mattr);
    pthread_mutex_init(&mt, &mattr);

    pthread_t tid[QueueLength];
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    for(int i=0; i < QueueLength; i++){
        pthread_create(&tid[i], &attr, (void* (*)(void*))poolSlave, (void*)masterSocket);
    }

    pthread_join(tid[0], NULL);
}
        
void processRequest(int socket){
    // Buffer used to store the name recieved from the client
    if(debug == 1) printf("processRequest Start\n");
    const int MaxName = 1024;
    char name[ MaxName + 1 ] = {0};
    int nameLength = 0;
    //int n;
    char newChar; //currently reading
    char hasGET = 0;
    char nameDoc[1025] = {0};
    int nameDocLength = 0;
    int docCheck = 0;   
    int numOfSpaces = 0;

    /**
     * Syntax of a request:
     * GET <sp> <Document Requested> <sp> HTTP/1.0 <crlf>
     * {<Other Header Information> <crlf>}*
     * <crlf>
     * Stop reading only when 2 consecutive <crlf> are met
     * 
    **/

    /*** 1. Read the HTTP header ***/
    while(nameLength < MaxName && read(socket, &newChar, sizeof(newChar))>0){
        //buffer used to store name from the client
        name[nameLength++] = newChar;

        if(docCheck){
            char character;
            nameDoc[nameDocLength++] = newChar;
            while((read(socket, &newChar, sizeof(newChar)))){
                if(newChar == ' '){
                    nameDoc[nameDocLength++] = '\0';
                    docCheck = 0; //Stop checking doc
                    break;
                }
                name[nameLength++] = newChar;
                nameDoc[nameDocLength++] = newChar;

            }
        }

        if(newChar == 'T'){
            if(name[nameLength-2] == 'E' && name[nameLength-3] == 'G'){
                hasGET = 1; //found GET
            }
        }

        else if(newChar == ' '){
            //if is the first space, start checking doc
            if(hasGET && numOfSpaces == 0){
                docCheck = 1;
            }
            numOfSpaces++;
        }

        //break only when \r\n\r\n is met
        if(newChar == '\n'){
            if(name[nameLength-2] == '\r' &&
               name[nameLength-3] == '\n' &&
               name[nameLength-4] == '\r')
            {
                name[nameLength] = '\0';
                break;
            }
        }
    }

    if(debug == 1) printf("The GET request is: %s", name);

    /***    Authentication START ***/
    if (Authentication == 1){
        if(!isAuthorized(name, socket)){
            close(socket);
            return;
        }
    }
    /***    Authentication END  ***/


    /*** 2. get the document path                  ***/
    /*** 3. Map the document path to the real path ***/
        
    char cwd[MaxName] = {0}; //current working directory
    getcwd(cwd, sizeof(cwd));
    int length_cwd = (std::string(cwd)).length();

    int typeCO = 0;
    int isDir = 0;

    
    if(!strncmp(nameDoc, "/icons", strlen("/icons")) || 
        !strncmp(nameDoc, "/htdocs", strlen("/htdocs")) ||
        !strncmp(nameDoc, "/cgi-bin", strlen("/cgi-bin")))
    {
        strcat(cwd, "/http-root-dir/");
        strcat(cwd, nameDoc);
    }
    else if(!strncmp(nameDoc, "/dir", strlen("/dir"))){
        isDir = 1;
        typeCO = recognizeCO(nameDoc);
        strcat(cwd, "/http-root-dir/htdocs");
        strcat(cwd, nameDoc);
    }
    else{
        if(!strcmp(nameDoc, "/")){
            strcat(cwd, "/http-root-dir/htdocs/index.html");
        }else{
            strcat(cwd, "/http-root-dir/htdocs");
            strcat(cwd, nameDoc);
        }

    }


    Log(socket, cwd);

        
    //if nameDoc leads to the parent of htdocs, which is INVALID
    if (strstr(nameDoc, "..") != NULL) {
        char resolved[MaxName + 1] = {0};
        char* r = realpath(cwd, resolved);
        // printf("nameDoc: %s\n", nameDoc);
        // printf("resolved: %s\n", resolved);
        if (r == NULL) {
            perror("realpath");
            //exit(0);
        }
        if(strlen(resolved) < (length_cwd + strlen("/http-root-dir"))){
            char buffer[128] = "Access Denied\r\nHTTP/1.1 403 Forbidden\r\nServer: CS 252 lab5/1.0\r\n\r\n";
            write(socket, buffer, strlen(buffer));

            close(socket);
            return;
        }
        //Override cwd
        //????????????????? DID NOT CHECK CORRECTNESS
        strcpy(cwd, resolved);
        
        if(debug == 1) printf("The resolved path is: %s\n", resolved);
    }


    /*** Determine Content type ***/
    char contentType[MaxName + 1] = {0};

    //endsWith
    ///////////////////Log(socket, resolved)
    if (strstr(nameDoc, ".html") != NULL || strstr(nameDoc, ".html/") != NULL) {
        strcpy(contentType, "text/html");
    } else if (strstr(nameDoc, ".jpg") != NULL || strstr(nameDoc, ".jpg/") != NULL) {
        strcpy(contentType, "image/jpeg");
    } else if (strstr(nameDoc, ".gif") != NULL || strstr(nameDoc, ".gif/") != NULL) {
        strcpy(contentType, "image/gif");
    } else if (strstr(nameDoc, ".png") != NULL || strstr(nameDoc, ".png/") != NULL) {
        strcpy(contentType, "image/png");
    } else if (strstr(nameDoc, ".svg") != NULL || strstr(nameDoc, ".svg/") != NULL) {
        strcpy(contentType, "image/svg+xml");
    } else {
        strcpy(contentType, "text/plain");
    }

    if(debug==1) printf("contentType: %s\n", contentType);

    //check and handle request from cgi-bin
    if(strstr(cwd, "cgi-bin")){
        cgiBinCase(socket, cwd);
        return;
    }

    //get rid of '?' and query after it before resolve the dir
    if(isDir){
        char* ques_mark = strstr(cwd, "?");
        if(ques_mark != NULL){
            *ques_mark = '\0';
        }
    }

    DIR* directory = opendir(cwd);
    if(directory != NULL){
        displayDir(socket, cwd, nameDoc, typeCO);
        return;
    }

    //
    if (strstr(cwd,"/stats")){
        send_stats(socket);
        return;
    }

    //Open the file, send 404 if fail
    FILE * file = fopen(cwd, "rb");

    //Send HTTP Reply Header
    if (file != NULL) {
        //printf("It reaches!\n");

        sendOkHeader(socket, contentType);

        if(debug == 1) printf("HTTP Reply Header sent\n\n");

        long count = 0;

        char c;
        //Sending the file,
        //start writing after two <clrf> in the reply header
        while (count = read(fileno(file), &c, sizeof(c))) {
            if (write(socket, &c, sizeof(c)) != count) {
                perror("write error: ");
                close(socket);
                fclose(file);
                return;
            }
        }

        fclose(file);

    } else {
        //sending 404, File Not Found
        send404Header(socket);
    }
}

bool isAuthorized(char* name, int socket){
    if(strstr(name, "Authorization") == NULL){
        const char * buffer = "WWW-Authenticate: Basic realm=\"myhttpd-cs252\"\r\n\r\nHTTP/1.1 401 Unauthorized\r\n";
        write(socket, buffer, strlen(buffer));

        close(socket);
        return false;
    }else{
        char* Authen = strstr(name, "Authorization");
        char credential[100];

        char* tmp = Authen+21;
        int i = 0;
        while(*tmp != '\0'){
            credential[i++] = *tmp;
            tmp++;
        }
        credential[i] = '\0';
        int k = strcmp(credential, KEY);

        //???
        if(strcmp(credential, KEY) != 13){
            printf("wrong key");
 
            // const char * sig = "WWW-Authenticate: Basic realm=\"myhttpd-cs252\"\r\n\r\n";
            // const char * msg = "HTTP/1.1 401 Unauthorized\r\n";
            // write(socket, msg, strlen(msg));
            // write(socket, sig, strlen(sig));

            char buffer[128] = "WWW-Authenticate: Basic realm=\"myhttpd-cs252\"\r\n\r\n";
            strcat(buffer, "HTTP/1.1 401 Unauthorized\r\n");
            write(socket, buffer, strlen(buffer));


            close(socket);
            return false;
        }
        return true;
    }
}

void sendOkHeader(int socket, char* contentType){
    char buffer[128] = "HTTP/1.1 200 OK\r\nServer: MyServer/1.0\r\nContent-Type ";
    strcat(buffer, contentType);
    strcat(buffer, "\r\n\r\n");
    write(socket, buffer, strlen(buffer));
}

void send404Header(int socket){
    //TOO SLOW:
    // const char * message = "File not found\n";

    // write(socket, "HTTP/1.1 404 File Not Found", 27);
    // write(socket, "\r\n", 2);
    // write(socket, "Server: MyServer/1.0", 20);
    // write(socket, "\r\n", 2);
    // write(socket, "Content-Type: text/html", 23);
    // write(socket, "\r\n", 2);
    // write(socket, "\r\n", 2);
    // write(socket, message, strlen(message));

    const char * message = "File not found\n";

    char buffer[128] = "HTTP/1.1 404 File Not Found\r\nServer: MyServer/1.0\r\nContent-Type: text/html\r\n\r\n";
    write(socket, buffer, strlen(buffer));
    write(socket, message, strlen(message));
}

int recognizeCO(char* docPath){
    int typeCO;
    if(strstr(docPath, "C=N;O=A") != 0){
		typeCO = NA;
	}else if(strstr(docPath, "C=N;O=D") != 0){
		typeCO = ND;
	}else if(strstr(docPath, "C=M;O=A") != 0){
		typeCO = MA;
	}else if(strstr(docPath, "C=M;O=D") != 0){
		typeCO = MD;
	}else if(strstr(docPath, "C=S;O=A") != 0){
		typeCO = SA;
	}else if(strstr(docPath, "C=S;O=D") != 0){
		typeCO = SD;
	}else if(strstr(docPath, "C=D;O=A") != 0){
		typeCO = DA;
	}else if(strstr(docPath, "C=D;O=D") != 0){
		typeCO = DD;
	}else{
        //typeCO not specified, default to be 'NA'
        typeCO = 0;
    }
    return typeCO;
}

void displayDir(int socket, char* cwd, char* docPath, int typeCO){
    struct dirent *ent;
    DIR *dir;

    const char* menu;
    char buffer[256] = "HTTP/1.0 200 Document follows\r\nServer: CS 252 lab5/1.0\r\nContent-type: text/html\r\n\r\n";
    const char * msg2 = "<html>\n <head>\n  <title>Index of ";
    const char * msg3 = "</title>\n </head>\n <body>\n<h1>Index of ";  
    const char * msg4 = "</h1>\n <table><tr>";
 
    strcat(buffer, msg2);
    strcat(buffer, msg3);
    strcat(buffer, cwd);
    strcat(buffer, msg4);
    write(socket, buffer, strlen(buffer));

    //decide the sorting type: typeCO
    if(typeCO == ND || typeCO == MD || typeCO == SD || typeCO == DD){
        menu = "<tr><th><img src=\"/icons/red_ball.gif\" alt=\"[ICO]\"></th><th><a href=\"?C=N;O=A\">Name</a></th><th><a href=\"?C=M;O=A\">Last modified</a></th><th><a href=\"?C=S;O=A\">Size</a></th><th><a href=\"?C=D;O=A\">Description</a></th></tr>\n<tr><th colspan=\"5\"><hr></th></tr>";
    }

    if(typeCO == NA){
        menu = "<tr><th><img src=\"/icons/red_ball.gif\" alt=\"[ICO]\"></th><th><a href=\"?C=N;O=D\">Name</a></th><th><a href=\"?C=M;O=A\">Last modified</a></th><th><a href=\"?C=S;O=A\">Size</a></th><th><a href=\"?C=D;O=A\">Description</a></th></tr>\n<tr><th colspan=\"5\"><hr></th></tr>";
    }
    else if(typeCO == MA){
        menu = "<tr><th><img src=\"/icons/red_ball.gif\" alt=\"[ICO]\"></th><th><a href=\"?C=N;O=A\">Name</a></th><th><a href=\"?C=M;O=D\">Last modified</a></th><th><a href=\"?C=S;O=A\">Size</a></th><th><a href=\"?C=D;O=A\">Description</a></th></tr>\n<tr><th colspan=\"5\"><hr></th></tr>";
    }
    else if(typeCO == SA){
        menu = "<tr><th><img src=\"/icons/red_ball.gif\" alt=\"[ICO]\"></th><th><a href=\"?C=N;O=A\">Name</a></th><th><a href=\"?C=M;O=A\">Last modified</a></th><th><a href=\"?C=S;O=D\">Size</a></th><th><a href=\"?C=D;O=A\">Description</a></th></tr>\n<tr><th colspan=\"5\"><hr></th></tr>";
    }
    else if(typeCO == DA){
        menu = "<tr><th><img src=\"/icons/red_ball.gif\" alt=\"[ICO]\"></th><th><a href=\"?C=N;O=A\">Name</a></th><th><a href=\"?C=M;O=A\">Last modified</a></th><th><a href=\"?C=S;O=A\">Size</a></th><th><a href=\"?C=D;O=D\">Description</a></th></tr>\n<tr><th colspan=\"5\"><hr></th></tr>";
    }

    write_parent(socket, menu);

    //item can be file or folder
    std::vector<fileInfo> itemVec;

    dir = opendir(cwd); //already ensured not dir != NULL when calling func

    if(debug==1) printf("Begin adding items to itemVec...\n");
    //add items to itemVec one by one

    int isDir = 0;
    while((ent = readdir(dir)) != NULL){
        //currently reading item; item can be file or subdir
        char item_path[128];

        char* temp = strstr(cwd, "dir");
        if(temp[strlen(temp)-1] != '/'){
            strcat(temp, "/");
        }
        strcpy(item_path, "/dir1/");
		strcat(item_path, ent->d_name);

        if(strncmp(ent->d_name, ".", 1) != 0 ){
            //resolve the absolute path for using stat()
            char abs[256] = {0};
            strcpy(abs, cwd);
            if(debug == 1) printf("ent->name:  %s\n", ent->d_name);
            strcat(abs, ent->d_name);
            if(debug == 1) printf("abs  %s\n", abs);
        
            struct stat attr;
            stat(abs, &attr);

            //raw size
            float fileSize = (float)attr.st_size/1000.0;
            if(debug == 1) printf("Filesize: %f\n", fileSize);
         
            //time
            struct tm *tm;
            tm = localtime(&attr.st_ctime);
            char time_str[256] = {0};
            sprintf(time_str,"%d-%d-%d %d:%d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);

            //type
            char type[28];
            strcpy(type, "/icons/"); 
            if(strstr(ent->d_name, ".gif") != NULL){
                strcat(type, "image.gif");
            }else if(strstr(ent->d_name, "dir") != NULL){
                isDir = 1;
                strcat(type, "menu.gif");
            }else{
                strcat(type, "unknown.gif");
            }

            //readable fileSize
            char size[128]; 
            if(isDir){
                sprintf(size, "-");
            }else{
                sprintf(size, "%.1f K", fileSize);
            }

            //add item to itemVec
            fileInfo finfo;
            finfo.type = type;
            finfo.path = std::string(item_path);
            finfo.name = std::string(ent->d_name);
            finfo.time = std::string(time_str);
            finfo.size = std::string(size);
            
            itemVec.push_back(finfo);
        }
    }
        if(debug==1) printf("after push\n");

        
        // for(int i = 0; i < itemVec.size(); i++){
		// 	printf("time: %s\n", itemVec[i].time.c_str());
        //     printf("name: %s\n", itemVec[i].name.c_str());
        //     printf("size: %s\n", itemVec[i].size.c_str());
        //     printf("size: %s\n", itemVec[i].type.c_str());
        //     printf("size: %s\n", itemVec[i].path.c_str());
        // }
        
        itemVec = sort(itemVec, typeCO);
    
        char htmltext[1024];
        for(int i = 0; i < itemVec.size(); i++){
			sprintf(htmltext, "<tr><td valign=\"top\"><img src=\"%s\" alt=\"[   ]\"></td><td><a href=\"%s\">%s</a></td><td align=\"right\">%s </td><td align=\"right\"> %s</td><td>&nbsp;</td></tr>\n", itemVec[i].type.c_str(), itemVec[i].path.c_str(), itemVec[i].name.c_str(), itemVec[i].time.c_str(), itemVec[i].size.c_str());
            write(socket,htmltext, strlen(htmltext));
        }
        
        sprintf(htmltext, "<tr><th colspan=\"5\"><hr></th></tr>\n</table>\n<address>Apache/2.4.18 (Ubuntu) Server at data.cs.purdue.edu Port 10160</address>\n</body></html>");
		write(socket, htmltext, strlen(htmltext));
		closedir(dir);
}

void write_parent(int socket, const char * menu){
    write(socket, menu, strlen(menu));
    const char * msg1 = "<tr><td valign=\"top\"><img src=\"/icons/menu.gif\" alt=\"[DIR]\"></td><td><a href=\"../";
    write(socket, msg1, strlen(msg1));
    const char * msg2 = "\">Parent Directory</a></td><td>&nbsp;</td><td align=\"right\">  - </td><td>&nbsp;</td></tr>";
    write(socket, msg2, strlen(msg2));
    // close(socket); Don't do this!!! it's the same int though copied
}

std::vector<fileInfo> sort(std::vector<fileInfo> itemVec, int typeCO){
    //DD, DA are equicalent to ND, NA respectively
    switch (typeCO){
            case ND:
                std::sort(itemVec.begin(), itemVec.end(), compNameA); 
                break;
            case NA:
                std::sort(itemVec.begin(), itemVec.end(), compNameD); 
                break;
            case MD:
                std::sort(itemVec.begin(), itemVec.end(), compTimeA); 
                break;
            case MA:
                std::sort(itemVec.begin(), itemVec.end(), compTimeD); 
                break;
            case SD:
                std::sort(itemVec.begin(), itemVec.end(), compSizeA); 
                break;
            case SA:
                std::sort(itemVec.begin(), itemVec.end(), compSizeD); 
                break;
            case DD:
                std::sort(itemVec.begin(), itemVec.end(), compNameA); 
                break;
            case DA:
                std::sort(itemVec.begin(), itemVec.end(), compNameD); 
                break;
            }
    return itemVec;
}

//hangle both normal and loadable cgi-bin
void cgiBinCase(int socket, char* cwd){
    char* query = strstr(cwd, "?");
    if(query != NULL){
        *query = 0;
        query++;
    }
    //else just send query of NULL
    
    //Case1: loadable module with extension '.so'
    if(strstr(cwd, ".so") != NULL){
        //send 200 response
        write(socket, "HTTP/1.1 200 Document follows", 29);
        write(socket, "\r\n", 2 );
        write(socket, "Server: MyServer/1.0", 20);
        write(socket, "\r\n", 2 );

        char fileName[128];
        if(strstr(cwd, "hello") != NULL){
            strcpy(fileName, "hello.so");
        }else if(strstr(cwd, "jj-mod") != NULL){
            strcpy(fileName, "jj-mod.so");
        }
        loadModule(socket, fileName, query);

    }
    //normal cgi-bin
    else{
        int ret = fork();
        if(ret == 0){
            setenv("REQUEST_METHOD", "GET", 1);
            if(query != NULL){ //then it's already the query after '?'
                setenv("QUERY_STRING", query, 1);
            }
            //send 200 response
            write(socket, "HTTP/1.1 200 Document follows", 29);
            write(socket, "\r\n", 2 );
            write(socket, "Server: MyServer/1.0", 20);
            write(socket, "\r\n", 2 );

            dup2(socket, 1);
            //execute cgi-bin executable to socket
            execvp(cwd, NULL);
            perror("execvp");
            _exit(1);
            return;
        }
        //wait for child process to finish 
        int s;
        waitpid(ret, &s, 0);
    }

}

void loadModule(int socket, char* fileName, char* query){
    //check if hello.so module is already opened,
        //if not, open it and load the module
        if(openlibrary[fileName] == NULL){
            char path[12] = "./";
            strcat(path, fileName);
            printf("\npath: %s\n", path);
            void* dl = dlopen(path, RTLD_LAZY);
            //add "fileName" to openlibrary
            openlibrary[fileName] = dl;
            if(dl == NULL){
                fprintf( stderr, "dlerror:%s\n", dlerror()); 
                perror( "dlopen");
                //exit(1);
                return;
            }
        }
        //load modules
        void (*hrun)(int, char*);
        hrun = (void(*)(int, char*))dlsym(openlibrary[fileName], "httprun");
        if(hrun == NULL){
            fprintf( stderr, "dlsym:%s\n", dlerror()); 
            perror( "dlsym");
            return;
        }
        //run the module to the socket
  	 	(*hrun)(socket, query);
}


void Log(int socket, char * cwd){
    if(debug==1) printf("start logging...\n");

    char new_log[525] = {0};
    char *addr = (char *)malloc (1024 * sizeof (char));

    struct sockaddr_in sk;
    int socket_size = sizeof(sk);

    getsockname(socket, (struct sockaddr*) &sk, (socklen_t *)&socket_size);
    addr = inet_ntoa(sk.sin_addr);

    strcat(new_log, "Host: ");
    strcat(new_log, addr);
    strcat(new_log, "\nDirectory: ");
    strcat(new_log, cwd);
    strcat(new_log, "\n\n");

    // FILE * fd = fopen("./http-root-dir/htdocs/logs", "a+");

    // fprintf(fd, "%s", new_log);
    // fclose(fd);
    char file_path[128] = {0};
    strcpy(file_path, "./http-root-dir/htdocs/logs");
    int fd;
    fd = open(file_path, O_CREAT| O_WRONLY | O_APPEND, 0666);
    write(fd, new_log, strlen(new_log));
    // free(addr);
}

void send_stats(int socket){
  const char *msg4 = "</h4>";
  const char *msg1 = "HTTP/1.0 200 Document follows\r\nServer: CS 252 lab5/1.0\r\nContent-type: text/html\r\n\r\n<html>\n <head>\n  <title>Statistics</title>\n </head>\n <body>\n<h1>Statistics</h1>\n";
  write(socket, msg1, strlen(msg1));
  const char *name = "<h3>The names of the student who wrote the project:</h3><h4>";
  write(socket, name, strlen(name));
  const char *name_user = "Jie Li";
  write(socket, name_user, strlen(name_user));
  write(socket, msg4, strlen(msg4));
  const char *msg2 = "<h3>The time the server has been running:</h3><h4>";
  char *msg3 = (char*)malloc(50);
  time_t lastime;
  time(&lastime);
  double diff_time = difftime(lastime, startime);
  sprintf(msg3, "%2f", diff_time);
  write(socket, msg2, strlen(msg2));
  write(socket, msg3, strlen(msg3));
  write(socket, msg4, strlen(msg4));
  const char *msg5 = "<h3>Number of Requests Since Server Started</h3><h4>";
  char *req = (char*)malloc(50);
  sprintf(req, "%d", num_req);
  write(socket, msg5, strlen(msg5));
  write(socket, req, strlen(req));
  write(socket, msg4, strlen(msg4));
 
    const char *msg6 = "<h3>The minimum service time and the URL request that took this time: \n";
    write(socket, msg6, strlen(msg6));
    char min_t[128];
    sprintf(min_t, "%2f", min_time);
    write(socket, min_t, strlen(min_t));
    

    const char *msg7 = "<h3>The maximum service time and the URL request that took this time: \n";
    write(socket, msg7, strlen(msg7));
    char max_t[128];
    sprintf(max_t, "%2f", max_time);
    write(socket, max_t, strlen(max_t));
}