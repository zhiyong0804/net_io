#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>

#define MAX_BUFFER_LEN 1024

#define handle_error(msg) \
               do { perror(msg); exit(EXIT_FAILURE); } while (0)


static int set_socket_non_block(int fd);
static int nio_write(int fd, char* buf, int len);

static int nio_write(int fd, char* buf, int len)
{
    int write_pos = 0;
    int left_len = len;

    while (left_len > 0)
    {
        int writed_len = 0;
        if ((writed_len = write(fd, buf + write_pos, left_len)) <= 0)
        {  
            if (errno == EAGAIN)
            {
               writed_len = 0;
            }
            else return -1; //表示写失败
        } 
        left_len -= writed_len;
        write_pos += writed_len;
    }
    
    return len;
}

static int set_socket_non_block(int sfd)
{
    int flags, res;

    flags = fcntl(sfd, F_GETFL);
    if (flags == -1)
    {
        perror("error : cannot get socket flags!\n");
        exit(1);
    }

    flags |= O_NONBLOCK;
    res    = fcntl(sfd, F_SETFL, flags);
    if (res == -1)
    {
        perror("error : cannot set socket flags!\n");
        exit(1);
    }

    return 0;
}

void * client_routine(void * argv)
{
    int fd = *(int*)argv;
    char buf[MAX_BUFFER_LEN] = {0};
	printf("thread running with fd=%d.\n", fd);
	
    while (1)
    {
        memset(buf, 0, MAX_BUFFER_LEN);
        int res = read(fd, buf, sizeof(buf));
        if (res < 0 )
        {
            if (errno == EAGAIN)
            {
				usleep(1000*1000);  // sleep 1
			    perror("read returnned with");
            }
            else
            {
                printf("errno=%d, remote client socket is closed.\n", errno);
                break;
            }
        }
		else if (res == 0)
		{
			printf("remote client socket is closed.\n");
			break;
		}
        else
        {
            printf("recved user data : %s\n", buf);
            memset(buf, 0, MAX_BUFFER_LEN);
            strcpy(buf, "Hello Client");
            nio_write(fd, buf, strlen(buf));
            printf("send data : %s\n", buf);
        }
    }
    close(fd);

    return NULL;
}

void* server_routine(void* argv)
{
	int sfd = *(int*)argv;
	int fd;
	struct sockaddr remote;
	
	while (1) 
    {
        int len = sizeof(struct sockaddr);
        fd = accept(sfd, &remote, &len);
        if (fd == -1)
        {
            if (errno == EAGAIN)
            {
                usleep(1000*2);
                printf("waiting for client connection.\n");
            }
        }
        else
        {
            printf("client socket=%d connect success\n", fd);
            pthread_t tid;
			set_socket_non_block(fd);
		    int clientfd = fd;
            int ret = pthread_create(&tid, NULL, client_routine, &clientfd);
            if (ret != 0)
            {
                handle_error("create thread failed.");
            }
        }
    }

	return NULL;
}

int main(int argc, char *argv[])
{
    struct addrinfo hint, *result;
    struct sockaddr remote;
    int res, sfd, fd;

    memset(&hint, 0, sizeof(hint));
    hint.ai_family   = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = 0;
    hint.ai_flags    = AI_PASSIVE;

    res = getaddrinfo(NULL, "8080", &hint, &result);
    if (res != 0) 
    {
        perror("error : cannot get socket address!\n");
        exit(1);
    }

    sfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sfd == -1) 
    {
        perror("error : cannot get socket file descriptor!\n");
        exit(1);
    }

    set_socket_non_block(sfd);
    
    res = bind(sfd, result->ai_addr, result->ai_addrlen);
    if (res == -1) 
    {
        perror("error : cannot bind the socket with the given address!\n");
        exit(1);
    }

    res = listen(sfd, SOMAXCONN);
    if (res == -1) 
    {
        perror("error : cannot listen at the given socket!\n");
        exit(1);
    }
	
	pthread_t tid;
	int ret = pthread_create(&tid, NULL, server_routine, &sfd);
   
    for(;;);

    return 0;
}
