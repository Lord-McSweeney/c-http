#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "../socket/socket.h"
#include "response.h"
#include "chunked.h"

typedef void (*dataReceiveHandler)(void *);

#ifndef _HTTP_HTTP
    #define _HTTP_HTTP 1
    #define maxResponseSize 1048578

struct http_url {
    char *protocol;
    char *hostname;
    short port;
    char *path;
    char *fragment;
};

enum _http_internal_url_parser_state {
    HTTP_PROTOCOL,
    HTTP_PROTOCOL_1,
    HTTP_PROTOCOL_2,
    HTTP_HOSTNAME,
    HTTP_PORT,
    HTTP_PATH,
    HTTP_FRAGMENT,
};

char *HTTP_makeStrCpy(const char *string) {
    char *destString = (char *) calloc(strlen(string) + 1, sizeof(char));
    strcpy(destString, string);
    return destString;
}

/*
errno list
1: End of URL reached, but parsing had not finished
2: Malformed URL
3: Illegal hostname
4: Illegal port
*/

struct http_url* http_url_from_string(char *string) {
    errno = 0;
    struct http_url* result_mem = (struct http_url*) calloc(1, sizeof(struct http_url));

    int urllen = strlen(string);
    
    // Special percent-encoded characters
    int numSpecial = 0;
    for (int i = 0; i < urllen; i ++) {
        if (string[i] == ' ') {
            numSpecial ++;
        }
    }
    
    char *protocol = (char *) calloc(urllen + numSpecial * 3 + 1, sizeof(char));
    char *hostname = (char *) calloc(urllen + numSpecial * 3 + 1, sizeof(char));
    char *port = (char *) calloc(urllen + numSpecial * 3 + 5, sizeof(char));
    char *path = (char *) calloc(urllen + numSpecial * 3 + 1, sizeof(char));
    char *fragment = (char *) calloc(urllen + numSpecial * 3, sizeof(char));
    
    int currentState = HTTP_PROTOCOL;
    int currentIndex = 0;
    int isIp = 0;
    for (int i = 0; i < urllen; i ++) {
        char curChar = string[i];
        switch (currentState) {
            case HTTP_PROTOCOL:
                if (curChar == 0) {
                    errno = 1;
                    return NULL;
                }

                if (curChar == ':') {
                    currentState = HTTP_PROTOCOL_1;
                    currentIndex = 0;
                    break;
                }
                
                // Protocols cannot have periods inside them; if they do, turns out we were parsing a hostname 
                if (curChar == '.') {
                    i = -1;
                    free(protocol);
                    protocol = HTTP_makeStrCpy("http");
                    currentState = HTTP_HOSTNAME;
                    currentIndex = 0;
                    break;
                }
                protocol[strlen(protocol)] = curChar;
                break;
            case HTTP_PROTOCOL_1:
                if (curChar == '/') {
                    currentState = HTTP_PROTOCOL_2;
                    currentIndex = 0;
                } else {
                    // turns out we started with the hostname, and this is the port
                    if (curChar >= '0' && curChar <= '9') {
                        i = -1;
                        free(protocol);
                        protocol = HTTP_makeStrCpy("http");
                        currentState = HTTP_HOSTNAME;
                        currentIndex = 0;
                        break;
                    } else {
                        errno = 2;
                        return NULL;
                    }
                }
                break;
            case HTTP_PROTOCOL_2:
                if (curChar == '/') {
                    currentState = HTTP_HOSTNAME;
                    currentIndex = 0;
                    break;
                } else {
                    errno = 2;
                    return NULL;
                }
                break;
            case HTTP_HOSTNAME:
                // TODO: This is actually probably an IP address
                if (currentIndex == 0 && curChar >= '0' && curChar <= '9') {
                    isIp = 1;
                }
                if (isIp && ((curChar >= 'a' && curChar <= 'z') || (curChar >= 'A' && curChar <= 'Z'))) {
                    return NULL;
                }
                if (curChar == ':') {
                    if (currentIndex == 0) {
                        errno = 3;
                        return NULL;
                    }
                    currentState = HTTP_PORT;
                    currentIndex = 0;
                    break;
                }
                if (curChar == '/') {
                    if (currentIndex == 0) {
                        errno = 3;
                        return NULL;
                    }
                    currentState = HTTP_PATH;
                    currentIndex = 0;
                    break;
                }
                hostname[strlen(hostname)] = curChar;
                break;
            case HTTP_PORT:
                if (curChar == '/') {
                    currentState = HTTP_PATH;
                    currentIndex = 0;
                    break;
                }
                if (curChar < '0' || curChar > '9') {
                    errno = 4;
                    return NULL;
                }
                port[strlen(port)] = curChar;
                break;
            case HTTP_PATH:
                if (curChar == '#') {
                    currentState = HTTP_FRAGMENT;
                    currentIndex = 0;
                    break;
                }
                if (curChar == ' ') {
                    path[strlen(path)] = '%';
                    path[strlen(path)] = '2';
                    path[strlen(path)] = '0';
                    break;
                }
                path[strlen(path)] = curChar;
                break;
            case HTTP_FRAGMENT:
                fragment[strlen(fragment)] = curChar;
                break;
        }
        currentIndex ++;
    }

