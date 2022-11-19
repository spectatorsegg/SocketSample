/* Include file */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include "config.h"

/* Definition */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr)[0])

/* Prototype */
ssize_t send_file(int sockfd, FILE *fp);
void *server_thread(void *arg);


int main(int argc, char *argv[]) 
{
    int sock;
    struct sockaddr_in addr;
    struct sockaddr_in client;
    int len;
    int sock0;
    pthread_t th;

    /* Create an endpoint for communication */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket error");
        exit(1);
    }

    /* Bind a name to the socket */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind error");
        exit(1);
    }

    /* Listen for connections on the socket */
    if (listen(sock, MAX_QUEUE) == -1) {
        perror("listen error");
        exit(1);
    }

    /* Accept a connetion on the socket */
    len = sizeof(client);

    while(1) {
        sock0 = accept(sock, (struct sockaddr*)&client, (socklen_t*)&len);
        if (sock0 == -1) {
            perror("accept error");
            exit(1);
        }

        printf("pthread_create\n");
        if (pthread_create(&th, NULL, server_thread, (void*)&sock0) != 0) {
            perror("pthread_create error");
            exit(1);
        }
    }
    close(sock);

    return 0;
}

/* Thread */
void *server_thread(void *arg)
{
    int *sock0;
    char fname[] = "testimage.jpg";
    char buff[30] = {0};
    FILE *fp;
    ssize_t send_size;

    printf("server_thread\n");
    (void)pthread_detach(pthread_self());
    sock0 = (int*)arg;

    /* Send the name of the file to be transferred */
    strncpy(buff, fname, ARRAY_SIZE(fname));
    printf("send\n");
    if (send(*sock0, buff, sizeof(buff), 0) == -1) {
        perror("send filename error");
        exit(1);
    }
    
    printf("fopen\n");
    /* Open the file to be transferred */
    fp = fopen(fname, "rb");
    if (fp == NULL) {
        perror("fopen error");
        exit(1);
    }

    /* Read and send data */
    send_size = send_file(*sock0, fp);
    if (send_size <= 0) {
        printf("Send failed\n");
    } else {
        printf("Send success, NumBytes = %ld\n", send_size);
    }

    fclose(fp);
    close(*sock0);
    pthread_exit((void*)0);
}

/* Read and send data */
ssize_t send_file(int sockfd, FILE *fp) 
{
    size_t read_size;
    ssize_t send_size;
    ssize_t total_size = 0;
    char buff[BUFF_SIZE] = {0};

    do {
        /* Read data from the file */
        read_size = fread(buff, sizeof(char), BUFF_SIZE, fp);
        if (ferror(fp)) {
            perror("fread error");
            total_size = -1;
            break;
        }

        /* Send data */
        send_size = send(sockfd, buff, read_size, 0);
        if (send_size == -1)
        {
            perror("send error");
            total_size = -1;
            break;
        }

        printf("send_size = %ld\n", send_size);
        total_size += send_size;

        memset(buff, 0, BUFF_SIZE);

    } while(send_size > 0);

    return total_size;
}

