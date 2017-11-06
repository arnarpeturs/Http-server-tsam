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
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

#define NO_FD -1
#define ISTRUE 1 
#define ISFALSE 0
#define MAX_FDS 200
#define KEEP_ALIVE_TIME 30
#define MAX_PORT 65535
#define MIN_PORT 1024
#define CERTIFICATE "../fd.crt"
#define PRIVATE_KEY "../fd.key"

//static SSL *server_ssl;
static SSL_CTX *ssl_ctx; 

struct clients
{
    struct sockaddr_in client;
    char ip[15];
    char port[10];
    time_t timer;
    char background_color[10];
    GHashTable* headers;
    GHashTable* queries;
};
typedef struct pollfd pollfd;

void debug(char* buffer){
    fprintf(stdout, "%s\n", buffer);
    fflush(stdout);
}

void init_SSL();
void start_server(int *sockfd, struct sockaddr_in *server, unsigned short port_number);
void post_request(char* in_buffer, int connfd, struct clients* client_array, int index);
void get_request(int connfd, struct clients* client_array, int index);
void not_implemented(int connfd, struct clients* client_array, int index);
void four_oh_four(int connfd, struct clients* client_array, int index);
void client_logger(char in_buffer[1024], struct clients* client_array, int index);
void request(char* buffer, int connfd, struct clients* client_array, int index);
int check_time_outs(pollfd* fds, struct clients* client_array, int number_of_clients);
void for_each_func(gpointer key, gpointer val, gpointer data);
void client_header_parser(int index, char* buffer, struct clients* client_array);
void color_page(int connfd, struct clients* client_array, int index);
unsigned short get_port(const char* port);
int is_numeric(const char* port, size_t len);
void add_queries_to_html(gpointer key, gpointer val, gpointer data);
void test_page(int connfd, struct clients* client_array, int index);
int fd_finder(pollfd* fds);
char* get_keep_alive(struct clients* client_array, int index);

