#include "../utils/string.h"

#ifndef _HTTP_URL
    #define _HTTP_URL 1

struct http_url {
    char *protocol;
    char *hostname;
    short port;
    char *path;
    char *fragment;

    int had_protocol_slashes;
    int has_explicit_port;
    int had_explicit_path;
    int had_explicit_fragment;
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
    
    char *protocol = (char *) calloc(urllen + 2, sizeof(char));
    char *hostname = (char *) calloc(urllen + 2, sizeof(char));
    char *port = (char *) calloc(urllen + 6, sizeof(char));
    char *path = (char *) calloc(urllen + 2, sizeof(char));
    char *fragment = (char *) calloc(urllen + 1, sizeof(char));
    
    int currentState = HTTP_PROTOCOL;
    int currentIndex = 1;
    int isIp = 0;
    int isPortExplicit = 0;
    int hasProtocolSlashes = 1;
    int hasPath = 0;
    int hasFragment = 0;
    int didGetToProtocolEnd = 0;
    for (int i = 0; i < urllen; i ++) {
        char curChar = string[i];
        switch (currentState) {
            case HTTP_PROTOCOL:
                if (curChar == 0) {
                    errno = 1;
                    return NULL;
                }

                if (curChar == ':') {
                    didGetToProtocolEnd = 1;
                    currentState = HTTP_PROTOCOL_1;
                    currentIndex = 0;
                    break;
                }
                
                // Protocols cannot have periods inside them; if they do, turns out we were parsing a hostname 
                if (curChar == '.') {
                    didGetToProtocolEnd = 1;
                    i = -1;
                    free(protocol);
                    protocol = makeStrCpy("http");
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
                } else if (curChar >= '0' && curChar <= '9') {
                    // turns out we started with the hostname, and this is the port
                    i = -1;
                    free(protocol);
                    protocol = makeStrCpy("http");
                    currentState = HTTP_HOSTNAME;
                    currentIndex = 0;
                    break;
                } else {
                    hasProtocolSlashes = 0;
                    // welp, turns out we had a [protocol]:[hostname] instead of [protocol]://[hostname]
                    currentState = HTTP_HOSTNAME;
                    currentIndex = 0;
                    i --;
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
                if (curChar == '#') {
                    if (currentIndex == 1) {
                        errno = 2;
                        return NULL;
                    }

                    hasPath = 1;
                    hasFragment = 1;
                    currentState = HTTP_FRAGMENT;
                    currentIndex = 0;
                    break;
                }
                if (currentIndex == 1 && curChar >= '0' && curChar <= '9') {
                    isIp = 1;
                }
                if (isIp && ((curChar >= 'a' && curChar <= 'z') || (curChar >= 'A' && curChar <= 'Z'))) {
                    errno = 3;
                    return NULL;
                }
                if (curChar == ':') {
                    if (currentIndex == 1) {
                        errno = 3;
                        return NULL;
                    }
                    isPortExplicit = 1;
                    currentState = HTTP_PORT;
                    currentIndex = 0;
                    break;
                }
                if (curChar == '/') {
                    if (currentIndex == 1) {
                        errno = 3;
                        return NULL;
                    }
                    hasPath = 1;
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
                    hasFragment = 1;
                    currentState = HTTP_FRAGMENT;
                    currentIndex = 0;
                    break;
                }
                if (curChar == '%') {
                    if (urllen - i >= 2) {
                        char next1 = string[i + 1];
                        char next2 = string[i + 2];
                        if (next1 >= '0' && next1 <= '9' && next2 >= '0' && next2 <= '9') {
                            char *txt = (char *) calloc(3, sizeof(char));
                            txt[0] = next1;
                            txt[1] = next2;
                            long int num = strtol(txt, NULL, 16);
                            path[strlen(path)] = (char) num;
                            i += 2;
                            break;
                        }
                    }
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
        path = makeStrCpy("/");
    } else if (path[0] != '/') {
        memmove(path + 1, path, strlen(path));
        path[0] = '/';
    }

    if (!didGetToProtocolEnd) {
        hostname = protocol;
        protocol = makeStrCpy("http");
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
        hostname = makeStrCpy("localhost");
    }

    result_mem->protocol = protocol;
    result_mem->hostname = makeStrCpy(hostname);
    result_mem->port = atoi(port);
    result_mem->path = path;
    result_mem->fragment = fragment;
    result_mem->has_explicit_port = isPortExplicit;
    result_mem->had_protocol_slashes = hasProtocolSlashes;
    result_mem->had_explicit_path = hasPath;
    result_mem->had_explicit_fragment = hasFragment;

    free(hostname);
    free(port);

    return result_mem;
}

char *http_urlToString(struct http_url *url) {
    char *newAlloc = (char *) calloc(strlen(url->protocol) + 9 + strlen(url->hostname) + strlen(url->path) + strlen(url->fragment), sizeof(char));
    strcpy(newAlloc, url->protocol);
    if (url->had_protocol_slashes) {
        strcat(newAlloc, "://");
    } else {
        strcat(newAlloc, ":");
    }
    strcat(newAlloc, url->hostname);
    if (url->has_explicit_port) {
        strcat(newAlloc, ":");
        char *al2 = (char *) calloc(16, sizeof(char));
        sprintf(al2, "%d", url->port);
        strcat(newAlloc, al2);
        free(al2);
    }
    if (url->had_explicit_path) {
        strcat(newAlloc, url->path);
        if (url->had_explicit_fragment) {
            strcat(newAlloc, "#");
            strcat(newAlloc, url->fragment);
        }
    }
    return newAlloc;
}

char* http_resolveRelativeURL(struct http_url *base, char *baseString, char *string) {
    // NOTE: This function duplicates a lot of "build string from struct http_url" code. Could that be extracted into a new function?
    if (strlen(string) == 0) {
        return baseString;
    }
    if (string[0] == '/') {
        if (string[1] == '/') {
            char *newAlloc = (char *) calloc(strlen(base->protocol) + 3 + strlen(string), sizeof(char));
            strcpy(newAlloc, base->protocol);
            strcat(newAlloc, ":");
            strcat(newAlloc, string);
            return newAlloc;
        } else {
            char *newAlloc = (char *) calloc(strlen(base->protocol) + 8 + strlen(base->hostname) + strlen(string), sizeof(char));
            strcpy(newAlloc, base->protocol);
            if (base->had_protocol_slashes) {
                strcat(newAlloc, "://");
            } else {
                strcat(newAlloc, ":");
            }
            strcat(newAlloc, base->hostname);
            if (base->has_explicit_port) {
                strcat(newAlloc, ":");
                char *al2 = (char *) calloc(16, sizeof(char));
                sprintf(al2, "%d", base->port);
                strcat(newAlloc, al2);
                free(al2);
            }
            strcat(newAlloc, string);
            return newAlloc;
        }
    } else if (string[0] == '#') {
            char *newAlloc = (char *) calloc(strlen(base->protocol) + 12 + strlen(base->hostname) + strlen(base->path) + strlen(string), sizeof(char));
            strcpy(newAlloc, base->protocol);
            if (base->had_protocol_slashes) {
                strcat(newAlloc, "://");
            } else {
                strcat(newAlloc, ":");
            }
            strcat(newAlloc, base->hostname);
            if (base->has_explicit_port) {
                strcat(newAlloc, ":");
                char *al2 = (char *) calloc(16, sizeof(char));
                sprintf(al2, "%d", base->port);
                strcat(newAlloc, al2);
                free(al2);
            }
            strcat(newAlloc, base->path);
            strcat(newAlloc, string);
            return newAlloc;
    } else if (string[0] == '?') {
        // TODO
        return baseString;
    } else {
        int isAbsolute = 0;
        int wasNotAbsolute = 0;
        for (int i = 0; i < strlen(string); i ++) {
            char cur = string[i];
            if (cur == '?' || cur == '#') {
                wasNotAbsolute = 1;
            }
            if (cur == ':' && !wasNotAbsolute) {
                isAbsolute = 1;
            }
        }
        if (isAbsolute) {
            return string;
        } else {
            char *resultPath = (char *) calloc(strlen(baseString) + strlen(string) + 2, sizeof(char));
            int lastSlashPos = 0;
            for (int i = strlen(base->path) - 1; i >= 0; i --) {
                if (base->path[i] == '/') {
                    lastSlashPos = i;
                    break;
                }
            }
            strcpy(resultPath, base->path);
            strcpy(resultPath + lastSlashPos + 1, string);

            char *newAlloc = (char *) calloc(strlen(base->protocol) + 12 + strlen(base->hostname) + strlen(resultPath), sizeof(char));
            strcpy(newAlloc, base->protocol);
            if (base->had_protocol_slashes) {
                strcat(newAlloc, "://");
            } else {
                strcat(newAlloc, ":");
            }
            strcat(newAlloc, base->hostname);
            if (base->has_explicit_port) {
                strcat(newAlloc, ":");
                char *al2 = (char *) calloc(16, sizeof(char));
                sprintf(al2, "%d", base->port);
                strcat(newAlloc, al2);
                free(al2);
            }
            strcat(newAlloc, resultPath);
            return newAlloc;
        }
    }
}

#endif
