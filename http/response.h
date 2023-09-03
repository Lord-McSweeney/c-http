#include <errno.h>

#include "../utils/string.h"

#ifndef _HTTP_RESPONSE
    #define _HTTP_RESPONSE 1

char *HTTP_toLowerCase(const char *text) {
    int len = strlen(text);
    char *allocated = (char *) calloc(len + 1, sizeof(char));
    for (int i = 0; i < len; i ++) {
        if (text[i] >= 'A' && text[i] <= 'Z') {
            allocated[i] = text[i] + 32;
        } else {
            allocated[i] = text[i];
        }
    }
    return allocated;
}

enum _http_internal_header_type {
    HEADER_NAME_VALUE,
    HEADER_ONLY_NAME,
};

enum _http_internal_response_parser_state {
    PARSE_HTTP_VERSION,
    PARSE_HTTP_STATUS,
    PARSE_HTTP_STATUS_NAME,
    PARSE_HTTP_HEADER_NAME,
    PARSE_HTTP_HEADER_SEPARATOR,
    PARSE_HTTP_HEADER_VALUE,
    PARSE_HTTP_BODY,
};

struct http_header {
    char *name;
    char *value;
    int type;
};

struct http_data {
    char *data;
    int length;
};

struct http_response {
    int response_code;
    char *response_description;
    int num_headers;
    struct http_header *headers;
    struct http_data response_body;
    int is_chunked;
    int is_html;
    int content_length;
    char *redirect;
    int do_redirect;
    int has_body;
    int error;
};

struct http_response HTTP_responseFromString(char *string, int is_html) {
    struct http_data data;
    data.length = strlen(string);
    data.data = string;

    struct http_response res;
    res.response_code = 200;
    res.response_description = makeStrCpy("OK");
    res.num_headers = 0;
    res.headers = NULL;
    res.response_body = data;
    res.is_chunked = 0;
    res.is_html = is_html;
    res.content_length = strlen(string);
    res.redirect = NULL;
    res.do_redirect = 0;
    res.has_body = 1;
    res.error = 0;

    return res;
}

// error codes:
/*
0: no error
1: malformed http response
2: HTTP version did not match
3: malformed http status
4: URL parse error
5: Failed to lookup host
6: Error while obtaining host IP
*/

/*
A content length of -1 indicates an error.
A content length of -2 indicates a response where the Content-Length header was not present (possibly chunked).
*/

struct http_response parsePossiblyIncompleteHTTPResponse(struct http_data rawResponse, char *expectedVersion) {
    struct http_response response;
    response.error = 0;
    response.is_chunked = 0;
    response.is_html = 0;
    response.content_length = -1;
    response.redirect = NULL;
    response.do_redirect = 0;
    response.has_body = 0;

    if (rawResponse.length < 7 || strncmp(rawResponse.data, "HTTP/", 5)) {
        response.error = 195;
        return response;
    }

    struct http_data response_body;
    response_body.length = 0;
    response_body.data = "";

    char *statusCode = (char *) calloc(5, sizeof(char));
    char *statusName = (char *) calloc(rawResponse.length - 7, sizeof(char));
    char *body = (char *) calloc(rawResponse.length - 7, sizeof(char));
    int bodyLength = 0;
    struct http_header *headers = (struct http_header *) calloc(0, sizeof(struct http_header));

