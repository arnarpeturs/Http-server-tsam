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
#define KEEP_ALIVE_TIME 30
#define PORT_NUMBER 32000

struct clients
{
    struct sockaddr_in client;
    char ip[15];
    char port[10];
    time_t timer;
    GHashTable* headers;
};
typedef struct pollfd pollfd;

void debug(char* buffer){
    fprintf(stdout, "%s\n", buffer);
    fflush(stdout);
}

void start_server(int *sockfd, struct sockaddr_in *server, unsigned short port_number);
void post_request(char* in_buffer, int connfd, char* host_ip, char* host_port, char* ip_addr);
void get_request(int connfd, char* host_ip, char* host_port, char* ip_addr, struct clients* client_array, int temp_index);
void unsupported_request(int connfd);
void not_implemented(int connfd);
void client_logger(char* host_ip, char* host_port, char in_buffer[1024], char* ip_addr);
void head_request(int connfd);
void request(char* buffer, int connfd, char* host_ip, char* host_port, char* ip_addr, struct clients* client_array, int temp_index);
int check_time_outs(pollfd* fds, struct clients* client_array, int number_of_clients);
void compressor(pollfd* fds, struct clients* client_array, int* nfds);
void for_each_func(gpointer key, gpointer val, gpointer data);
void client_header_parser(int index, char* buffer, struct clients* client_array);


int main(int argc, char** argv)
{
    unsigned short port_number = atoi(argv[1]);
    int sockfd = 0, rc;
    int run_server = 1;
    int nfds = 1;
    int compress_array = 0;
    struct sockaddr_in server;
    struct clients client_array[MAX_FDS];
    char buffer[1024];
    pollfd fds[MAX_FDS];
    char server_ip[15];
    memset(server_ip, 0, sizeof(server_ip));

    start_server(&sockfd, &server, port_number);

    inet_ntop(AF_INET, &server.sin_addr, server_ip, INET_ADDRSTRLEN);
    
    memset(fds, 0, sizeof(fds));
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    //server loop
    while(run_server == ISTRUE){
        memset(buffer, 0, sizeof(buffer));

        rc = poll(fds, MAX_FDS, KEEP_ALIVE_TIME);

        if(rc < 0){ exit (-1);}
        if (rc != 0) {
            for(int i = 0; i < MAX_FDS; i++){
                if(fds[i].fd == -1 || !(fds[i].revents & POLLIN)){
                    continue;
                }
                if(i == 0){
                    socklen_t len = (socklen_t)sizeof(struct sockaddr_in);
                    memset(client_array[nfds].ip, 0, sizeof(client_array[nfds].ip));
                    memset(client_array[nfds].port, 0, sizeof(client_array[nfds].port));

                    fds[nfds].fd = accept(sockfd, (struct sockaddr*)&client_array[nfds].client, &len);
                    fds[nfds].events = POLLIN;

                    client_array[nfds].timer = time(NULL);

                    //get ip address and port from client
                    getnameinfo((struct sockaddr *)&client_array[nfds].client, len, client_array[nfds].ip, sizeof(client_array[nfds].ip), 
                    client_array[nfds].port, sizeof(client_array[nfds].port), NI_NUMERICHOST | NI_NUMERICSERV);
        
                    char ip_addr[INET_ADDRSTRLEN];
                    memset(ip_addr, 0, sizeof(ip_addr));

                    inet_ntop(AF_INET, &client_array[nfds].client.sin_addr, ip_addr, INET_ADDRSTRLEN);
                    nfds++;

                }
                else {
                    rc = recv(fds[i].fd, buffer, sizeof(buffer) -1, 0);

                    if(rc == 0){
                        close(fds[i].fd);
                        fds[i].fd = -1;
                        compress_array = ISTRUE;
                    } 
                    else {
                        client_header_parser(i, buffer, client_array);
                        //Reads the request type from buffer and sends appropriate response
                        request(buffer, fds[i].fd, client_array[i].ip, client_array[i].port, server_ip, client_array, i);
                        client_logger(client_array[i].ip, client_array[i].port, buffer, server_ip);
                        //checks for keep-alive
                        if(strstr(buffer, "HTTP/1.0") || strstr(buffer, "Connection: close")){
                            close(fds[i].fd);
                            fds[i].fd = -1;
                            compress_array = ISTRUE;
                        }
                        else{
                            client_array[i].timer = time(NULL);   
                        }
                    }
                }
            }
        }
        //If client has timed out or closed connection we set compress_array to TRUE and call the compressor function
        if (check_time_outs(fds, client_array, nfds)) compress_array = ISTRUE;
        if(compress_array){
            compress_array = ISFALSE;
            compressor(fds,client_array, &nfds);
        }
    }
}

