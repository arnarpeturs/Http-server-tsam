#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <stdlib.h>
#include <glib.h>
#include <assert.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <errno.h>

#define ISTRUE 1 
#define ISFALSE 0

struct clients
{
    struct sockaddr_in client;
    char ip[15];
    char port[10];
    //keep alive
};
typedef struct pollfd pollfd;
/*
* poll: https://www.ibm.com/support/knowledgecenter/en/ssw_i5_54/rzab6/poll.htm
*/
void debug(const char* x){
    fprintf(stdout, "%s\n", x);
    fflush(stdout);
}

//void cleanup(int nfds, pollfd *fds); TODO!!!!!!!!1
void start_server(int *sockfd, struct sockaddr_in *server);
void post_request(char* in_buffer, int connfd, char* host_ip, char* host_port, char* ip_addr);
void get_request(int connfd, char* host_ip, char* host_port, char* ip_addr);
void unsupported_request(int connfd);
void client_logger(char* host_ip, char* host_port, char in_buffer[1024], char* ip_addr);
void head_request(int connfd);
void request(char* buffer, int connfd, char* host_ip, char* host_port, char* ip_addr);

int main()
{
    int sockfd = 0, rc;
    int run_server = 1;
    int connfd = 0;
    int nfds = 1, current_size = 0;
    int compress_array = 0;
    struct sockaddr_in server;
    struct clients client_array[200];
    char buffer[1024];
    pollfd fds[200];

    debug("starting server");
    start_server(&sockfd, &server);
    for(int i = 1; i < 200; i++){
        fds[i].fd = -1;
    }
    memset(fds, 0, sizeof(fds));
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;
    memset(client_array, 0, sizeof(client_array));
    int next_client = 0;


    while(run_server == ISTRUE){
        int p = poll(fds, 200, -1);
        if(p < 0){ exit (-1);}
        for(int i = 0; i < 200; i++){
            if(fds[i].fd == -1 || !(fds[i].revents & POLLIN)){
                continue;
            }
            if(i == 0){
                socklen_t len = socklen(struct sockaddr_in);
                next_client++;

                debug("new client");
                fds[next_client].fd = accept(sockfd, (struct sockaddr*)&client_array[next_client].client, len);
                fds[next_client].events = POLLIN;


            }
            else {
                char buffer[1000];
                ssize_t n = recv(fds[i].fd, buffer, sizeof(buffer) -1, 0);
                if(n == 0){
                    close(fds[i].fd);
                    fds[i].fd = -1;
                }
                else{
                    buffer[n] = '\0';
                    char buffer2[1000];
                    memset(buffer2, 0, sizeof(buffer2));
                    strcat(buffer2, "HTTP/1.1 200 OK\r\nDate: Mon, 27 Jul 2009 12:28:53 GMT\r\nContent-Length: 238\r\nContent-Type: text/html\r\n\r\n");
                    strcat(buffer2, "<html><head><title>test</title></head><body><h1>Hello, World!</h1><form method=\"post\">First name: <input type=\"text\" name=\"fname\"><br>Last name: <input type=\"text\" name=\"lname\"><br><input type=\"submit\" value=\"Submit\"></form></body></html>");
                    send(fds[i].fd, buffer2, strlen(buffer2), 0);
               }
            }
        }
        
    }
}


void start_server(int *sockfd, struct sockaddr_in *server){
    int rc, on = 1;
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(*sockfd < 0){
        perror("socket failed");
        exit(-1);
    }

    rc = setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR,(char*)&on, sizeof(on));
    if(rc < 0){
        perror("setsockopt failed");
        close(*sockfd);
        exit(-1);
    }
    
    rc = ioctl(*sockfd, FIONBIO, (char*)&on);
    if(rc < 0){
        perror("ioctl failed");
        close(*sockfd);
        exit(-1);
    }
    memset(server, 0, sizeof(struct sockaddr_in));
    server->sin_family = AF_INET;
    server->sin_addr.s_addr = htonl(INADDR_ANY);
    server->sin_port = htons(32000);
    rc = bind(*sockfd, (struct sockaddr *)server, (socklen_t) sizeof(struct sockaddr_in));
    if(rc < 0){
        perror("bind failed");
        exit(-1);
    }
    rc = listen(*sockfd, 32);
    if(rc < 0){
        perror("listen failed");
        exit(-1);
    }
}
/*
* What kind of request did we recieve. 
* Calls appropriate function to generate respnse.
*/
void request(char* buffer, int connfd, char* host_ip, char* host_port, char* ip_addr){
        debug("request cock");
        if(buffer[0] == 'G'){
            if(buffer[5] != ' '){
                debug("!cock");
                unsupported_request(connfd);
            }
            else {
                get_request(connfd, host_ip, host_port, ip_addr);      
            }     
        }
        else if(buffer[0] == 'P'){
            post_request(buffer, connfd, host_ip, host_port, ip_addr);
        }
        else if(buffer[0] == 'H'){
            head_request(connfd);
        }
        else{
            debug("failed sending cock");
            unsupported_request(connfd);
        }
}

