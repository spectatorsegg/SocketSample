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

/* wolfSSL */
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/wolfio.h>
#include <wolfssl/wolfcrypt/error-crypt.h>

/* Definition */
#define EMSG(msg)   (fprintf(stderr, msg))
#define CERT_FILE "./certs/client-cert.pem"
#define KEY_FILE  "./certs/client-key.pem"
#define CA_FILE   "./certs/ca-cert.pem"

int recv_file(WOLFSSL *ssl, FILE *fp);

/* Main function */
int main(int argc, char *argv[]) 
{
    struct sockaddr_in server;
    int sock;
    char buff[100] = {0};
    int pnum;

    char fname[30] = {0};
    char recv_fname[40] = "rcv_";
    FILE *fp;
    int recv_size;

    WOLFSSL_CTX* ctx = NULL;
    WOLFSSL*     ssl = NULL;
 
     /* Create an endpoint for communication */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        EMSG("ERROR: Failed to create a socket.\n");
        goto exit1;
    }

    /* Initiate a connecion on the socket */
    server.sin_family = AF_INET;
    server.sin_port = htons(TEST_PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == -1) {
        EMSG("ERROR: Failed to connect.\n");
        goto exit2;
    }

    /* Initialize wolfSSL */
    if (wolfSSL_Init() != WOLFSSL_SUCCESS) {
        EMSG("ERROR: Failed to initialize the library.\n");
        goto exit2;
    }

    /* Create and initialize WOLFSSL_CTX */
    if ((ctx = wolfSSL_CTX_new(wolfTLSv1_3_client_method())) == NULL) {
        EMSG("ERROR: failed to create WOLFSSL_CTX.\n");
        goto exit2;
    }

    /* Load client certificate into WOLFSSL_CTX */
    if (wolfSSL_CTX_use_certificate_file(ctx, CERT_FILE, WOLFSSL_FILETYPE_PEM) != WOLFSSL_SUCCESS) {
        EMSG("ERROR: failed to load the certificate file.\n");
        goto exit3;
    }

    /* Load client key into WOLFSSL_CTX */
    if (wolfSSL_CTX_use_PrivateKey_file(ctx, KEY_FILE, WOLFSSL_FILETYPE_PEM) != WOLFSSL_SUCCESS) {
        EMSG("ERROR: failed to load the key file.\n");
        goto exit3;
    }

    /* Load CA certificate into WOLFSSL_CTX */
    if (wolfSSL_CTX_load_verify_locations(ctx, CA_FILE, NULL) != WOLFSSL_SUCCESS) {
        EMSG("ERROR: failed to load the CA file.\n");
        goto exit3;
    }

    /* Create a WOLFSSL object */
    if ((ssl = wolfSSL_new(ctx)) == NULL) {
        EMSG("ERROR: failed to create WOLFSSL object.\n");
        goto exit3;
    }

    /* Attach wolfSSL to the socket */
    if (wolfSSL_set_fd(ssl, sock) != WOLFSSL_SUCCESS) {
        EMSG("ERROR: Failed to set the file descriptor.\n");
        goto exit3;
    }

    /* Connect to wolfSSL on the server side */
    if (wolfSSL_connect(ssl) != WOLFSSL_SUCCESS) {
        EMSG("ERROR: failed to connect to wolfSSL.\n");
        goto exit3;
    }

    while (1) {
        /* Receive a message from the server */
        printf("receive usage\n");
        memset(buff, 0, sizeof(buff));
        if (wolfSSL_read(ssl, (void*)buff, sizeof(buff)) < 0) {
            EMSG("ERROR: failed to recieve.\n");
            goto exit3;
        }
        printf("%s\n", buff);
 
        /* Wait for input from stdin */
        pnum = getchar();

        /* Send photo number */
        if (wolfSSL_write(ssl, (void*)&pnum, sizeof(pnum)) != sizeof(pnum)) {
            EMSG("ERROR: failed to send photo number.\n");
            goto exit3;
        }

        /* Receive a reply from the server */
        memset(buff, 0, sizeof(buff));
        if (wolfSSL_read(ssl, (void*)buff, sizeof(buff)) < 0) {
            EMSG("ERROR: failed to recieve.\n");
            goto exit3;
        }

        if (strncmp(buff, "Request accepted.", 17) == 0) {
            printf("Request accepted.\n");
            break;
        } else {
            printf("%s\n\n", buff);
        }
    }

    if (pnum == '0') {
        goto exit3;
    }

    /* Receive the name of the file to be transferred */
    if (wolfSSL_read(ssl, (void*)fname, sizeof(fname)) < 0) {
        EMSG("ERROR: failed to recieve.\n");
        goto exit3;
    }

    /* Create a file to save received data */
    strcat(recv_fname, fname);
    fp = fopen(recv_fname, "wb");
    if (fp == NULL) {
        EMSG("ERROR: failed to open the file.\n");
        goto exit3;
    }

    /* Receive the size of file */
    if (wolfSSL_read(ssl, (void*)&pnum, sizeof(pnum)) < 0) {
        EMSG("ERROR: failed to recieve.\n");
        goto exit3;
    }
    printf("File size = %d\n", pnum);
    
    /* Receive image data */
    recv_size = recv_file(ssl, fp);
    if (recv_size <= 0) {
        printf("Receive failed\n");
    } else {
        printf("Receive success, NumBytes = %d\n", recv_size);
    }
    
    fclose(fp);

    /* Display image */
    DisplayImage((const char*)recv_fname);

exit3:
    if (ssl) {
        wolfSSL_free(ssl);      /* Free the wolfSSL object                  */
    }
    if (ctx) {
        wolfSSL_CTX_free(ctx);  /* Free the wolfSSL context object          */
    }
    wolfSSL_Cleanup();          /* Cleanup the wolfSSL environment          */

exit2:
    close(sock);

exit1:
    return 0;
}


/* Receive and save data */
int recv_file(WOLFSSL *ssl, FILE *fp)
{
    int recv_size;
    int total_size = 0;
    char buff[BUFF_SIZE] = {0};
    int err;
    char err_msg[80];

    sleep(1);
    do {
        /* Receive file data */
        if ((recv_size = wolfSSL_read(ssl, (void*)buff, sizeof(buff))) < 0) {
            printf("recv_size = %d\n", recv_size);
            err = wolfSSL_get_error(ssl, 0);
            wolfSSL_ERR_error_string(err, err_msg);
            printf("err = %d, %s\n", err, err_msg);
 //           fprintf(stderr, "wolfSSL_read error = %d\n",
 //               wolfSSL_get_error(ssl, recv_size));
            
//            perror("recv error");
//            total_size = -1;
//            break;
        }
 
        /* Save data to the file */
        if (fwrite(buff, sizeof(char), recv_size, fp) != (size_t)recv_size) {
            EMSG("ERROR: failed to write.\n");
            total_size = -1;
            break;
        }

        printf("recv_size = %d\n", recv_size);
 
        memset(buff, 0, BUFF_SIZE);
        
        total_size += recv_size;

    } while (recv_size > 0);

    return total_size;
}