    int currentState = PARSE_HTTP_VERSION;
    int currentIndex = 0;
    int numHeaders = 0;
    int contentLengthNotReturned = 1;
    for (int i = 0; i < rawResponse.length; i ++) {
        char curChar = rawResponse.data[i];

        char nextChar;
        if (i + 1 < rawResponse.length) {
            nextChar = rawResponse.data[i + 1];
        } else {
            nextChar = '\0';
        }
        switch(currentState) {
            case PARSE_HTTP_VERSION:
                if (currentIndex == 0) {
                    if (strncmp(rawResponse.data + i, "HTTP/", 5)) {
                        response.error = 1;
                        return response;
                    } else {
                        i += 5;
                        currentIndex = 5;
                    }
                } else if (currentIndex == 5) {
                    if (strncmp(rawResponse.data + i, expectedVersion, 3)) {
                        response.error = 2;
                        return response;
                    } else {
                        i += 3;
                        currentIndex = 8;
                    }
                } else if (currentIndex == 8) {
                    if (curChar != ' ') {
                        response.error = 1;
                        return response;
                    } else {
                        currentState = PARSE_HTTP_STATUS;
                        currentIndex = 0;
                    }
                }
                break;
            case PARSE_HTTP_STATUS:
                if (currentIndex == 4) {
                    if (curChar == ' ') {
                        currentState = PARSE_HTTP_STATUS_NAME;
                        currentIndex = 0;
                        break;
                    } else {
                        response.error = 3;
                        return response;
                    }
                }
                if (curChar < '0' || curChar > '9') {
                    response.error = 3;
                    return response;
                }
                statusCode[strlen(statusCode)] = curChar;
                break;
            case PARSE_HTTP_STATUS_NAME:
                if (curChar == '\n') {
                    currentState = PARSE_HTTP_HEADER_NAME;
                    currentIndex = 0;
                    break;
                }
                statusName[strlen(statusName)] = curChar;
                break;
            case PARSE_HTTP_HEADER_NAME:
                if (curChar == '\r' && nextChar == '\n') {
                    if (currentIndex == 1) {
                        currentState = PARSE_HTTP_BODY;
                        currentIndex = 0;
                        response.has_body = 1;
                        i ++; // CR+LF
                        break;
                    } else {
                        headers[numHeaders - 1].type = HEADER_ONLY_NAME;
                        currentState = PARSE_HTTP_HEADER_NAME;
                        currentIndex = 0;
                        i ++;
                        break;
                    }
                }
                if (curChar == ':') {
                    headers[numHeaders - 1].type = HEADER_NAME_VALUE;
                    currentState = PARSE_HTTP_HEADER_SEPARATOR;
                    currentIndex = 0;
                    break;
                }
                int header_index = numHeaders - 1;
                if (currentIndex == 1) {
                    numHeaders ++;
                    header_index = numHeaders - 1;
                    headers = (struct http_header *) realloc(headers, sizeof(struct http_header) * numHeaders);
                    headers[header_index].name = (char *) calloc(rawResponse.length - 7, sizeof(char));
                    headers[header_index].value = NULL;
                }
                char *nameString = headers[header_index].name;
                nameString[strlen(nameString)] = curChar;
                break;
            case PARSE_HTTP_HEADER_SEPARATOR:
                if (curChar == ' ') {
                    currentState = PARSE_HTTP_HEADER_SEPARATOR;
                    // setting currentindex is unnecessary here
                } else {
                    i = i - 1;
                    currentState = PARSE_HTTP_HEADER_VALUE;
                    currentIndex = 0;
                }
                break;
            case PARSE_HTTP_HEADER_VALUE:
                if (curChar == '\r' && nextChar == '\n') {
                    char *lowerName = HTTP_toLowerCase(headers[numHeaders - 1].name);

                    if (!strcmp(lowerName, "transfer-encoding")) {
                        if (!strcmp(headers[numHeaders - 1].value, "chunked")) {
                            response.is_chunked = 1;
                        }
                    }
                    if (!strcmp(lowerName, "content-type")) {
                        if (!strncmp(headers[numHeaders - 1].value, "text/html", 9)) {
                            response.is_html = 1;
                        }
                    }
                    if (!strcmp(lowerName, "content-length")) {
                        response.content_length = atoi(headers[numHeaders - 1].value);
                        contentLengthNotReturned = 0;
                    }
                    if (!strcmp(lowerName, "location")) {
                        int stat = atoi(statusCode);
                        if (stat == 301 || stat == 302 || stat == 303 || stat == 307 || stat == 308) {
                            response.redirect = headers[numHeaders - 1].value;
                            response.do_redirect = 1;
                        }
                    }
                    free(lowerName);
                    currentState = PARSE_HTTP_HEADER_NAME;
                    currentIndex = 0;
                    i ++; // aaagh CR+LF
                    break;
                }
                int value_index = numHeaders - 1;
                if (currentIndex == 1) {
                    headers[header_index].value = (char *) calloc(rawResponse.length - 7, sizeof(char));
                }
                char *valueString = headers[value_index].value;
                valueString[strlen(valueString)] = curChar;
                break;
            case PARSE_HTTP_BODY:
                body[bodyLength] = curChar;
                bodyLength ++;
                break;
        }
        currentIndex ++;
    }

    response.response_code = atoi(statusCode);
    free(statusCode);

    response.response_description = statusName;
    if (contentLengthNotReturned) {
        response.content_length = -2;
    }
    response.headers = headers;
    response.num_headers = numHeaders;


    response_body.data = body;
    response_body.length = bodyLength;

    response.response_body = response_body;
    return response;
}

#endif