void start_server(int *sockfd, struct sockaddr_in *server, unsigned short port_number){
    int rc;
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(*sockfd < 0){
        perror("socket failed");
        exit(-1);
    }
    memset(server, 0, sizeof(struct sockaddr_in));
    server->sin_family = AF_INET;
    server->sin_addr.s_addr = htonl(INADDR_ANY);
    server->sin_port = htons(port_number);
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
* Calls appropriate function to generate response.
*/
void request(char* buffer, int connfd, char* host_ip, char* host_port, char* ip_addr, struct clients* client_array, int temp_index){
    
    //TODO: Breyta unsupported
    char** split_buffer = g_strsplit(buffer, " ", 2);
    if(g_strcmp0(split_buffer[0], "GET") == 0){
        if(buffer[5] != ' '){
            unsupported_request(connfd);
        }
        else {
            get_request(connfd, host_ip, host_port, ip_addr, client_array, temp_index);   
        }    
    }
    else if(g_strcmp0(split_buffer[0], "HEAD") == 0){
        head_request(connfd);
    }
    else if(g_strcmp0(split_buffer[0], "POST") == 0){
        post_request(buffer, connfd, host_ip, host_port, ip_addr);
    }
    else if(g_strcmp0(split_buffer[0], "PUT") == 0){
        unsupported_request(connfd);
    }
    else if(g_strcmp0(split_buffer[0], "DELETE") == 0){
        unsupported_request(connfd);
    }
    else if(g_strcmp0(split_buffer[0], "CONNECT") == 0) {
        unsupported_request(connfd);
    }
    else if(g_strcmp0(split_buffer[0], "OPTIONS") == 0){
        unsupported_request(connfd);
    }
    else if(g_strcmp0(split_buffer[0], "TRACE") == 0){
        unsupported_request(connfd);
    }
    else {
        unsupported_request(connfd);
    }
    g_strfreev(split_buffer);
    /*
    if(buffer[0] == 'G' && buffer[1] == 'E' && buffer[2] == 'T'){
        if(buffer[5] != ' '){
            unsupported_request(connfd);
        }
        else {
            get_request(connfd, host_ip, host_port, ip_addr);      
        }     
    }
    else if(buffer[0] == 'P' && buffer[1] == 'O' && buffer[2] == 'S' && buffer[3] == 'T'){
        post_request(buffer, connfd, host_ip, host_port, ip_addr);
    }
    else if(buffer[0] == 'H' && buffer[1] == 'E' && buffer[2] == 'A' && buffer[3] == 'D'){
        head_request(connfd);
    }
    else{
        unsupported_request(connfd);
    }*/
}

void post_request(char* in_buffer, int connfd, char* host_ip, char* host_port, char* ip_addr){
    char send_buffer[1024];
    char final_send_buffer[1024];
    memset(send_buffer, 0, sizeof(send_buffer));
    memset(final_send_buffer, 0, sizeof(final_send_buffer));

    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */

    char** split_buffer = g_strsplit(in_buffer, "data=", 2);
    strcpy(send_buffer, "HTTP/1.1 200 OK\r\nDate: ");
    strcat(send_buffer, asctime(localtime(&ltime)));
    strcat(send_buffer,"Content-type: text/html\r\nContent-Length: ");



    char tmp_buffer[2000];
    memset(tmp_buffer,0,sizeof(tmp_buffer));
    strcpy(tmp_buffer, "<!DOCTYPE html><html><head><title>WebSite</title></head><body><p>");
    strcat(tmp_buffer, "http://");
    strcat(tmp_buffer, ip_addr);
    strcat(tmp_buffer, "/ ");
    strcat(tmp_buffer, host_ip);
    strcat(tmp_buffer, ":");
    strcat(tmp_buffer, host_port);
    strcat(tmp_buffer, "</p>");
    strcat(tmp_buffer, "<form method=\"post\">POST DATA: <input type=\"text\" name=\"pdata\"><input type=\"submit\" value=\"Submit\">");
    strcat(tmp_buffer, split_buffer[0]);
    strcat(tmp_buffer, "</body></html>");

    char len_buf[10];
    memset(len_buf, 0, sizeof(len_buf));
    sprintf(len_buf, "%zu", strlen(tmp_buffer));

    strcat(send_buffer, len_buf);
    strcat(send_buffer, "\r\n\r\n");
    strcat(send_buffer, tmp_buffer);
    send(connfd, send_buffer, strlen(send_buffer), 0);
    g_strfreev(split_buffer);
}

/*
* Generates response to request that is not GET, POST or HEAD
*/
void unsupported_request(int connfd){
    char* send_buffer = "HTTP/1.1 404 Not Found\r\nContent-type: text/html\r\nContent-Length: 94\r\n\r\n<!DOCTYPE html><html><head><title>404</title></head><body><h1>404 Not Found</h1></body></html>";
    send(connfd, send_buffer, strlen(send_buffer), 0);
}

/*
* Generates response to GET request
*/
void get_request(int connfd, char* host_ip, char* host_port, char* ip_addr, struct clients* client_array, int temp_index) {

    char send_buffer[1024];
    memset(send_buffer, 0, sizeof(send_buffer));

    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */

    strcpy(send_buffer, "HTTP/1.1 200 OK\r\nDate: ");
    strcat(send_buffer, asctime(localtime(&ltime)));
    if(g_strcmp0(g_hash_table_lookup(client_array[temp_index].headers, "Connection"), "keep-alive") == 0){
        strcat(send_buffer, "Connection: keep-alive\r\n");
    }
    else{
        strcat(send_buffer, "Connection: close\r\n");
    }
    strcat(send_buffer,"Content-type: text/html\r\nContent-Length: ");
    
    char tmp_buffer[1024];
    memset(tmp_buffer, 0, sizeof(tmp_buffer));
    strcpy(tmp_buffer, "<!DOCTYPE html><html><head><title>WebSite</title></head><body><p>");
    strcat(tmp_buffer, "http://");
    strcat(tmp_buffer, ip_addr);
    strcat(tmp_buffer, "/ ");
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
void head_request(int connfd){
    char head_buffer[1024];
    memset(head_buffer, 0, sizeof(head_buffer));
    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */

    char* prequel = "HTTP/1.1 200 OK\r\n"    
    "Connection: close\r\n"
    "Content-type: text/html \r\n"
    "Content-length: 0\r\n"
    "Date: "; 
    char* sequel = "Location: 127.0.0.1\r\n"
    "Server: cool server\r\n";

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
    memset(to_file_buffer, 0, sizeof(to_file_buffer));
    FILE *fptr;

    fptr = fopen("client_logger.log", "a");
    if(fptr == NULL)
    {
      printf("Error!");
      exit(1);
    }
    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */

    if(in_buffer[0] == 'G'){
        if(in_buffer[5] != ' '){
            request_method = "ERROR";
            response_code = "404 Not Found";            
        }
        else{
            request_method = "GET";
            response_code = "200 OK";   
        }
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
        request_method = "ERROR";
        response_code = "400 Not Found";
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
    g_strfreev(split_buffer);
}
/*
* If a client closes connection it minimizes the client and fds arrays to the number of clients connected
*/
void compressor(pollfd* fds, struct clients* client_array, int* nfds){
    int tmp = *nfds;
    for(int i = 0; i < tmp; i++){
        if(fds[i].fd == -1){
            for(int j = i; j < tmp; j++){
                memcpy(&client_array[j+1], &client_array[j], sizeof(client_array[j]));
                fds[j].fd = fds[j+1].fd;
            }
            (*nfds)--;
        }
    }
}

/*
* Handles keep-alive
*/
int check_time_outs(pollfd* fds, struct clients* client_array, int number_of_clients)
{
    int someone_timed_out = 0;
    for (int i = 1; i < number_of_clients; i++) {
        if (fds[i].fd != -1) {
            time_t current = time(NULL);
            if (difftime(current, client_array[i].timer) >= KEEP_ALIVE_TIME) {
                someone_timed_out = 1;
                close(fds[i].fd);
                fds[i].fd = -1;
            }
        }
    }
    return someone_timed_out;
}
void client_header_parser(int index, char* buffer, struct clients* client_array){
    char** split_buffer = g_strsplit(buffer, "\r\n", 0);
    client_array[index].headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    int ind = 1;

    char** first_split = g_strsplit(split_buffer[0], " ", 0);
    char** url_split = g_strsplit(first_split[1], "?", 0);

    g_hash_table_insert(client_array[index].headers, "Method", g_strdup(first_split[0]));
    g_hash_table_insert(client_array[index].headers, "Url", g_strdup(url_split[0]));
    if (url_split[1] != NULL)
    {
    	g_hash_table_insert(client_array[index].headers, "Query", g_strdup(url_split[1]));
    }
    g_hash_table_insert(client_array[index].headers, "Version", g_strdup(first_split[2]));

    g_strfreev(first_split);
    g_strfreev(url_split);

    fprintf(stdout, "%s\n", buffer);
    fflush(stdout);

    fprintf(stdout, "%s\n", "i am the fuck the chineese master of cocks");
    fflush(stdout);

    while(split_buffer[ind] != NULL){
        if(g_strcmp0(split_buffer[ind], "") == 0){
        	if(g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Method"), "POST") == 0){
    			g_hash_table_insert(client_array[index].headers, "Data", g_strdup(split_buffer[ind+1]));
        	}
        	break;
        }    	
        char ** temp_split = g_strsplit(split_buffer[ind], ": ", 0);
    	g_hash_table_insert(client_array[index].headers, 
    		g_strdup(temp_split[0]), g_strdup(temp_split[1]));
    	g_strfreev(temp_split);
    	ind++;
    }
    if(g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Connection"), "keep-alive") == 0){        
        g_hash_table_insert(client_array[index].headers,"Keep-alive", "timeout=30");
    }
    if(g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Connection"), "close") == 0){        
        g_hash_table_insert(client_array[index].headers,"close", "timeout=30");
    }
    g_strfreev(split_buffer);
  
	g_hash_table_foreach(client_array[index].headers, for_each_func, NULL);
	//exit(1);
}

void for_each_func(gpointer key, gpointer val, gpointer data)
{
	printf("%s -> %s\n", (char*)key, (char*)val);
}

