
#ifndef _SOCKET_GENERIC
    #define _SOCKET_GENERIC 1

struct socket_info {
    int descriptor;
    int bytesRead;
    int error;
    void *extra;
};

#endif
