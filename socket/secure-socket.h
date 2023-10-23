#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "socket.h"

struct socket_info secure_rwsocket(const char *address, const char *hostname, int port, char *senddata, char *towrite, int amount) {
    struct socket_info errorStruct;
    errorStruct.descriptor = -1;
    errorStruct.bytesRead = -1;
    errorStruct.error = -1;
    errorStruct.extra = NULL;

    struct hostent *host;
    struct sockaddr_in server;
    int socket_desc = socket(PF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(address);

    while (1) {
        if (connect(socket_desc, (struct sockaddr *) & server, sizeof(server)) < 0) {
            if (errno == 115 || errno == 114) {
                continue;
            } else {
                errorStruct.error = -2;
                return errorStruct;
            }
        }
        break;
    }

    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (ctx == NULL) {
        errorStruct.error = -6;
        return errorStruct;
    }

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, socket_desc);

    // Send hostname data- some sites don't accept the
    // connection without this
    SSL_set_tlsext_host_name(ssl, hostname);

    // Clear the error queue to avoid leaving old errors in it
    ERR_clear_error();

    int conn = SSL_connect(ssl);
    if (conn != 1) {
        if (conn == -1) {
            int err = SSL_get_error(ssl, conn);
            if (err == 1) {
                unsigned long err = ERR_get_error();
                fprintf(stderr, "Error while securely connecting to server. Error code: %ld", err);
                char *buf = (char *) calloc(256, sizeof(char));
                ERR_error_string(err, buf);
                fprintf(stderr, " (%s)\n", buf);
                free(buf);
            } else {
                fprintf(stderr, "Error while securely connecting to server. Error code: %d\n", err);
            }
        } else {
            fprintf(stderr, "Error while securely connecting to server. Error code: \"%d\"\n", conn);
        }
        errorStruct.error = -7;
        return errorStruct;
    }

    if (SSL_write(ssl, senddata, strlen(senddata)) <= 0) {
        fprintf(stderr, "Error while writing data to server.\n");
        errorStruct.error = -3;
        return errorStruct;
    }

    char *server_reply = (char *) calloc(amount + 1, sizeof(char));
    int bytesRead = SSL_read(ssl, server_reply, amount);

    if (bytesRead < 0) {
        fprintf(stderr, "Error while receiving response from server.\n");
        errorStruct.error = -4;
        return errorStruct;
    }

    strncpy(towrite, server_reply, amount);
    free(server_reply);

    struct socket_info returnValue;
    returnValue.descriptor = socket_desc;
    returnValue.bytesRead = bytesRead;
    returnValue.error = 0;
    returnValue.extra = (void *) ssl;

    return returnValue;
}

struct socket_info secure_rsocket(struct socket_info info, char *towrite, int amount) {
    struct socket_info errorStruct;
    errorStruct.descriptor = info.descriptor;
    errorStruct.bytesRead = -1;
    errorStruct.error = -1;
    errorStruct.extra = info.extra;

    char *server_reply = (char *) calloc(amount + 1, sizeof(char));
    int bytesRead = SSL_read((SSL *) info.extra, server_reply, amount);
    if (bytesRead <= 0) {
        fprintf(stderr, "Error while receiving additional response from server.\n");
        return errorStruct;
    }

    strncpy(towrite, server_reply, amount);
    free(server_reply);

    struct socket_info returnValue;
    returnValue.descriptor = info.descriptor;
    returnValue.bytesRead = bytesRead;
    returnValue.error = 0;
    returnValue.extra = info.extra;

    return returnValue;
}

void secure_csocket(struct socket_info info) {
    close(info.descriptor);
    SSL_CTX_free(SSL_get_SSL_CTX((SSL *) info.extra));
}