int main(int argc, char** argv)
{
    if(argc != 2){
        perror("Invalid arguments");
        exit(-1);
    }
    
    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();
    unsigned short port_number = get_port(argv[1]);
    int sockfd = 0, rc;
    int run_server = 1;
    int nfds = 1;
    struct sockaddr_in server;
    struct clients client_array[MAX_FDS];
    char buffer[1024];
    pollfd fds[MAX_FDS];

    start_server(&sockfd, &server, port_number);
   // init_SSL();
    
    memset(fds, 0, sizeof(fds));
    for (int i = 1; i < MAX_FDS; i++) fds[i].fd = -1;
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    //server loop
    while(run_server == ISTRUE){
        memset(buffer, 0, sizeof(buffer));

        rc = poll(fds, MAX_FDS, KEEP_ALIVE_TIME);
        
        if(rc < 0){ exit (-1);}
        if (rc != 0) {
            debug("cockmaster");
            for(int i = 0; i < MAX_FDS; i++){
                if(fds[i].fd == -1 || !(fds[i].revents & POLLIN)){
                    continue;
                }
                if(i == 0){
                    debug("new client");
                    nfds = fd_finder(fds);
                    socklen_t len = (socklen_t)sizeof(struct sockaddr_in);
                    memset(client_array[nfds].ip, 0, sizeof(client_array[nfds].ip));
                    memset(client_array[nfds].port, 0, sizeof(client_array[nfds].port));
                    memset(client_array[nfds].background_color, 0, sizeof(client_array[nfds].background_color));
                    fds[nfds].fd = accept(sockfd, (struct sockaddr*)&client_array[nfds].client, &len);
                    fds[nfds].events = POLLIN;
                    //client_array[nfds].headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
                    //client_array[nfds].queries = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
                    client_array[nfds].timer = time(NULL);

                    //get ip address and port from client
                    getnameinfo((struct sockaddr *)&client_array[nfds].client, len, client_array[nfds].ip, sizeof(client_array[nfds].ip), 
                    client_array[nfds].port, sizeof(client_array[nfds].port), NI_NUMERICHOST | NI_NUMERICSERV);
                    client_array[nfds].headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
                    client_array[nfds].queries = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

                    debug("New Client added");
                    //nfds++;
                }
                else {
                    rc = recv(fds[i].fd, buffer, sizeof(buffer) -1, 0);
                    debug("recieved shit from client");

                    if(rc == 0){
                        debug("CLOSING CLIENT");
                        close(fds[i].fd);
                        fds[i].fd = -1;
                        g_hash_table_destroy(client_array[i].queries);
                        g_hash_table_destroy(client_array[i].headers);
                    } 
                    else {
                        debug("parsing");
                        client_header_parser(i, buffer, client_array);
                        //Reads the request type from buffer and sends appropriate response
                        debug("parsing done");
                        request(buffer, fds[i].fd, client_array, i);
                        client_logger(buffer, client_array, i);
                        //checks for keep-alive
                        if(strstr(buffer, "HTTP/1.0") || strstr(buffer, "Connection: close")){
                            debug("Client closed");
                            close(fds[i].fd);
                            fds[i].fd = -1;
                            g_hash_table_destroy(client_array[i].queries);
                            g_hash_table_destroy(client_array[i].headers);
                        }
                        else{
                            client_array[i].timer = time(NULL);
                            debug("reseting client timer");
                        }
                    }
                }
            }
        }
        //Closing timed out clients
        if (check_time_outs(fds, client_array, MAX_FDS) == 1) {
            debug("Closing timed out clients"); 
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
    rc = listen(*sockfd, 200);
    if(rc < 0){
        perror("listen failed");
        exit(-1);
    }
}

int fd_finder(pollfd* fds){
    for(unsigned int i = 1; i < MAX_FDS; ++i){
        if(fds[i].fd == -1){
            fprintf(stdout, "FD found: %i\n", i);
            fflush(stdout);
            return i;
        }
    }
    return -1;
}
/*
* What kind of request did we recieve. 
* Calls appropriate function to generate response.
*/
void request(char* buffer, int connfd, struct clients* client_array, int index){
    debug(buffer);
    //TODO: Breyta unsupported
    char** split_buffer = g_strsplit(buffer, " ", 2);
    if(g_strcmp0(split_buffer[0], "GET") == 0 || g_strcmp0(split_buffer[0], "HEAD") == 0){
        if(g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Url"), "/color") == 0){
            color_page(connfd, client_array, index);
        }
        else if(g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Url"), "/test") == 0){
            test_page(connfd, client_array, index);
        }
        else if((buffer[5] != ' ' && g_strcmp0(split_buffer[0], "GET") == 0)
         || (buffer[6] != ' ' && g_strcmp0(split_buffer[0], "HEAD") == 0)){
            four_oh_four(connfd, client_array, index);
        }
        else {
            get_request(connfd, client_array, index);   
        }
    }
    else if(g_strcmp0(split_buffer[0], "POST") == 0){
        post_request(buffer, connfd, client_array, index);
    }
    else {
        not_implemented(connfd, client_array, index);
    }
    g_strfreev(split_buffer);
}

void post_request(char* in_buffer, int connfd, struct clients* client_array, int index){
    char send_buffer[1024];
    char final_send_buffer[1024];
    memset(send_buffer, 0, sizeof(send_buffer));
    memset(final_send_buffer, 0, sizeof(final_send_buffer));

    time_t ltime; 
    ltime=time(NULL); 

    char** split_buffer = g_strsplit(in_buffer, "\r\n\r\n", 0);
    strcat(send_buffer, g_hash_table_lookup(client_array[index].headers, "Version"));
    strcat(send_buffer, " 200 OK");
    strcat(send_buffer, "\r\nDate: ");
    strcat(send_buffer, asctime(localtime(&ltime)));
    strcat(send_buffer, get_keep_alive(client_array, index));
    strcat(send_buffer,"Content-type: text/html\r\nContent-Length: ");
    char tmp_buffer[1024];
    memset(tmp_buffer, 0, sizeof(tmp_buffer));
    strcat(tmp_buffer, "<!DOCTYPE html><html><head><title>WebSite</title></head><body");
    strcat(tmp_buffer, "><p>");
    strcat(tmp_buffer, "http://");
    strcat(tmp_buffer, g_hash_table_lookup(client_array[index].headers, "Host"));
    strcat(tmp_buffer, g_hash_table_lookup(client_array[index].headers, "Url"));
    strcat(tmp_buffer, " ");
    strcat(tmp_buffer, client_array[index].ip);
    strcat(tmp_buffer, ":");
    strcat(tmp_buffer, client_array[index].port);
    strcat(tmp_buffer, "</p>");
    strcat(tmp_buffer, "<form method=\"post\">POST DATA: <input type=\"text\" name=\"pdata\"><input type=\"submit\" value=\"Submit\">"); 
    if(g_str_has_prefix(split_buffer[1], "pdata=")== TRUE) {
        char** splitty = g_strsplit(split_buffer[1], "pdata=", 0);
        
        strcat(tmp_buffer, splitty[1]);
        g_strfreev(splitty);
    }
    else{
        strcat(tmp_buffer, split_buffer[1]);    
    }
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

void not_implemented(int connfd, struct clients* client_array, int index){

    char send_buffer[1024];
    memset(send_buffer, 0, sizeof(send_buffer));

    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */

    strcat(send_buffer, g_hash_table_lookup(client_array[index].headers, "Version"));
    strcat(send_buffer, " ");
    strcat(send_buffer, "501 Not Implemented");
    strcat(send_buffer, "\r\nDate: ");
    strcat(send_buffer, asctime(localtime(&ltime)));
    strcat(send_buffer, get_keep_alive(client_array, index));
    strcat(send_buffer,"Content-type: text/html\r\nContent-Length: ");
    
    char tmp_buffer[1024];
    memset(tmp_buffer, 0, sizeof(tmp_buffer));
    strcat(tmp_buffer, "<!DOCTYPE html><html><head><title>WebSite</title></head><body>");
    strcat(tmp_buffer, "<h1>501 Not Implemented</h1>");
    strcat(tmp_buffer, "</body></html>");   

    char len_buf[10];
    memset(len_buf, 0, sizeof(len_buf));
    sprintf(len_buf, "%zu", strlen(tmp_buffer));
    strcat(send_buffer, len_buf);
    strcat(send_buffer, "\r\n\r\n");
    strcat(send_buffer, tmp_buffer);
    send(connfd, send_buffer, strlen(send_buffer), 0);
}

void four_oh_four(int connfd, struct clients* client_array, int index) {

    char send_buffer[1024];
    memset(send_buffer, 0, sizeof(send_buffer));

    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */

    strcat(send_buffer, g_hash_table_lookup(client_array[index].headers, "Version"));
    strcat(send_buffer, " ");
    strcat(send_buffer, "404 Not Found");
    strcat(send_buffer, "\r\nDate: ");
    strcat(send_buffer, asctime(localtime(&ltime)));
    strcat(send_buffer, get_keep_alive(client_array, index));
    strcat(send_buffer,"Content-type: text/html\r\nContent-Length: ");
    
    char tmp_buffer[1024];
    memset(tmp_buffer, 0, sizeof(tmp_buffer));
    strcat(tmp_buffer, "<!DOCTYPE html><html><head><title>WebSite</title></head><body>");
    strcat(tmp_buffer, "<h1>404 Not Found</h1>");
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
* Generates response to GET request
*/
void get_request(int connfd, struct clients* client_array, int index) {

    char send_buffer[1024];
    memset(send_buffer, 0, sizeof(send_buffer));

    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */

    strcat(send_buffer, g_hash_table_lookup(client_array[index].headers, "Version"));
    strcat(send_buffer, " 200 OK");
    strcat(send_buffer, "\r\nDate: ");
    strcat(send_buffer, asctime(localtime(&ltime)));
    strcat(send_buffer, get_keep_alive(client_array, index));

    strcat(send_buffer,"Content-type: text/html\r\nContent-Length: ");
    
    char tmp_buffer[1024];
    memset(tmp_buffer, 0, sizeof(tmp_buffer));
    strcat(tmp_buffer, "<!DOCTYPE html><html><head><title>WebSite</title></head><body");

    strcat(tmp_buffer, "><p>");
    strcat(tmp_buffer, "http://");
    strcat(tmp_buffer, g_hash_table_lookup(client_array[index].headers, "Host"));
    strcat(tmp_buffer, g_hash_table_lookup(client_array[index].headers, "Url"));
    strcat(tmp_buffer, " ");
    strcat(tmp_buffer, client_array[index].ip);
    strcat(tmp_buffer, ":");
    strcat(tmp_buffer, client_array[index].port);
    strcat(tmp_buffer, "</p>");
    strcat(tmp_buffer, "<form method=\"post\">POST DATA: <input type=\"text\" name=\"pdata\"><input type=\"submit\" value=\"Submit\">");
    strcat(tmp_buffer, "</body></html>");   

    char len_buf[10];
    memset(len_buf, 0, sizeof(len_buf));
    sprintf(len_buf, "%zu", strlen(tmp_buffer));
    strcat(send_buffer, len_buf);
    strcat(send_buffer, "\r\n\r\n");
    if(g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Method"), "GET") == 0){
        strcat(send_buffer, tmp_buffer);
    }
    send(connfd, send_buffer, strlen(send_buffer), 0);
}

/*
* Logs information from client
*/
void client_logger(char in_buffer[1024], struct clients* client_array, int index){
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
        response_code = "501 Not implemented";
    }
    
    strcat(to_file_buffer, asctime(localtime(&ltime)));
    strcat(to_file_buffer, " : ");
    strcat(to_file_buffer, client_array[index].ip);
    strcat(to_file_buffer, ":");
    strcat(to_file_buffer, client_array[index].port);
    strcat(to_file_buffer, " ");
    strcat(to_file_buffer, request_method);
    strcat(to_file_buffer, " ");
    strcat(to_file_buffer, "http://");
    strcat(to_file_buffer, g_hash_table_lookup(client_array[index].headers, "Host"));
    strcat(to_file_buffer, g_hash_table_lookup(client_array[index].headers, "Url"));
    strcat(to_file_buffer, " : ");
    strcat(to_file_buffer, response_code);

    fprintf(fptr,"\r\n%s", to_file_buffer);
    fclose(fptr);
}
/*
* If a client closes connection it minimizes the client and fds arrays to the number of clients connected
*/

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
                g_hash_table_destroy(client_array[i].queries);
                g_hash_table_destroy(client_array[i].headers);
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
    
    int ind = 1;
    int query_index = 0;

    char** first_split = g_strsplit(split_buffer[0], " ", 0);
    char** url_split = g_strsplit(first_split[1], "?", 0);
    //g_hash_table_foreach(client_array[index].headers, for_each_func, NULL);
    g_hash_table_insert(client_array[index].headers, g_strdup("Method"), g_strdup(first_split[0]));
    g_hash_table_insert(client_array[index].headers, g_strdup("Url"), g_strdup(url_split[0]));
   
    if(g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Url"), "/test") == 0 ||
        g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Url"), "/color") == 0){
        if (url_split[1] != NULL){
            g_hash_table_insert(client_array[index].headers, g_strdup("Query"), g_strdup(url_split[1]));
            char** query_split = g_strsplit(url_split[1], "&", 0);
            while(query_split[query_index] != NULL){
                char** sub_split = g_strsplit(query_split[query_index], "=", 0);
                if(sub_split[1] != NULL){
                    if(strstr(sub_split[1], "=")){
                        query_index++;
                        g_strfreev(sub_split);
                        continue;
                    }
                }
                if(sub_split[1] == NULL){
                    query_index++;
                    g_strfreev(sub_split);
                    continue;
                }
                g_hash_table_insert(client_array[index].queries, g_strdup(sub_split[0]), 
                                    g_strdup(sub_split[1]));
                g_strfreev(sub_split);
                query_index++;
            }
            g_strfreev(query_split);
        }
    }
    
    g_hash_table_insert(client_array[index].headers, g_strdup("Version"), g_strdup(first_split[2]));

    g_strfreev(first_split);
    g_strfreev(url_split);

    while(split_buffer[ind] != NULL){
        if(g_strcmp0(split_buffer[ind], "") == 0){
            if(g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Method"), "POST") == 0){
                g_hash_table_insert(client_array[index].headers, g_strdup("Data"), g_strdup(split_buffer[ind+1]));
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
        g_hash_table_insert(client_array[index].headers,g_strdup("Keep-alive"), g_strdup("timeout=30"));
    }
    if(g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Connection"), "close") == 0){        
        g_hash_table_insert(client_array[index].headers,g_strdup("close"), g_strdup("timeout=0"));
    }

    char* bg_color = g_hash_table_lookup(client_array[index].queries, "bg");
    if(g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Url"), "/test") != 0 
        && bg_color != NULL){
            strcpy(client_array[index].background_color, bg_color); 
        
    }
    g_strfreev(split_buffer);
    debug("new_headers");
    g_hash_table_foreach(client_array[index].headers, for_each_func, NULL);

}

void for_each_func(gpointer key, gpointer val, gpointer data)
{
    printf("%s -> %s\n", (char*)key, (char*)val);
}

void add_queries_to_html(gpointer key, gpointer val, gpointer data){
    char* str = (char*)data;
    strcat(str, "<p>");
    strcat(str, (char*)key);
    strcat(str, " --> ");
    strcat(str, (char*)val);
    strcat(str, "</p>");
}

void color_page(int connfd, struct clients* client_array, int index) {
    char send_buffer[1024];
    memset(send_buffer, 0, sizeof(send_buffer));

    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */

    strcpy(send_buffer, "HTTP/1.1 200 OK\r\nDate: ");
    strcat(send_buffer, asctime(localtime(&ltime)));
    strcat(send_buffer, get_keep_alive(client_array, index));
    if(client_array[index].background_color[0] != '\0'){
        time_t cookietime = time(NULL) + 3600; 
        strcat(send_buffer, "Set-Cookie: bg=");
        strcat(send_buffer, client_array[index].background_color);
        
        //strcat(send_buffer, "; expires=");
        //strcat(send_buffer, asctime(localtime(&cookietime)));
        //strcat(send_buffer, "; path=");
        //strcat(send_buffer, g_hash_table_lookup(client_array[index].headers, "Url"));
        //strcat(send_buffer, "; domain=app.localhost:6969; secure\r\n");
        
        strcat(send_buffer, "\r\n");
    }

    strcat(send_buffer, "Content-type: text/html\r\nContent-Length: ");

    char tmp_buffer[1024];
    memset(tmp_buffer, 0, sizeof(tmp_buffer));
    strcat(tmp_buffer, "<!DOCTYPE html><html><head><title>WebSite</title></head><body");
    strcat(tmp_buffer, " style=\"background-color:");


    char* cookie = g_hash_table_lookup(client_array[index].headers, "Cookie");
    int cookie_index = 0;
    if(cookie != NULL){
        if(client_array[index].background_color[0] != '\0'){
            strcat(tmp_buffer, client_array[index].background_color);
        }
        else{
            char** cookie_split = g_strsplit(cookie, "; ", 0);
            if(cookie_split != NULL){
                while(cookie_split[cookie_index] != NULL){
                    char** sub_split = g_strsplit(cookie_split[cookie_index], "=", 0);
                    debug("sub_split");
                    debug(sub_split[0]);
                    debug(sub_split[1]);
                    if(sub_split[0] == NULL){
                        g_strfreev(sub_split);
                        break;
                    }
                    else if(g_strcmp0(sub_split[0], "bg") == 0){
                        strcat(tmp_buffer, sub_split[1]);
                        g_strfreev(sub_split);
                        break;
                    }
                    debug("sub_split");
                    debug(sub_split[0]);
                    if(sub_split[1] != NULL){
                        if(strstr(sub_split[1], "=")){
                            cookie_index++;
                            g_strfreev(sub_split);
                            continue;
                        }
                    }
                    if(sub_split[1] == NULL){
                        cookie_index++;
                        g_strfreev(sub_split);
                        continue;
                    }
                    cookie_index++;
                }
            }
            strcat(tmp_buffer, client_array[index].background_color);
            g_strfreev(cookie_split);
        }
    }
    else{
        strcat(tmp_buffer, client_array[index].background_color);
    }
    
    strcat(tmp_buffer, "\">");
    strcat(tmp_buffer, "</body></html>");   

    char len_buf[10];
    memset(len_buf, 0, sizeof(len_buf));
    sprintf(len_buf, "%zu", strlen(tmp_buffer));
    strcat(send_buffer, len_buf);
    strcat(send_buffer, "\r\n\r\n");
    if(g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Method"), "GET") == 0){
        strcat(send_buffer, tmp_buffer);
    }
    send(connfd, send_buffer, strlen(send_buffer), 0);
}

unsigned short get_port(const char* port){
    size_t len = 0;
    if((len = strlen(port)) > 5 || !is_numeric(port, len)){
        exit(1);
    }
    int temp_port = atoi(port);
    if(temp_port <= MIN_PORT || temp_port > MAX_PORT){
        printf("%s\n", "ILLEGAL PORT_NUMBER");
        exit(1);
    }
    return (unsigned short)temp_port;
}
int is_numeric(const char* port, size_t len){
    for(unsigned int i = 0; i < len; i++){
        if(port[i] > 57 || port[i] < 48){
            return 0;
        }
    }
    return 1;
}

void test_page(int connfd, struct clients* client_array, int index) {
    char send_buffer[1024];
    memset(send_buffer, 0, sizeof(send_buffer));

    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */

    strcpy(send_buffer, "HTTP/1.1 200 OK\r\nDate: ");
    strcat(send_buffer, asctime(localtime(&ltime)));
    strcat(send_buffer, get_keep_alive(client_array, index));
    strcat(send_buffer,"Content-type: text/html\r\nContent-Length: ");
    
    char tmp_buffer[1024];
    memset(tmp_buffer, 0, sizeof(tmp_buffer));
    strcat(tmp_buffer, "<!DOCTYPE html><html><head><title>WebSite</title></head><body>");

    g_hash_table_foreach(client_array[index].queries, add_queries_to_html, tmp_buffer);
 
    strcat(tmp_buffer, "</body></html>");   

    char len_buf[10];
    memset(len_buf, 0, sizeof(len_buf));
    sprintf(len_buf, "%zu", strlen(tmp_buffer));
    strcat(send_buffer, len_buf);
    strcat(send_buffer, "\r\n\r\n");
    if(g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Method"), "GET") == 0){
        strcat(send_buffer, tmp_buffer);
    }
    send(connfd, send_buffer, strlen(send_buffer), 0);
}

void init_SSL()
{
    // Internal SSL init functions
    SSL_library_init();
    SSL_load_error_strings();

    // Authentication
    if ((ssl_ctx = SSL_CTX_new(TLSv1_method())) == NULL){
        perror("SSL CTX");
        exit(EXIT_FAILURE);
    } 
    if (SSL_CTX_use_certificate_file(ssl_ctx, CERTIFICATE, SSL_FILETYPE_PEM) <= 0){
        perror("certificate");
        exit(EXIT_FAILURE);
    }
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, PRIVATE_KEY, SSL_FILETYPE_PEM) <= 0){
        perror("privatekey");
        exit(EXIT_FAILURE);
    }
    if (SSL_CTX_check_private_key(ssl_ctx) != 1){
        perror("match");
        exit(EXIT_FAILURE);  
    } 
}
char* get_keep_alive(struct clients* client_array, int index){
    if(g_hash_table_contains(client_array[index].headers, "Connection")){
        if(g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Connection"), "keep-alive") == 0){
            return "Connection: keep-alive\r\n";
        }   
        else{
            return"Connection: close\r\n";
        }
    }
    else{
        if(g_strcmp0(g_hash_table_lookup(client_array[index].headers, "Method"), "HTTP/1.1") == 0){
            return "Connection: keep-alive\r\n";
        }   
        else{
            return "Connection: close\r\n";
        }        
    }
}