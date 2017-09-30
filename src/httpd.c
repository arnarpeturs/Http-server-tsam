

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>


void unsupported_request(int connfd);

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
        
        fprintf(stdout, "%s\n", ip_addr);
        fflush(stdout);
        fprintf(stdout, "%s %s\n", hostIP, hostPort);
        fflush(stdout);
        
        // Receive from connfd, not sockfd.
        ssize_t n = recv(connfd, buffer, sizeof(buffer) - 1, 0);
        if(buffer[0] == 'G'){

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
//TODO:: taka in client og len connfd
void get_request() {
    struct sockaddr_in* cock_address = (struct sockaddr_in*)&client;
        struct in_addr client_ip = cock_address->sin_addr;

        char hostIP[1025];
        char hostPort[32];
        getnameinfo((struct  sockaddr *)&client,len, hostIP, sizeof(hostIP), hostPort, sizeof(hostPort), NI_NUMERICHOST | NI_NUMERICSERV);
        char ip_addr[INET_ADDRSTRLEN];  
        inet_ntop(AF_INET, &client_ip, ip_addr, INET_ADDRSTRLEN);
       
}