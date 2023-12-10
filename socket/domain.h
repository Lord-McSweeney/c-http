#ifndef _RESOLVE_DOMAIN_H
    #define _RESOLVE_DOMAIN_H 1

    #ifdef unix
        #include <arpa/inet.h>
        #include <netdb.h>
        #include <string.h>

        int lookupIP(char *hostname, char *buffer) {
            struct hostent *he;
            struct in_addr **addr_list;
            int i;

            if ((he = gethostbyname(hostname)) == NULL) {
	            return h_errno;
            }

            addr_list = (struct in_addr **) he->h_addr_list;

            for (i = 0; addr_list[i] != NULL; i ++) {
	            strcpy(buffer, inet_ntoa(*addr_list[i]));
            }

            return 0;
        }

    #else
        #ifndef TRY_AGAIN
            #define TRY_AGAIN 2
        #endif

        int lookupIP(char *_hostname, char *_buffer) {
            return TRY_AGAIN;
        }

    #endif
#endif
