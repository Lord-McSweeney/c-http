#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef _UTILS_STRING
    #define _UTILS_STRING 1

char *safeEncodeString(char *string) {
    char *result = (char *) calloc(strlen(string) + 16, sizeof(char));
    char *len = (char *) calloc(16, sizeof(char));
    sprintf(len, "%ld", strlen(string));
    sprintf(result, "%ld", strlen(len));
    sprintf(result + strlen(result), "%ld", strlen(string));
    sprintf(result + strlen(result), "%s", string);
    return result;
}

char *safeDecodeString(char *string) {
    char *result = (char *) calloc(strlen(string) + 2, sizeof(char));

    int lenLen = string[0] - 48;
    string = string + 1;

    char *len2 = (char *) calloc(strlen(string) + 2, sizeof(char));
    strncpy(len2, string, lenLen);

    int len = atoi(len2);

    string = string + lenLen;

    free(len2);

    strncpy(result, string, len);

    return result;
}

int safeGetEncodedStringLength(char *string) {
    int lenLen = string[0] - 48;

    char *len2 = (char *) calloc(strlen(string) + 2, sizeof(char));
    strncpy(len2, string + 1, lenLen);

    int len = atoi(len2);

    free(len2);

    return 1 + lenLen + len;
}

char *makeStrCpy(const char *string) {
    char *destString = (char *) calloc(strlen(string) + 1, sizeof(char));
    strcpy(destString, string);
    return destString;
}

void clrStr(char *string) {
    int len = strlen(string);
    for (int i = 0; i < len; i ++) {
        string[i] = 0;
    }
}

int isWhiteSpace(char t) {
    return t == ' ' || t == '\n' || t == '\t' || t == '\r';
}

char *trimString(char *string) {
    int isAtStringYet = 0;
    int len = strlen(string);

    char *res = (char *) calloc(len + 1, sizeof(char));
    int idx = 0;

    for (int i = 0; i < len; i ++) {
        if (!isWhiteSpace(string[i])) {
            isAtStringYet = 1;
        }
        if (isAtStringYet) {
            int foundChars = 1;
            if (isWhiteSpace(string[i])) {
                foundChars = 0;
                for (int j = i; j < len; j ++) {
                    if (!isWhiteSpace(string[j])) {
                        foundChars = 1;
                    }
                }
            }
            if (foundChars) {
                res[idx] = string[i];
                idx ++;
            } else {
                break;
            }
        }
    }

    return makeStrCpy(res);
}

int stringContains(char *string, char t) {
    int len = strlen(string);
    for (int i = 0; i < len; i ++) {
        if (string[i] == t) {
            return 1;
        }
    }
    return 0;
}

#endif