    if (strlen(path) == 0) {
        free(path);
        path = HTTP_makeStrCpy("/");
    } else if (path[0] != '/') {
        memmove(path + 1, path, strlen(path));
        path[0] = '/';
    }

    if (strlen(port) == 0) {
        if (!strcmp(protocol, "http")) {
            strcpy(port, "80");
        } else if (!strcmp(protocol, "https")) {
            strcpy(port, "443");
        } else {
            strcpy(port, "80");
        }
    }

    if (!strcmp(protocol, "file")) {
        strcpy(port, "0");
        hostname = HTTP_makeStrCpy("localhost");
        printf("path: %s\n", path);
    }

    result_mem->protocol = protocol;
    result_mem->hostname = HTTP_makeStrCpy(hostname);
    result_mem->port = atoi(port);
    result_mem->path = path;
    result_mem->fragment = fragment;
    free(hostname);
    free(port);

    return result_mem;
}

struct http_response http_readFileToHTTP(char *path) {
    char *contents = (char *) calloc(1048576, sizeof(char));
    errno = 0;
    FILE *fp = fopen(path, "r");
    if (errno == ENOENT) {
        struct http_response err;
        err.error = 198;
        return err;
    }
    if (errno == EACCES) {
        struct http_response err;
        err.error = 196;
        return err;
    }

    errno = 0;
    unsigned int c = fgetc(fp);
    if (errno == EISDIR) {
        struct http_response err;
        err.error = 197;
        return err;
    }

    int i = 0;
    while(c != EOF) {
        contents[i] = c;
        c = fgetc(fp);
        i ++;
        if (i > 1048574) {
            break;
        }
    }

    struct http_response result;
    result.response_code = 200;
    result.response_description = HTTP_makeStrCpy("OK");
    result.num_headers = 0;
    result.headers = NULL;
    result.is_chunked = 0;
    result.is_html = 0;
    result.content_length = i;
    result.error = 0;

    struct http_data res;
    res.length = i;
    res.data = contents;
    result.response_body = res;

    return result;
}

struct http_response http_makeHTTPRequest(char *charURL, dataReceiveHandler chunkHandler, dataReceiveHandler finishHandler, void *chunkArg) {
    struct http_url *url = http_url_from_string(charURL);

    if (!strcmp(url->protocol, "file")) {
        return http_readFileToHTTP(url->path);
    }

    if (errno) {
        struct http_response failure;
        failure.error = 4;
        return failure;
    }
    struct http_response errorResponse;
    errorResponse.error = 1;

    errno = 0;
    char *buffer = (char *) calloc(maxResponseSize, sizeof(char)); // this should be dynamically returned by rwsocket, but 1mb is probably good for now
    char *ipBuffer = (char *) calloc(4096, sizeof(char)); // this should be dynamic, but 4kb is probably good for now

    char *baseString = "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\n\r\n";
    char *requestString = (char *) calloc(strlen(baseString) + strlen(url->path) + strlen(url->hostname) + strlen("uqers") + 1, sizeof(char));
    sprintf(requestString, baseString, url->path, url->hostname, "uqers");

    int res = lookupIP(url->hostname, ipBuffer);
    if (res) {
        if (res == HOST_NOT_FOUND) {
            //printf("Host not found\n");
            errorResponse.error = 5;
            return errorResponse;
        }
        if (res == TRY_AGAIN) {
            //printf("Temporary error in name resolution\n");
            errorResponse.error = 7;
            return errorResponse;
        }
        errorResponse.error = 6;
        return errorResponse;
    }
    struct socket_info tcpResult = rwsocket(ipBuffer, url->port, requestString, buffer, maxResponseSize);
    free(ipBuffer);
    free(url->protocol);
    free(url->hostname);
    free(url->path);
    free(url->fragment);
    free(url);
    free(requestString);

