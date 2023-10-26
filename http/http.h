#include <errno.h>

#ifndef _HTTP_HTTP

    #include "../socket/secure-socket.h"
    #include "../socket/socket.h"
    #include "../utils/string.h"

    #include "chunked.h"
    #include "cookie.h"
    #include "response.h"
    #include "url.h"

// see https://aticleworld.com/ssl-server-client-using-openssl-in-c/ for TLS 1.2 example

    #define _HTTP_HTTP 1
    #define maxInitialResponseSize 8192
    #define maxSegmentLength 4096

struct http_cookie_store *HTTP_globalCookieStore = NULL;

void HTTP_setGlobalCookieStore(struct http_cookie_store *store) {
    HTTP_globalCookieStore = store;
}

struct http_cookie_store *HTTP_getGlobalCookieStore() {
    return HTTP_globalCookieStore;
}

typedef void (*dataReceiveHandler)(void *);

typedef struct socket_info (*socketConnecter)(const char *, const char *, int, char *, char *, int);

typedef struct socket_info (*socketReader)(struct socket_info, char *, int);

typedef void (*socketCloser)(struct socket_info);


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
    result.response_description = makeStrCpy("OK");
    result.num_headers = 0;
    result.headers = NULL;
    result.is_chunked = 0;
    result.is_html = 0;
    result.content_length = i;
    result.error = 0;
    result.redirect = NULL;
    result.do_redirect = 0;

    struct http_data res;
    res.length = i;
    res.data = contents;
    result.response_body = res;

    return result;
}

