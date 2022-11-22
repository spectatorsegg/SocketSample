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

/* wolfSSL */
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

/* Definition */
#define MAX_QUEUE   5
#define MAX_THREADS 5
#define EMSG(msg)   (fprintf(stderr, msg))

#define CERT_FILE "./certs/server-cert.pem"
#define KEY_FILE  "./certs/server-key.pem"
#define CA_FILE   "./certs/client-cert.pem"

struct tharg {
    int          open;
    pthread_t    tid;
    int          num;
    int          connd;
    WOLFSSL_CTX* ctx;
    int*         shutdown;
};

void *server_thread(void *arg);
int send_file(WOLFSSL *ssl, FILE *fp);


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
    WOLFSSL_CTX* ctx = NULL;

    /* Create an endpoint for communication */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) { 
        EMSG("ERROR: Failed to create a socket.\n");
        goto exit1;
    }

    /* Bind a name to the socket */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        EMSG("ERROR: Failed to bind.\n");
        goto exit2;
    }

    /* Listen for connections on the socket */
    if (listen(sock, MAX_QUEUE) == -1) {
        EMSG("ERROR: Failed to listen.\n");
        goto exit2;
    }

     /* Initialize wolfSSL */
    if (wolfSSL_Init() != WOLFSSL_SUCCESS) {
        EMSG("ERROR: Failed to initialize wolfSSL.\n");
        goto exit2;
    }

    /* Create and initialize WOLFSSL_CTX */
    if ((ctx = wolfSSL_CTX_new(wolfTLSv1_3_server_method())) == NULL) {
        EMSG("ERROR: failed to create WOLFSSL_CTX.\n");
        goto exit2;
    }

    /* Require mutual authentication */
    wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_PEER | WOLFSSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);

    /* Load server certificates into WOLFSSL_CTX */
    if (wolfSSL_CTX_use_certificate_file(ctx, CERT_FILE, WOLFSSL_FILETYPE_PEM) != WOLFSSL_SUCCESS) {
        EMSG("ERROR: failed to load the certificate file.\n");
        goto exit3;
    }

    /* Load server key into WOLFSSL_CTX */
    if (wolfSSL_CTX_use_PrivateKey_file(ctx, KEY_FILE, WOLFSSL_FILETYPE_PEM) != WOLFSSL_SUCCESS) {
        EMSG("ERROR: failed to load the key file.\n");
        goto exit3;
    }

    /* Load client certificate as "trusted" into WOLFSSL_CTX */
    if (wolfSSL_CTX_load_verify_locations(ctx, CA_FILE, NULL) != WOLFSSL_SUCCESS) {
        EMSG("ERROR: failed to load the CA file.\n");
        goto exit3;
    }

    /* Initialize thread array */
    for (i = 0; i < MAX_THREADS; i++) {
        thread[i].open = 1;
        thread[i].num = i;
        thread[i].ctx = ctx;
        thread[i].shutdown = &shutdown;
    }

    len = sizeof(client);
    while (!shutdown) {
        /* Skip if the maximum number of threads has been reached */
        for (i = 0; (i < MAX_THREADS) && (thread[i].open == 0); i++);
        if (i == MAX_THREADS) {
            continue;
        }
    
        /* Accept a connetion on the socket */
        printf("wait for connection from clients...\n");
        if ((sock0 = accept(sock, (struct sockaddr*)&client, (socklen_t*)&len)) == -1) {
            EMSG("ERROR: failed to accept.\n");
            goto exit3;
        }

        /* Set thread argument */
        thread[i].open = 0;
        thread[i].connd = sock0;

        /* Create a new thread */
        if (pthread_create(&thread[i].tid, NULL, server_thread, &thread[i]) != 0) {
            EMSG("ERROR: failed to create a thread.\n");
            close(sock0);
            goto exit3;
        }

        /* Detach the thread to automatically release its resources when the thread terminates */
        pthread_detach(thread[i].tid);
    }

    /* Suspend shutdown untill all threads are closed */
    do {
        shutdown = 1;
        for (i = 0; i < MAX_THREADS; i++) {
            if (thread[i].open == 0) {
                shutdown = 0;
            }
        }
    } while (!shutdown);

    printf("Shutdown complete\n");


exit3:
    if (ctx) {
        /* Free the wolfSSL context object */
        wolfSSL_CTX_free(ctx);
    }
    /* Cleanup the wolfSSL environment */
    wolfSSL_Cleanup();          

exit2:
    close(sock);

