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
    //keep alive
};
typedef struct pollfd pollfd;
/*
* poll: https://www.ibm.com/support/knowledgecenter/en/ssw_i5_54/rzab6/poll.htm
*/
void debug(const char* x) {
    fprintf(stdout, "%s\n", x);
    fflush(stdout);
}

//void cleanup(int nfds, pollfd *fds); TODO!!!!!!!!1
void start_server(int *sockfd, struct sockaddr_in *server);
void post_request(char* in_buffer, int connfd, char* host_ip, char* host_port, char* ip_addr, int** rc);
void get_request(int connfd, char* host_ip, char* host_port, char* ip_addr, int** rc);
void unsupported_request(int connfd, int** rc);
void client_logger(char* host_ip, char* host_port, char in_buffer[1024], char* ip_addr);
void head_request(int connfd, int** rc);
void request(char* buffer, int connfd, char* host_ip, char* host_port, char* ip_addr, int* rc);

int main()
{
    int sockfd = 0, rc, len;
    int run_server = 1, close_conn = 0;
    int connfd = 0;
    int nfds = 1, current_size = 0;
    int compress_array = 0;
    struct sockaddr_in server;
    struct clients client_array[200];
    char buffer[1024];
    pollfd fds[200];

    debug("starting server");
    start_server(&sockfd, &server);

    memset(fds, 0, sizeof(fds));
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    //poll!!
    // Before the server can accept buffers, it has to listen to the
    // welcome port. A backlog of one connection is allowed.        // We first have to accept a TCP connection, connfd is a fresh
    // handle dedicated to this connection.
    //client ip address
    while(run_server == ISTRUE) {
        memset(buffer, 0, sizeof(buffer));
        debug("server running");
        rc = poll(fds, nfds, 30000);
        debug("poll received");
        if(rc < 0) {
            perror("poll failed");
            break;
        }
        if(rc == 0) {
            fprintf(stdout, "%s\n", "timeout after 30 secs");
            fflush(stdout);
            //TODO: continue
            break;
        }

        current_size = nfds;
        for(int i = 0; i < current_size; i++) {
            if(fds[i].revents == 0) {
                continue;
            }
            if(fds[i].revents != POLLIN) {
                debug("revent error");
                run_server = ISFALSE;
                break; // todo: not break
            }
            if(fds[i].fd == sockfd) {
                debug("listening socket is readable");

                while(connfd != -1) {
                    socklen_t socklen = (socklen_t) sizeof(client_array[i].client);
                    connfd = accept(sockfd, (struct sockaddr *)&client_array[i].client, &socklen);

                    // get info for the get request
                    struct sockaddr_in* client_address = (struct sockaddr_in*)&client_array[i].client;
                    struct in_addr client_ip = client_address->sin_addr;
                    char host_ip[1025];
                    char host_port[32];
                    memset(host_port, 0, sizeof(host_port));
                    memset(host_ip, 0, sizeof(host_ip));

                    getnameinfo((struct sockaddr *)&client_array[i].client, socklen, host_ip, sizeof(host_ip),
                                host_port, sizeof(host_port), NI_NUMERICHOST | NI_NUMERICSERV);


                    char ip_addr[INET_ADDRSTRLEN];
                    memset(ip_addr, 0, sizeof(ip_addr));

                    inet_ntop(AF_INET, &client_ip, ip_addr, INET_ADDRSTRLEN);

                    fprintf(stdout, "%d connfd is: \n", connfd);
                    fflush(stdout);

                    //TODO: búa til nýjan client í struct og setja í hashmap client[i]
                    //connfd = accept(sockfd, NULL, NULL);
                    if(connfd < 0) {
                        if(errno != EWOULDBLOCK) {
                            perror("accept failed");
                            run_server = ISFALSE;
                        }
                        connfd = 0;
                        break;
                    }
                    debug("new connection");
                    fds[nfds].fd = connfd;
                    fds[nfds].events = POLLIN;
                    nfds++;
                }
            }
            else {
                debug("fd is readable");
                close_conn = ISFALSE;
                while(ISTRUE) {

                    rc = recv(fds[i].fd, buffer, sizeof(buffer), 0);
                    if(rc < 0) {
                        if(errno != EWOULDBLOCK) {
                            perror("recv failed");
                            close_conn = ISTRUE;
                        }
                        break;
                    }
                    if(rc == 0) {
                        debug("connection closed by client");
                        close_conn = ISTRUE;
                        break;
                    }
                    len = rc;
                    //TODO: lesa úr client[i] inn í breyturnar


                    fprintf(stdout, "%s\n", ip_addr);
                    fflush(stdout);

                    client_logger(host_ip, host_port, buffer, ip_addr);
                    request(buffer, fds[i].fd, host_ip, host_port, ip_addr, &rc);
                    //struct sockaddr_in *ipv4 = (struct sockaddr_in *)clients_array[i].client->sin_addr;



                    printf(" %d bytes recieved\n", len);
                    //rc = send(fds[i].fd, buffer, len , 0);
                    //printf("%d is rc lenght!! cocks !!! \n", rc);
                    printf("%d rc value \n", rc);
                    //TODO: input frá user getur ekki verið stærra en buffer
                    close_conn = ISTRUE;
                    break;

                }
                if(close_conn) {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    compress_array = ISTRUE;
                }
            }
        }

        if(compress_array) {
            compress_array = ISFALSE;
            for(int i = 0; i < nfds; i++) {
                if(fds[i].fd == -1) {
                    for(int j = i; j < nfds; j++) {
                        client_array[j] = client_array[j+1];
                        fds[j].fd = fds[j+1].fd;
                    }
                    nfds--;
                }
            }
        }

    }
    for(int i = 0; i < nfds; i++) {
        if(fds[i].fd >= 0) {
            close(fds[i].fd);
        }
    }
}