struct http_response http_makeNetworkHTTPRequest(
    struct http_url *url,
    socketConnecter rwsocket,
    socketReader rsocket,
    socketCloser csocket,
    char *userAgent,
    dataReceiveHandler chunkHandler,
    dataReceiveHandler finishHandler,
    void *chunkArg
) {
    struct http_response errorResponse;
    errorResponse.error = 1;

    int curBufLen = maxInitialResponseSize;
    char *buffer = (char *) calloc(maxInitialResponseSize, sizeof(char)); // this should be dynamically returned by rwsocket, but 8kb is probably good for now
    char *ipBuffer = (char *) calloc(2048, sizeof(char)); // this should be dynamic, but 2kb is probably good

    char *cookieString = HTTP_cookieStoreToString(HTTP_globalCookieStore, url->hostname);
    char *baseString = (char *) calloc(64 + strlen(url->path) + strlen(url->hostname) + strlen(userAgent) + strlen(cookieString), sizeof(char));
    strcpy(baseString, "GET ");
    strcat(baseString, url->path);
    strcat(baseString, " HTTP/1.1\r\nHost: ");
    strcat(baseString, url->hostname);
    strcat(baseString, "\r\n");
    if (HTTP_globalCookieStore && HTTP_globalCookieStore->count) {
        strcat(baseString, "Cookie: ");
        strcat(baseString, cookieString);
        strcat(baseString, "\r\n");
    }
    strcat(baseString, "User-Agent: ");
    strcat(baseString, userAgent);
    strcat(baseString, "\r\n\r\n");
    char *requestString = makeStrCpy(baseString);

    int res = lookupIP(url->hostname, ipBuffer);
    if (res) {
        free(buffer);
        free(ipBuffer);
        if (res == HOST_NOT_FOUND) {
            errorResponse.error = 5;
            return errorResponse;
        }
        if (res == TRY_AGAIN) {
            errorResponse.error = 7;
            return errorResponse;
        }
        errorResponse.error = 6;
        return errorResponse;
    }

    errno = 0;
    struct socket_info tcpResult = rwsocket(ipBuffer, url->hostname, url->port, requestString, buffer, maxInitialResponseSize);
    free(ipBuffer);
    free(url->protocol);
    // Don't free url->hostname or url as the hostname is required for header parsing
    free(url->path);
    free(url->fragment);
    free(requestString);

    if (chunkHandler != NULL) chunkHandler(chunkArg);

    if (tcpResult.error) {
        switch(errno) {
            case 101:
              //  printf("Network unreachable\n");
                errorResponse.error = errno;
                free(buffer);
                return errorResponse;
            case 111:
              //  printf("Connection refused\n");
                errorResponse.error = errno;
                free(buffer);
                return errorResponse;
            case 113:
               // printf("No route to host\n");
                errorResponse.error = errno;
                free(buffer);
                return errorResponse;
        }
        switch (tcpResult.error) {
            case -6:
                errorResponse.error = 192;
                free(buffer);
                return errorResponse;
            case -7:
                errorResponse.error = 193;
                free(buffer);
                return errorResponse;
        }
        errorResponse.error = 200;
        free(buffer);
        return errorResponse;
        //printf("error: %d\n", errno);
    }

    // we can't realloc-shrink the buffer because it gets reused


    struct http_data initialHttpResponse;
    initialHttpResponse.length = tcpResult.bytesRead;
    initialHttpResponse.data = buffer;

    struct http_response parsedResponse = parsePossiblyIncompleteHTTPResponse(initialHttpResponse, "1.1");

    if (parsedResponse.error) {
        errorResponse.error = parsedResponse.error;
        csocket(tcpResult);
        return errorResponse;
    }

    // There's no guarantee that all the headers will be sent in the initial read
    while(!parsedResponse.has_body) {
        int initialBytesRead = tcpResult.bytesRead;
        char *currentPosition = buffer + initialBytesRead;
        tcpResult = rsocket(tcpResult, currentPosition, (maxInitialResponseSize - 8) - tcpResult.bytesRead);

        if (tcpResult.error) {
            errorResponse.error = 200;
            free(buffer);
            return errorResponse;
        }

        tcpResult.bytesRead = initialBytesRead + tcpResult.bytesRead;

        initialHttpResponse.length = tcpResult.bytesRead;
        parsedResponse = parsePossiblyIncompleteHTTPResponse(initialHttpResponse, "1.1");

        if (parsedResponse.error) {
            errorResponse.error = parsedResponse.error;
            csocket(tcpResult);
            return errorResponse;
        }
    }

    if (parsedResponse.error) {
        errorResponse.error = parsedResponse.error;
        csocket(tcpResult);
        return errorResponse;
    }

    if (parsedResponse.is_chunked) {
        int totalBytesRead = tcpResult.bytesRead;

        struct chunked_response_state chunkedResponse = parseChunkedResponse(parsedResponse.response_body);
        if (chunkedResponse.error) {
            errorResponse.error = 199;
            free(buffer);
            csocket(tcpResult);
            return errorResponse;
        }
        parsedResponse.response_body = chunkedResponse.current_parsed_data;

        char *currentPosition = buffer + totalBytesRead;
        while (!chunkedResponse.finished) {
            if (totalBytesRead + maxSegmentLength > curBufLen) {
                curBufLen *= 2;
                buffer = realloc(buffer, curBufLen);
            }

            currentPosition = buffer + totalBytesRead;
            errno = 0;
            tcpResult = rsocket(tcpResult, currentPosition, maxSegmentLength);
            totalBytesRead += tcpResult.bytesRead;

            if (chunkHandler != NULL) chunkHandler(chunkArg);

            initialHttpResponse.data = currentPosition;
            initialHttpResponse.length = tcpResult.bytesRead;
            appendChunkToBody(&chunkedResponse, initialHttpResponse);

            if (chunkedResponse.error) {
                errorResponse.error = 199;
                csocket(tcpResult);
                return errorResponse;
            }

            free(parsedResponse.response_body.data);
            parsedResponse.response_body = chunkedResponse.current_parsed_data;
        }
        free(chunkedResponse.current_chunked_data.data);
        if (finishHandler != NULL) finishHandler(chunkArg);
    } else if (parsedResponse.content_length == -2) {
        // This is pretty normal, no reason to warn
    } else {
        if (parsedResponse.content_length != parsedResponse.response_body.length) {
            int totalBytesRead = 0; // note; this implementation of a repeated read doesn't actually match up with the chunked response response length handling
            char *currentPosition;
            char *extra = NULL;

            while (parsedResponse.content_length > parsedResponse.response_body.length) {
                if (totalBytesRead + maxSegmentLength > curBufLen) {
                    curBufLen *= 2;
                    buffer = realloc(buffer, curBufLen);
                }

                if (extra) free(extra);

                currentPosition = buffer + totalBytesRead;
                errno = 0;
                tcpResult = rsocket(tcpResult, currentPosition, maxSegmentLength);
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

    csocket(tcpResult);

    for (int i = 0; i < parsedResponse.num_headers; i ++) {
        struct http_header header = parsedResponse.headers[i];
        char *name = toLowerCase(header.name);

        if (!strcmp(name, "set-cookie")) {
            HTTP_addCookieToStore(HTTP_globalCookieStore, HTTP_parseCookieString(header.value, url->hostname));
        }

        free(name);
    }
    free(url->hostname);
    free(url);

    return parsedResponse;
}

struct http_response http_makeHTTPRequest(char *charURL, char *userAgent, dataReceiveHandler chunkHandler, dataReceiveHandler finishHandler, void *chunkArg) {
    struct http_url *url = http_url_from_string(charURL);

    if (errno) {
        struct http_response failure;
        failure.error = 4;
        return failure;
    }

    if (!strcmp(url->protocol, "http")) {
        return http_makeNetworkHTTPRequest(url, rwsocket, rsocket, csocket, userAgent, chunkHandler, finishHandler, chunkArg);
    } else if (!strcmp(url->protocol, "https")) {
        return http_makeNetworkHTTPRequest(url, secure_rwsocket, secure_rsocket, secure_csocket, userAgent, chunkHandler, finishHandler, chunkArg);
    } else if (!strcmp(url->protocol, "file")) {
        free(url->hostname);
        free(url->protocol);
        struct http_response response = http_readFileToHTTP(url->path);
        free(url->path);
        free(url->fragment);
        free(url);

        return response;
    } else if (!strcmp(charURL, "about:blank")) {
        struct http_data body;
        body.length = 0;
        body.data = makeStrCpy("");

        struct http_response result;
        result.response_code = 200;
        result.response_description = makeStrCpy("OK");
        result.num_headers = 0;
        result.headers = NULL;
        result.is_chunked = 0;
        result.is_html = 0;
        result.content_length = 0;
        result.error = 0;
        result.redirect = NULL;
        result.do_redirect = 0;
        result.has_body = 1;
        result.response_body = body;

        return result;
    } else if (!strcmp(url->protocol, "about")) {
        struct http_data body;
        body.length = strlen("Invalid about: page");
        body.data = makeStrCpy("Invalid about: page");

        struct http_response result;
        result.response_code = 400;
        result.response_description = makeStrCpy("INVALID");
        result.num_headers = 0;
        result.headers = NULL;
        result.is_chunked = 0;
        result.is_html = 0;
        result.content_length = 0;
        result.error = 0;
        result.redirect = NULL;
        result.do_redirect = 0;
        result.has_body = 1;
        result.response_body = body;

        return result;
    } else {
        // Unsupported protocol
        struct http_response errorResponse;
        errorResponse.error = 194;
        return errorResponse;
    }
}

#endif