exit1:
    return 0;
}


/* Thread */
void *server_thread(void *arg)
{
    struct tharg *thread = arg;
    const char usage[] =
                "Select a number.\n"
                " '0': Shutdown\n"
                " '1': Send photo 1\n"
                " '2': Send photo 2\n"
                " '3': Send photo 3\n";
    const char rep_success[] = "Request accepted.";
    const char rep_failure[] = "Wrong number.";
    int pnum;
    const char image1[] = "image1.jpg";
    const char image2[] = "image2.jpg";
    const char image3[] = "image3.jpg";

    char buff[30] = {0};
    FILE *fp;
    int send_size;
    WOLFSSL*    ssl = NULL;

    printf("thread %d open\n", thread->num);

    /* Create a WOLFSSL object */
    if ((ssl = wolfSSL_new(thread->ctx)) == NULL) {
        EMSG("ERROR: failed to create WOLFSSL object.\n");
        goto exit;
    }

    /* Attach wolfSSL to the socket */
    wolfSSL_set_fd(ssl, thread->connd);

    /* Establish TLS connection */
    if (wolfSSL_accept(ssl) != WOLFSSL_SUCCESS) {
        EMSG("ERROR: failed to accept WOLFSSL.\n");
        goto exit;
    }

    while (1) {
        /* Send usage instrunctions to the client */
        if (wolfSSL_write(ssl, usage, sizeof(usage)) != sizeof(usage)) {
            EMSG("ERROR: failed to send usage.\n");
            continue;
        }

        /* Receive a photo number */
        if (wolfSSL_read(ssl, &pnum, sizeof(pnum)) < 0) {
            EMSG("ERROR: failed to receive photo number.\n");
            continue;
        }
        if (pnum == '0' ||
            pnum == '1' ||
            pnum == '2' ||
            pnum == '3') {
            /* Send reply message */
            if (wolfSSL_write(ssl, rep_success, sizeof(rep_success)) != sizeof(rep_success)) {
                EMSG("ERROR: failed to send reply.\n");
                continue;
            }            
            break;
        } else {
            /* Send reply message */
            if (wolfSSL_write(ssl, rep_failure, sizeof(rep_failure)) != sizeof(rep_failure)) {
                EMSG("ERROR: failed to send reply.\n");
            }            
        }
    }

    switch (pnum) {
        case '0':
            *thread->shutdown = 1;
            goto exit;
        case '1':
            strncpy(buff, image1, sizeof(buff));
            break;
        case '2':
            strncpy(buff, image2, sizeof(buff));
            break;
        case '3':
            strncpy(buff, image3, sizeof(buff));
            break;
        default:
            break;
    }

    /* Send the name of the file to be transferred */
    if (wolfSSL_write(ssl, buff, sizeof(buff)) != sizeof(buff)) {
        EMSG("ERROR: failed to send the filename.\n");
        goto exit;
    }
    
    /* Open the file */
    if ((fp = fopen(buff, "rb")) == NULL) {
        EMSG("ERROR: failed to open the file.\n");
        goto exit;
    }

    /* Read and send data */
    send_size = send_file(ssl, fp);
    if (send_size <= 0) {
        printf("Send failed\n");
    } else {
        printf("Send success, NumBytes = %d\n", send_size);
    }

    fclose(fp);

    /* Cleanup after this connection */
    wolfSSL_shutdown(ssl);

exit:
    if (ssl) {
        /* Free the wolfSSL object */
        wolfSSL_free(ssl);
    }
    close(thread->connd);
    thread->open = 1;
    printf("thread %d exit\n", thread->num);
    pthread_exit(NULL);
}

/* Read and send data */
int send_file(WOLFSSL *ssl, FILE *fp) 
{
    int read_size;
    int send_size;
    int total_size = 0;
    char buff[BUFF_SIZE] = {0};

    do {
        /* Read data from the file */
        read_size = (int)fread(buff, sizeof(char), sizeof(buff), fp);
        if (ferror(fp)) {
            EMSG("ERROR: failed to read the file.\n");
            total_size = -1;
            break;
        }

        /* Send data */
        if ((send_size = wolfSSL_write(ssl, buff, read_size)) != read_size) {
            EMSG("ERROR: failed to send data.\n");
            total_size = -1;
            break;
        }

        printf("send size = %d\n", send_size);
        total_size += send_size;

        memset(buff, 0, BUFF_SIZE);

    } while(send_size > 0);

    return total_size;
}

