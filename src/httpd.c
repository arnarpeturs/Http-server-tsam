/* A TCP echo server.
 *
 * Receive a buffer on port 32000, turn it into upper case and return
 * it to the sender.
 *
 * Copyright (c) 2016, Marcel Kyas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Reykjavik University nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MARCEL
 * KYAS NOR REYKJAVIK UNIVERSITY BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>

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
        
    	char ip_addr[INET_ADDRSTRLEN];
    	char* drasl2 = inet_ntoa(client.sin_addr);
//        ssize_t n = recv(connfd, buffer, sizeof(buffer) - 1, 0);
    	socklen_t len = (socklen_t) sizeof(client);

        int connfd = accept(sockfd, (struct sockaddr *) &client, &len);

        //send(connfd, drasl, strlen(drasl), 0);

    	inet_ntop(AF_INET, &drasl2, ip_addr, INET_ADDRSTRLEN);
        

    	fprintf(stdout, "%s\n", ip_addr);
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
