#include <sys/socket.h>
#include <netinet/in.h>
int set_socket_to_nonblock(int fd);
int set_socket_to_block(int fd);
void print_socket_block_mode(int fd);
int socket_is_nonblock(int fd);
int timeout_connect(int sockfd, struct sockaddr_in * serv_addr, int timeout);
int wait_connect(int sockfd, int timeout);
