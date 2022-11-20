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
#define MAX_QUEUE   5
#define MAX_THREADS 5
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr)[0])

struct tharg {
    int          open;
    pthread_t    tid;
    int          num;
    int          connd;
//    WOLFSSL_CTX* ctx;
    int*         shutdown;
};

void *server_thread(void *arg);
ssize_t send_file(int sockfd, FILE *fp);


/* Main function */
int main(int argc, char *argv[]) 
{
    int sock;
    struct sockaddr_in addr;
    struct sockaddr_in client;
    int len;
    int sock0;
    struct tharg thread[MAX_THREADS];
    int i;
    int shutdown = 0;

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
        goto exit;
    }

    /* Listen for connections on the socket */
    if (listen(sock, MAX_QUEUE) == -1) {
        perror("listen error");
        goto exit;
    }

    /* Initialize thread array */
    for (i = 0; i < MAX_THREADS; i++) {
        thread[i].open = 1;
        thread[i].num = i;
        thread[i].shutdown = &shutdown;
    }

    len = sizeof(client);
    while (shutdown == 0) {
        /* Skip if the maximum number of threads has been reached */
        for (i = 0; (i < MAX_THREADS) && (thread[i].open == 0); i++);
        if (i == MAX_THREADS) {
            continue;
        }
    
        /* Accept a connetion on the socket */
        sock0 = accept(sock, (struct sockaddr*)&client, (socklen_t*)&len);
        if (sock0 == -1) {
            perror("accept error");
            goto exit;
        }

        /* Set thread argument */
        thread[i].open = 0;
        thread[i].connd = sock0;

        /* Create a new thread */
        if (pthread_create(&thread[i].tid, NULL, server_thread, &thread[i]) != 0) {
            perror("pthread_create error");
            goto exit;
        }

        /* Detach the thread to automatically release its resources when the thread terminates */
        pthread_detach(thread[i].tid);
    }

    printf("Exit while loop\n");
    /* Suspend shutdown untill all threads are closed */
    do {
        shutdown = 1;
        for (i = 0; i < MAX_THREADS; i++) {
            if (thread[i].open == 0) {
                shutdown = 0;
            }
        }
    } while (shutdown == 0);

    printf("Shutdown complete\n");

exit:
    close(sock);

    return 0;
}

/* Thread */
void *server_thread(void *arg)
{
    struct tharg *thread = arg;
    const char usage[] =
                "Select a number.\n"
                "0: Shutdown\n"
                "1: Send photo 1\n"
                "2: Send photo 2\n";
    const char rep_success[] = "Request accepted.";
    const char rep_failure[] = "Incorrect number.";
    int pnum;
    const char image1[] = "image1.jpg";
    const char image2[] = "image2.jpg";
 
    char buff[30] = {0};
    FILE *fp;
    ssize_t send_size;

    printf("thread %d open\n", thread->num);

    while (1) {
        /* Send usage instrunctions to the client */
        printf("send usage\n");
        if (send(thread->connd, usage, sizeof(usage), 0) == -1) {
            perror("send usage error");
            continue;
        }

        /* Receive photo number */
        if (recv(thread->connd, &pnum, sizeof(pnum), 0) == -1) {
            perror("receive photo number error");
            continue;
        }
        if (pnum == '0' ||
            pnum == '1' ||
            pnum == '2') {
            /* Send reply message */
            if (send(thread->connd, rep_success, sizeof(rep_success), 0) == -1) {
                perror("send reply error");
                continue;
            }            
            break;
        } else {
            /* Send reply message */
            if (send(thread->connd, rep_failure, sizeof(rep_failure), 0) == -1) {
                perror("send reply error");
            }            
        }
    }

    switch (pnum) {
        case '0':
            *thread->shutdown = 1;
            break;
        case '1':
            strncpy(buff, image1, sizeof(buff));
            break;
        case '2':
            strncpy(buff, image2, sizeof(buff));
            break;
        default:
            break;
    }

    /* Send the name of the file to be transferred */
    printf("Send filename\n");
    if (send(thread->connd, buff, sizeof(buff), 0) == -1) {
        perror("send filename error");
        exit(1);
    }
    
    printf("Fopen\n");
    /* Open the file to be transferred */
    fp = fopen(buff, "rb");
    if (fp == NULL) {
        perror("fopen error");
        exit(1);
    }

    /* Read and send data */
    send_size = send_file(thread->connd, fp);
    if (send_size <= 0) {
        printf("Send failed\n");
    } else {
        printf("Send success, NumBytes = %ld\n", send_size);
    }

    fclose(fp);


    close(thread->connd);
    thread->open = 1;
    printf("Thread exit\n");
    pthread_exit(NULL);
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