    if (chunkHandler != NULL) chunkHandler(chunkArg);

    if (tcpResult.error) {
        switch(errno) {
            case 101:
              //  printf("Network unreachable\n");
                errorResponse.error = errno;
                return errorResponse;
            case 111:
              //  printf("Connection refused\n");
                errorResponse.error = errno;
                return errorResponse;
            case 113:
               // printf("No route to host\n");
                errorResponse.error = errno;
                return errorResponse;
        }
        //printf("error: %d\n", errno);
    }

    // we can't realloc-shrink the buffer because it gets reused


    struct http_data initialHttpResponse;
    initialHttpResponse.length = tcpResult.bytesRead;
    initialHttpResponse.data = buffer;/*
    if (tcpResult.error) {
        printf("Error code: %d", tcpResult.error);
    }*/

    struct http_response parsedResponse = parsePossiblyIncompleteHTTPResponse(initialHttpResponse, "1.1");

    if (parsedResponse.error) {
        errorResponse.error = parsedResponse.error;
        return errorResponse;
    }

    if (parsedResponse.is_chunked) {
        int totalBytesRead = tcpResult.bytesRead;

        struct chunked_response_state chunkedResponse = parseChunkedResponse(parsedResponse.response_body);
        if (chunkedResponse.error) {
            //fprintf(stderr, "Encountered error while parsing chunked response: %d\n", chunkedResponse.error);
            errorResponse.error = 199;
            return errorResponse;
        }
        parsedResponse.response_body = chunkedResponse.current_parsed_data;

        char *currentPosition = buffer + totalBytesRead;
        while (!chunkedResponse.finished) {
            //fprintf(stderr, "Current position: %ld\n", (long) currentPosition);
            currentPosition = buffer + totalBytesRead;
            errno = 0;
            tcpResult = rsocket(tcpResult.descriptor, currentPosition, 1048570 - totalBytesRead);
            totalBytesRead += tcpResult.bytesRead;

            if (chunkHandler != NULL) chunkHandler(chunkArg);

            initialHttpResponse.data = currentPosition;
            initialHttpResponse.length = tcpResult.bytesRead;
            //fprintf(stderr, "About to append chunk to body\n");
            appendChunkToBody(&chunkedResponse, initialHttpResponse);
            //fprintf(stderr, "Appended chunk to body\n");

            if (chunkedResponse.error) {
                errorResponse.error = 199;
                return errorResponse;
            }

            free(parsedResponse.response_body.data);
            parsedResponse.response_body = chunkedResponse.current_parsed_data;
        }
        if (finishHandler != NULL) finishHandler(chunkArg);
    } else if (parsedResponse.content_length == -2) {
        // puts("WARNING: Content-Length header was not returned, assuming that that was the whole response");
        // This is pretty normal, no reason to warn
    } else {
        if (parsedResponse.content_length != parsedResponse.response_body.length) {
            int totalBytesRead = 0; // note; this implementation of a repeated read doesn't actually match up with the chunked response response length handling
            char *currentPosition;
            char *extra = NULL;

            while (parsedResponse.content_length > parsedResponse.response_body.length) {
                if (extra) free(extra);

                currentPosition = buffer + totalBytesRead;
                errno = 0;
                tcpResult = rsocket(tcpResult.descriptor, currentPosition, 1048570 - totalBytesRead);
                totalBytesRead += tcpResult.bytesRead;

                if (chunkHandler != NULL) chunkHandler(chunkArg);

                initialHttpResponse.data = currentPosition;
                initialHttpResponse.length = tcpResult.bytesRead;

                char *extra = (char *) calloc(totalBytesRead + parsedResponse.response_body.length + 2, sizeof(char));

                memcpy(extra, parsedResponse.response_body.data, parsedResponse.response_body.length);
                memcpy(extra + parsedResponse.response_body.length, initialHttpResponse.data, initialHttpResponse.length);
                free(parsedResponse.response_body.data);

                int newLength = parsedResponse.response_body.length + initialHttpResponse.length;

                struct http_data newData;
                newData.data = extra;
                newData.length = newLength;

                parsedResponse.response_body = newData;
            }
        }
        if (finishHandler != NULL) finishHandler(chunkArg);
    }

    free(buffer);

    return parsedResponse;
}

#endif
