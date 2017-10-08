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
#define MAX_FDS 200

struct clients
{
    struct sockaddr_in client;
    char ip[15];
    char port[10];
    time_t timer;
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

void start_server(int *sockfd, struct sockaddr_in *server);
void post_request(char* in_buffer, int connfd, char* host_ip, char* host_port, char* ip_addr, int buffer_int);
void get_request(int connfd, char* host_ip, char* host_port, char* ip_addr, int buffer_int);
void unsupported_request(int connfd, int buffer_int);
void client_logger(char* host_ip, char* host_port, char in_buffer[1024], char* ip_addr);
void head_request(int connfd, int buffer_int);
void request(char* buffer, int connfd, char* host_ip, char* host_port, char* ip_addr, int buffer_int);

/*
void print_arr(pollfd* fds) {
    for (int i = 0; i < MAX_FDS; i++) {
        fprintf(stdout, "%d ", fds[i].fds);
    }
    fprintf(stdout, "\n");
}*/

int check_time_outs(pollfd* fds, struct clients* client_array, int number_of_clients)
{
    int someone_timed_out = 0;
    for (int i = 1; i < number_of_clients; i++) {
        if (fds[i].fd != -1) {
            time_t current = time(NULL);
            if (difftime(current, client_array[i].timer) >= 5) { //TODO SET 30
                debug("cunt");
                someone_timed_out = 1;
                close(fds[i].fd);
                fds[i].fd = -1;
            }
        }
    }
    return someone_timed_out;
}

int main()
{
    int sockfd = 0, rc;
    int run_server = 1;
    int connfd = 0;
    int nfds = 1, current_size = 0;
    int compress_array = 0;
    struct sockaddr_in server;
    struct clients client_array[MAX_FDS];
    char buffer[1024];
    pollfd fds[MAX_FDS];

    debug("starting server");
    start_server(&sockfd, &server);

    memset(fds, 0, sizeof(fds));
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    while(run_server == ISTRUE){
        memset(buffer, 0, sizeof(buffer));
        debug("server running");

        rc = poll(fds, MAX_FDS, 5000);
        debug("Poll");
        if(rc < 0){ exit (-1);}
        if (rc != 0) {
            for(int i = 0; i < MAX_FDS; i++){
                if(fds[i].fd == -1 || !(fds[i].revents & POLLIN)){
                    continue;
                }
                if(i == 0){
                    socklen_t len = (socklen_t)sizeof(struct sockaddr_in);
                    memset(client_array[nfds].ip, 0, 15);
                    memset(client_array[nfds].port, 0, 10);

                    fds[nfds].fd = accept(sockfd, (struct sockaddr*)&client_array[nfds].client, &len);
                    fds[nfds].events = POLLIN;

                    client_array[nfds].timer = time(NULL);

                    fprintf(stdout, "NEW CLIENT (i,fd): (%d, %d)\n", fds[nfds].fd, nfds);fflush(stdout);

                    getnameinfo((struct sockaddr *)&client_array[nfds].client, len, client_array[nfds].ip, 15, 
                    client_array[nfds].port, 10, NI_NUMERICHOST | NI_NUMERICSERV);
        
                    char ip_addr[INET_ADDRSTRLEN];
                    memset(ip_addr, 0, sizeof(ip_addr));

                    inet_ntop(AF_INET, &client_array[nfds].client.sin_addr, ip_addr, INET_ADDRSTRLEN);
                    nfds++;

                }
                else {
                    fprintf(stdout, "Message from client (i,fd) = (%d, %d)\n", i, fds[i].fd);fflush(stdout);

                    rc = recv(fds[i].fd, buffer, sizeof(buffer) -1, 0);

                    if(rc == 0){
                        close(fds[i].fd);
                        fds[i].fd = -1;
                        compress_array = ISTRUE;
                    } else {
                        // do stuff for a simgle client request

                        request(buffer, fds[i].fd, client_array[i].ip, client_array[i].port, client_array[i].ip, rc);
                        client_array[i].timer = time(NULL);
                    }
                }
            }
        }
        if (check_time_outs(fds, client_array, nfds)) compress_array = ISTRUE;
        if(compress_array){
            compress_array = ISFALSE;
            for(int i = 0; i < nfds; i++){
                if(fds[i].fd == -1){
                    for(int j = i; j < nfds; j++){
                        fprintf(stdout, "%d replacing %d\n", fds[j+1].fd, fds[j].fd);
                        memcpy(&client_array[j+1], &client_array[j], sizeof(client_array[j]));
                        fds[j].fd = fds[j+1].fd;
                    }
                    nfds--;
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
void request(char* buffer, int connfd, char* host_ip, char* host_port, char* ip_addr, int buffer_int){
        
        if(buffer[0] == 'G'){
            debug("buffer[0] get");
            if(buffer[5] != ' '){
               
                unsupported_request(connfd, buffer_int);
            }
            else {
                get_request(connfd, host_ip, host_port, ip_addr, buffer_int);      
            }     
        }
        else if(buffer[0] == 'P'){
            post_request(buffer, connfd, host_ip, host_port, ip_addr, buffer_int);
        }
        else if(buffer[0] == 'H'){

            head_request(connfd, buffer_int);
        }
        else{
            unsupported_request(connfd, buffer_int);
        }
}

//TODO: IMPLEMENT
void post_request(char* in_buffer, int connfd, char* host_ip, char* host_port, char* ip_addr, int buffer_int){
    char send_buffer[1024];
    char final_send_buffer[1024];
    memset(send_buffer, 0, sizeof(send_buffer));
    memset(final_send_buffer, 0, sizeof(final_send_buffer));
    
    char** split_buffer = g_strsplit(in_buffer, "pdata=", 2);
    strcpy(send_buffer, "HTTP/1.1 200 OK\r\nDate: Mon, 27 Jul 2009 12:28:53 GMT\r\nContent-type: text/html\r\nContent-Length: ");
    
    char tmp_buffer[2000];
    memset(tmp_buffer,0,sizeof(tmp_buffer));
    strcpy(tmp_buffer, "<html><head><title>WebSite</title></head><body><p>");
    strcat(tmp_buffer, "http://");
    strcat(tmp_buffer, ip_addr);
    strcat(tmp_buffer, " ");
    strcat(tmp_buffer, host_ip);
    strcat(tmp_buffer, ":");
    strcat(tmp_buffer, host_port);
    strcat(tmp_buffer, "</p>");
    strcat(tmp_buffer, "<form method=\"post\">POST DATA: <input type=\"text\" name=\"pdata\"><input type=\"submit\" value=\"Submit\">");
    strcat(tmp_buffer, split_buffer[1]);
    strcat(tmp_buffer, "</body></html>");

    char len_buf[10];
    memset(len_buf, 0, sizeof(len_buf));
    sprintf(len_buf, "%zu", strlen(tmp_buffer));

    strcat(send_buffer, len_buf);
    strcat(send_buffer, "\r\n\r\n");
    strcat(send_buffer, tmp_buffer);
    send(connfd, send_buffer, strlen(send_buffer), 0);
}

/*
* Generates response to request that is not GET, POST or HEAD
*/
void unsupported_request(int connfd, int buffer_int){
    //In memory html
    char* send_buffer = "404 Not Found\r\n"
    "Content-type: text/html\r\n"
    "\r\n"
    "<html>\r\n"
    " <body>\r\n"
    "  <h1>Not Found</h1>\r\n"
    " </body>\r\n"
    "</html>\r\n";
    send(connfd, send_buffer, strlen(send_buffer), 0);
}

/*
* Generates response to GET request
*/
void get_request(int connfd, char* host_ip, char* host_port, char* ip_addr, int buffer_int) {

    char send_buffer[10000];
    memset(send_buffer, 0, sizeof(send_buffer));
    //In memory html

    strcpy(send_buffer, "HTTP/1.1 200 OK\r\nDate: Mon, 27 Jul 2009 12:28:53 GMT\r\nContent-type: text/html\r\nContent-Length: ");
    
    char tmp_buffer[2000];
    memset(tmp_buffer,0,sizeof(tmp_buffer));
    strcpy(tmp_buffer, "<html><head><title>WebSite</title></head><body><p>");
    strcat(tmp_buffer, "http://");
    strcat(tmp_buffer, ip_addr);
    strcat(tmp_buffer, " ");
    strcat(tmp_buffer, host_ip);
    strcat(tmp_buffer, ":");
    strcat(tmp_buffer, host_port);
    strcat(tmp_buffer, "</p>");
    strcat(tmp_buffer, "<form method=\"post\">POST DATA: <input type=\"text\" name=\"pdata\"><input type=\"submit\" value=\"Submit\">");
    strcat(tmp_buffer, "</body></html>");

    char len_buf[10];
    memset(len_buf, 0, sizeof(len_buf));
    sprintf(len_buf, "%zu", strlen(tmp_buffer));

    strcat(send_buffer, len_buf);
    strcat(send_buffer, "\r\n\r\n");
    strcat(send_buffer, tmp_buffer);

    debug(send_buffer);

    send(connfd, send_buffer, strlen(send_buffer), 0);

}

/*
* Generates response to HEAD request. 
*/
void head_request(int connfd, int buffer_int){
    char head_buffer[1024];
    memset(head_buffer, 0, sizeof(head_buffer));
    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */
    //In memory header
    char* prequel = "HTTP/1.1 200 OK\r\n"    
    "Connection: close\r\n"
    "Content-type: text/html \r\n"
    "Content-length: 0\r\n"
    "Date: "; 
    char* sequel = "Location: 127.0.0.1\r\n"
    "Server: cool server\r\n";
    head_buffer[0] = '\0';

    strcat(head_buffer, prequel);
    strcat(head_buffer, asctime(localtime(&ltime)));
    strcat(head_buffer,"\r\n");
    strcat(head_buffer, sequel);
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