/* Include file */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "config.h"
#include "opencv_test.h"

/* Prototype */
ssize_t recv_file(int sockfd, FILE *fp);

int main(void)
{
    struct sockaddr_in server;
    int sock;
    char fname[30] = {0};
    char recv_fname[50] = "rcv_";
    FILE *fp;
    ssize_t recv_size;

     /* Create an endpoint for communication */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket error");
        exit(1);
    }

    /* Initiate a connecion on the socket */
    server.sin_family = AF_INET;
    server.sin_port = htons(TEST_PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == -1) {
        perror("connect error");
        exit(1);
    }

    /* Receive the name of the file to be transferred */
    if (recv(sock, (void*)fname, sizeof(fname), 0) == -1) 
    {
        perror("recv error");
        exit(1);
    }

    /* Create a file to save received data */
    strcat(recv_fname, fname);
    fp = fopen(recv_fname, "wb");
    if (fp == NULL) 
    {
        perror("fopen error");
        exit(1);
    }
    
    /* Receive and save data */
    recv_size = recv_file(sock, fp);
    if (recv_size <= 0) {
        printf("Receive failed\n");
    } else {
        printf("Receive success, NumBytes = %ld\n", recv_size);
    }
    
    fclose(fp);
    close(sock);

    /* Display image */
    DisplayImage((const char*)recv_fname);

    return 0;
}


/* Receive and save data */
ssize_t recv_file(int sockfd, FILE *fp)
{
    ssize_t recv_size;
    ssize_t total_size = 0;
    char buff[BUFF_SIZE] = {0};

    do {
        /* Receive file data */
        recv_size = recv(sockfd, buff, sizeof(buff), 0);
        if (recv_size == -1) {
           perror("recv error");
           total_size = -1;
           break;
        }
 
        /* Save data to the file */
        if (fwrite(buff, sizeof(char), recv_size, fp) != (size_t)recv_size) {
            perror("fwrite error");
            total_size = -1;
            break;
        }

        printf("recv_size = %ld\n", recv_size);
 
        memset(buff, 0, BUFF_SIZE);
        
        total_size += recv_size;

    } while (recv_size > 0);

    return total_size;
}


