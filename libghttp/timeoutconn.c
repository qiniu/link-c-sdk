#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include "timeoutconn.h"
#include <sys/select.h>
//#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

/*
 unsigned int ul = 1;
 ioctl(sockfd, FIONBIO, &ul);
 */
int set_socket_to_nonblock(int fd) {
        int flags;
        if((flags = fcntl(fd, F_GETFL, 0)) < 0) {
                if (flags == -1)
                        return errno;
                else
                        return flags;
        }
        flags |= O_NONBLOCK;
        if(fcntl(fd, F_SETFL, flags) < 0) {
                if (flags == -1)
                        return errno;
                else
                        return flags;
        }
        return 0;
}

/*
 unsigned int ul = 0;
 ioctl(sockfd, FIONBIO, &ul);
 */
int set_socket_to_block(int fd) {
        int flags;
        if((flags = fcntl(fd, F_GETFL, 0)) < 0) {
                if (flags == -1)
                        return errno;
                else
                        return flags;
        }
        flags &= ~O_NONBLOCK;
        if(fcntl(fd, F_SETFL, flags) < 0) {
                if (flags == -1)
                        return errno;
                else
                        return flags;
        }
        return 0;
}

void print_socket_block_mode(int fd) {
        int flags;
        if((flags = fcntl(fd, F_GETFL, 0)) < 0) {
                return;
        }
        if (flags & O_NONBLOCK)
                fprintf(stderr, "===========>socket is nonblock:%02x %02x\n", flags, O_NONBLOCK);
        else
                fprintf(stderr, "===========>socket is block:%02x %02x\n", flags, O_NONBLOCK);
}

int socket_is_nonblock(int fd) {
        int flags;
        if((flags = fcntl(fd, F_GETFL, 0)) < 0) {
                return -1;
        }
        return flags & O_NONBLOCK;
}

int wait_connect(int sockfd, int timeout) {
        
        struct timeval tm;
        fd_set set;
        int ret = 0;
        int len = sizeof(int);
        int error = -1;
        tm.tv_sec = timeout;
        tm.tv_usec = 0;
        FD_ZERO(&set);
        FD_SET(sockfd, &set);
        if((ret = select(sockfd+1, NULL, &set, NULL, &tm)) > 0) {
                getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t*)&len);
                if (error == 0)
                        return 0;
                else
                        return error;
        }
        if (ret == 0)
                return -1;
        return errno;
}

int timeout_connect(int sockfd, struct sockaddr_in * serv_addr, int timeout) {
        
        int ok = 0, ret = 0;
        
        
        if ((ret = set_socket_to_nonblock(sockfd)) != 0)
                return ret;
        
        if(connect(sockfd, (struct sockaddr*)serv_addr, sizeof(struct sockaddr_in)) < 0) {
                
                if (errno != EINPROGRESS) {
                        return errno;
                }
                if ((ret = wait_connect(sockfd, timeout)) == 0) {
                        ok = 1;
                }
        } else
                ok = 1;
        
        if((ret = set_socket_to_block(sockfd)) != 0)
                return ret;
        
        if (!ok) {
                return ret;
        }
        return 0;
}

/*
 int main(int argc, char **argv)
 {
 }
 */