void start_server(int *sockfd, struct sockaddr_in *server) {
    int rc, on = 1;
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(*sockfd < 0) {
        perror("socket failed");
        exit(-1);
    }

    rc = setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR,(char*)&on, sizeof(on));
    if(rc < 0) {
        perror("setsockopt failed");
        close(*sockfd);
        exit(-1);
    }

    rc = ioctl(*sockfd, FIONBIO, (char*)&on);
    if(rc < 0) {
        perror("ioctl failed");
        close(*sockfd);
        exit(-1);
    }
    memset(server, 0, sizeof(struct sockaddr_in));
    server->sin_family = AF_INET;
    server->sin_addr.s_addr = htonl(INADDR_ANY);
    server->sin_port = htons(32000);
    rc = bind(*sockfd, (struct sockaddr *)server, (socklen_t) sizeof(struct sockaddr_in));
    if(rc < 0) {
        perror("bind failed");
        exit(-1);
    }
    rc = listen(*sockfd, 32);
    if(rc < 0) {
        perror("listen failed");
        exit(-1);
    }
}
/*
* What kind of request did we recieve.
* Calls appropriate function to generate respnse.
*/
void request(char* buffer, int connfd, char* host_ip, char* host_port, char* ip_addr, int* rc) {
    if(buffer[0] == 'G') {
        if(buffer[5] != ' ') {
            unsupported_request(connfd, &rc);
        }
        else {
            get_request(connfd, host_ip, host_port, ip_addr, &rc);
        }
    }
    else if(buffer[0] == 'P') {
        post_request(buffer, connfd, host_ip, host_port, ip_addr, &rc);
    }
    else if(buffer[0] == 'H') {
        head_request(connfd, &rc);
    }
    else {
        unsupported_request(connfd, &rc);
    }
}

//TODO: IMPLEMENT
void post_request(char* in_buffer, int connfd, char* host_ip, char* host_port, char* ip_addr, int** rc) {
    char send_buffer[1024];
    char final_send_buffer[1024];
    memset(send_buffer, 0, sizeof(send_buffer));
    memset(final_send_buffer, 0, sizeof(final_send_buffer));
    send_buffer[0] = '\0';
    final_send_buffer[0] = '\0';
    //In memory html
    char* prequel = "HTTP/1.1 200 OK\n"
                    "Content-type: text/html\n"
                    "content-lenght: ";

    char* prequel2 = "\n"
                     "<html>\n"
                     " <body>\n"
                     "  <p>\n";
    char* sequel = "  </p>\n"
                   " </body>\n"
                   "</html>\n";

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

    fprintf(stdout, "%s\n", final_send_buffer);
    fflush(stdout);

    **rc = send(connfd, final_send_buffer, strlen(final_send_buffer), 0);
}

/*
* Generates response to request that is not GET, POST or HEAD
*/
void unsupported_request(int connfd, int** rc) {
    //In memory html
    char* drasl = "404 Not Found\n"
                  "Content-type: text/html\n"
                  "\n"
                  "<html>\n"
                  " <body>\n"
                  "  <h1>Not Found</h1>\n"
                  " </body>\n"
                  "</html>\n";

    **rc = send(connfd, drasl, strlen(drasl), 0);
}

/*
* Generates response to GET request
*/
void get_request(int connfd, char* host_ip, char* host_port, char* ip_addr, int** rc) {
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
    **rc = send(connfd, send_buffer, strlen(send_buffer), 0);
}

/*
* Generates response to HEAD request.
*/
void head_request(int connfd, int** rc) {
    char head_buffer[1024];
    memset(head_buffer, 0, sizeof(head_buffer));
    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */
    //In memory header
    char* prequel = "HTTP/1.1 200 OK\n"
                    "Connection: close\r\n"
                    "Content-type: text/html \r\n"
                    "Content-length: 0"
                    "Date: ";
    char* sequel = "Location: 127.0.0.1\r\n"
                   "Server: cool server\r\n";
    head_buffer[0] = '\0';

    strcat(head_buffer, prequel);
    strcat(head_buffer, asctime(localtime(&ltime)));
    strcat(head_buffer,"\n");
    strcat(head_buffer, sequel);

    **rc = send(connfd, head_buffer, strlen(head_buffer), 0);
}

/*
* Logs information from client
*/
void client_logger(char* host_ip, char* host_port, char in_buffer[1024], char* ip_addr) {
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

    if(in_buffer[0] == 'G') {
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
    else {
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