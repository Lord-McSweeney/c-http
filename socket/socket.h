#ifndef _HTTP_SOCK_H
    #define _HTTP_SOCK_H 1

        #include "common.h"

        #ifdef unix

            #include <arpa/inet.h>
            #include <errno.h>
            #include <string.h>
            #include <unistd.h>

            struct socket_info rwsocket(const char *address, const char *_hostname, int portnum, char *senddata, char *towrite, int amount) {
                struct socket_info errorStruct;
                errorStruct.descriptor = -1;
                errorStruct.bytesRead = -1;
                errorStruct.error = -1;
                errorStruct.extra = NULL;

                struct timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = 2500;
                int socket_desc;
                struct sockaddr_in server;
                char *server_reply = (char *) calloc(amount + 1, sizeof(char));

                socket_desc = socket(AF_INET, SOCK_STREAM, 0);
                if (socket_desc == -1) {
                    printf("Error while creating socket. Error code: \"%d\"\n", errno);
                    errorStruct.error = -1;
                    return errorStruct;
                }

                server.sin_addr.s_addr = inet_addr(address);
                server.sin_family = AF_INET;
                server.sin_port = htons(portnum);
                setsockopt(socket_desc, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

                while (1) {
                    if (connect(socket_desc, (struct sockaddr *) & server, sizeof(server)) < 0) {
                        if (errno == 115 || errno == 114) {
                            continue;
                        } else {/*
                            printf("Error while connecting to remote server. Error code: \"%d\"", errno);
                            if (errno == 111) {
                                printf(" (ERR_CONNECTION_REFUSED)");
                            }
                            if (errno == 101) {
                                printf(" (ERR_NETWORK_UNREACHABLE)");
                            }
                            printf("\n");*/
                            errorStruct.error = -2;
                            return errorStruct;
                        }
                        //printf("\n");
                    }
                    break;
                }

                if (send(socket_desc, senddata, strlen(senddata), 0) < 0) {
                    printf("Error while sending data to remote server. Error code: \"%d\"\n", errno);
                    errorStruct.error = -3;
                    return errorStruct;
                }

                int bytesRead = recv(socket_desc, server_reply, amount, 0);

                if (bytesRead < 0) {
                    printf("Error while recieving response from server. Error code: \"%d\"\n", errno);
                    errorStruct.error = -4;
                    return errorStruct;
                }

                strncpy(towrite, server_reply, amount);
                free(server_reply);

                struct socket_info returnValue;
                returnValue.descriptor = socket_desc;
                returnValue.bytesRead = bytesRead;
                returnValue.error = 0;
                returnValue.extra = NULL;
                return returnValue;
            }

            struct socket_info rsocket(struct socket_info info, char *towrite, int amount) {
                struct socket_info errorStruct;
                errorStruct.descriptor = -1;
                errorStruct.bytesRead = -1;
                errorStruct.error = -1;

                char *server_reply = (char *) calloc(amount + 1, sizeof(char));
                int bytesRead = recv(info.descriptor, server_reply, amount, 0);
                if (bytesRead < 0) {
                    printf("Error while recieving additional response from server. Error code: \"%d\"\n", errno);
                    return errorStruct;
                }

                strncpy(towrite, server_reply, amount);
                free(server_reply);

                struct socket_info returnValue;
                returnValue.descriptor = info.descriptor;
                returnValue.bytesRead = bytesRead;
                returnValue.error = 0;

                return returnValue;
            }

            void csocket(struct socket_info info) {
                close(info.descriptor);
            }

        #else

            struct socket_info rwsocket(const char *address, const char *_hostname, int _portnum, char *_senddata, char *_towrite, int _amount) {
                log_err("rwsocket called, not supported on non-Unix compilation target\n");

                struct socket_info errorStruct;
                errorStruct.descriptor = -1;
                errorStruct.bytesRead = -1;
                errorStruct.error = -1;
                errorStruct.extra = NULL;

                return errorStruct;
            }

            struct socket_info rsocket(struct socket_info info, char *_towrite, int _amount) {
                log_err("rsocket called, not supported on non-Unix compilation target\n");

                struct socket_info errorStruct;
                errorStruct.descriptor = info.descriptor;
                errorStruct.bytesRead = -1;
                errorStruct.error = -1;
                errorStruct.extra = NULL;

                return errorStruct;
            }

            void csocket(struct socket_info _info) { }

        #endif

#endif