//TODO: IMPLEMENT
void post_request(char* in_buffer, int connfd, char* host_ip, char* host_port, char* ip_addr){
    char send_buffer[1024];
    char final_send_buffer[1024];
    memset(send_buffer, 0, sizeof(send_buffer));
    memset(final_send_buffer, 0, sizeof(final_send_buffer));
    send_buffer[0] = '\0';
    final_send_buffer[0] = '\0';
    //In memory html
    char* prequel = "HTTP/1.1 200 OK\r\n"
    "Content-type: text/html\r\n"
    "content-lenght: ";
    
    char* prequel2 = "\r\n\r\n"
    "<html>\r\n"
    " <body>\r\n"
    "  <p>\r\n";
    char* sequel = "  </p>\r\n"
    " </body>\r\n"
    "</html>\r\n";

    char** split_buffer = g_strsplit(in_buffer, "\r\n\r\n", 2);

    send_buffer[0] = '\0';
    strcat(send_buffer, prequel2);
    strcat(send_buffer, "http://");
    strcat(send_buffer, ip_addr);
    strcat(send_buffer, " ");
    strcat(send_buffer, host_ip);
    strcat(send_buffer, ":");
    strcat(send_buffer, host_port);
    strcat(send_buffer, " ");
    strcat(send_buffer,split_buffer[1]);
    strcat(send_buffer, sequel);
    
    size_t content_lenght = strlen(send_buffer);

    strcat(final_send_buffer, prequel);

    sprintf(final_send_buffer + strlen(final_send_buffer), "%zu", content_lenght);
    strcat(final_send_buffer, send_buffer);
    debug("this is final send buffer biatch!");
    debug(final_send_buffer);
    debug("post cock");
    send(connfd, final_send_buffer, strlen(final_send_buffer), 0);
}

/*
* Generates response to request that is not GET, POST or HEAD
*/
void unsupported_request(int connfd){
    //In memory html
    char* send_buffer = "404 Not Found\n"
    "Content-type: text/html\n"
    "\n"
    "<html>\n"
    " <body>\n"
    "  <h1>Not Found</h1>\n"
    " </body>\n"
    "</html>\n";
    debug("unsupported cock");
    send(connfd, send_buffer, strlen(send_buffer), 0);
}

/*
* Generates response to GET request
*/
void get_request(int connfd, char* host_ip, char* host_port, char* ip_addr) {
    char send_buffer[1024];
    memset(send_buffer, 0, sizeof(send_buffer));
    //In memory html
    char* prequel = "HTTP/1.1 200 OK\n"
    "Content-type: text/html\n"
    "\n"
    "<html>\n"
    " <body>\n"
    "  <p>\n";
    char* sequel = "  </p>\n"
    " </body>\n"
    "</html>\n";

    send_buffer[0] = '\0';
    strcat(send_buffer, prequel);
    strcat(send_buffer, "http://");
    strcat(send_buffer, ip_addr);
    strcat(send_buffer, " ");
    strcat(send_buffer, host_ip);
    strcat(send_buffer, ":");
    strcat(send_buffer, host_port);
    strcat(send_buffer, sequel);
    debug(send_buffer);
    debug("get cock");
    send(connfd, send_buffer, strlen(send_buffer), 0);
}

/*
* Generates response to HEAD request. 
*/
void head_request(int connfd){
    char head_buffer[1024];
    memset(head_buffer, 0, sizeof(head_buffer));
    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */
    //In memory header
    char* prequel = "HTTP/1.1 200 OK\n"    
    "Connection: close\r\n"
    "Content-type: text/html \r\n"
    "Content-length: 0\r\n"
    "Date: "; 
    char* sequel = "Location: 127.0.0.1\r\n"
    "Server: cool server\r\n";
    head_buffer[0] = '\0';

    strcat(head_buffer, prequel);
    strcat(head_buffer, asctime(localtime(&ltime)));
    strcat(head_buffer,"\n");
    strcat(head_buffer, sequel);
    debug("head cock");
    send(connfd, head_buffer, strlen(head_buffer), 0);
}

/*
* Logs information from client
*/
void client_logger(char* host_ip, char* host_port, char in_buffer[1024], char* ip_addr){
    char to_file_buffer[1000];
    char* request_method;
    char* response_code;
    to_file_buffer[0] = '\0';
    FILE *fptr;

    fptr = fopen("client_logger.log", "a");
    if(fptr == NULL)
    {
      printf("Error!");
      exit(1);
    }
    // timestamp : <client ip>:<client port> <request method> <requested URL> : <response code> 
    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */

    if(in_buffer[0] == 'G'){
        request_method = "GET";
        response_code = "200 OK";
    }
    else if(in_buffer[0] == 'P') {
        request_method = "POST";
        response_code = "200 OK";
    }
    else if(in_buffer[0] == 'H') {
        request_method = "HEAD";
        response_code = "200 OK";
    }
    else{
        //custom response for unautherised request
        request_method = "ERROR";
        response_code = "400 Bad Request";
    }

    char** split_buffer = g_strsplit(in_buffer, " ", 3);
    
    strcat(to_file_buffer, asctime(localtime(&ltime)));
    strcat(to_file_buffer, " : ");
    strcat(to_file_buffer, host_ip);
    strcat(to_file_buffer, ":");
    strcat(to_file_buffer, host_port);
    strcat(to_file_buffer, " ");
    strcat(to_file_buffer, request_method);
    strcat(to_file_buffer, " ");
    strcat(to_file_buffer, ip_addr);
    strcat(to_file_buffer, split_buffer[1]);
    strcat(to_file_buffer, " : ");
    strcat(to_file_buffer, response_code);

    fprintf(fptr,"\r\n%s", to_file_buffer);
    fclose(fptr);
}