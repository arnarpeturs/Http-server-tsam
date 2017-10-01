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

void get_request(int connfd, char* host_ip, char* host_port, char* ip_addr);
void unsupported_request(int connfd);
void client_logger(char* host_ip, char* host_port, char* in_buffer);

int main()
{
    int sockfd;
    struct sockaddr_in server, client;
    char buffer[1024];

    // Create and bind a TCP socket.
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // Network functions need arguments in network byte order instead of
    // host byte order. The macros htonl, htons convert the values.
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(6969);
    bind(sockfd, (struct sockaddr *) &server, (socklen_t) sizeof(server));

    // Before the server can accept buffers, it has to listen to the
    // welcome port. A backlog of one connection is allowed.
    listen(sockfd, 13);
    for (;;) {
        // We first have to accept a TCP connection, connfd is a fresh
        // handle dedicated to this connection.
        //client ip address
        socklen_t len = (socklen_t) sizeof(client);
        int connfd = accept(sockfd, (struct sockaddr *) &client, &len);
        
        // get info for the get request
        struct sockaddr_in* cock_address = (struct sockaddr_in*)&client;
        struct in_addr client_ip = cock_address->sin_addr;
        char host_ip[1025];
        char host_port[32];
        getnameinfo((struct sockaddr *)&client,len, host_ip, sizeof(host_ip), host_port, sizeof(host_port), NI_NUMERICHOST | NI_NUMERICSERV);
        char ip_addr[INET_ADDRSTRLEN];  
        inet_ntop(AF_INET, &client_ip, ip_addr, INET_ADDRSTRLEN);
        
        // Receive from connfd, not sockfd.
        ssize_t n = recv(connfd, buffer, sizeof(buffer) - 1, 0);
        client_logger(host_ip, host_port, buffer);
        if(buffer[0] == 'G'){
            get_request(connfd, host_ip, host_port, ip_addr);   
        }
        else if(buffer[0] == 'P'){

        }
        else if(buffer[0] == 'H'){

        }
        else{
            unsupported_request(connfd);
        }

        //buffer[n] = '\0';
       	fprintf(stdout, "Received:\n%s\n", buffer);
       	fflush(stdout);
       	break;
    }
}

void unsupported_request(int connfd){
    char* drasl = "HTTP/1.1 404 Not Found\n"
    "Content-type: text/html\n"
    "\n"
    "<html>\n"
    " <body>\n"
    "  <h1>Not Found</h1>\n"
    "  <p>Luger pistol</p>\n"
    " </body>\n"
    "</html>\n";

    send(connfd, drasl, strlen(drasl), 0);
}

void get_request(int connfd, char* host_ip, char* host_port, char* ip_addr) {
    char send_buffer[1024];
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


    send(connfd, send_buffer, strlen(send_buffer), 0);
}
void client_logger(char* host_ip, char* host_port, char* in_buffer){
    char to_file_buffer[1000];
    char* buff_split;
    to_file_buffer[0] = '\0';
    FILE *fptr;

    buff_split = strtok(in_buffer, " ");

    fptr = fopen("client_logger.log", "w");
    if(fptr == NULL)
    {
      printf("Error!");
      exit(1);
    }
    // timestamp : <client ip>:<client port> <request method> <requested URL> : <response code> 
    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */

    strcat(to_file_buffer, asctime(localtime(&ltime)));
    strcat(to_file_buffer, " : ");
    strcat(to_file_buffer, host_ip);
    strcat(to_file_buffer, ":");
    strcat(to_file_buffer, host_port);
    strcat(to_file_buffer, " ");
    strcat(to_file_buffer, buff_split[0]);
    strcat(to_file_buffer, " ");
    strcat(to_file_buffer, buff_split[1] + " : <responsecode>");

    fprintf(fptr,"%s", to_file_buffer);
    fclose(fptr);
}