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
    free(len);

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
    return t < 33 && (t == ' ' || t == '\n' || t == '\t' || t == '\r');
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

struct split_string {
    char **result;
    int count;
};

struct split_string splitStringWithWhitespaceDelimiter(char *string) {
    struct split_string result;
    result.count = 0;
    result.result = NULL;

    int len = strlen(string);

    int spaceChunkSize = 16;

    int maxSpace = spaceChunkSize;
    char *currentSpace = (char *) calloc(maxSpace, sizeof(char));
    int usedSpace = 0;
    for (int i = 0; i < len; i ++) {
        char curChar = string[i];
        if (isWhiteSpace(curChar)) {
            if (currentSpace[0]) {
                result.count ++;
                result.result = (char **) realloc(result.result, result.count * sizeof(char *));
                result.result[result.count - 1] = makeStrCpy(currentSpace);
                clrStr(currentSpace);
                usedSpace = 0;
            }
        } else {
            if (usedSpace + spaceChunkSize/2 == maxSpace) {
                maxSpace += spaceChunkSize;
                currentSpace = realloc(currentSpace, maxSpace * sizeof(char));
                memset(currentSpace + (maxSpace - spaceChunkSize), 0, spaceChunkSize);
            }
            currentSpace[usedSpace] = curChar;
            usedSpace ++;
        }
    }
    if (currentSpace[0]) {
        result.count ++;
        result.result = (char **) realloc(result.result, result.count * sizeof(char *));
        result.result[result.count - 1] = makeStrCpy(currentSpace);
    }

    free(currentSpace);

    return result;
}

char *doubleStringBackslashes(char *string) {
    int len = strlen(string);
    int numBackslashes = 0;
    for (int i = 0; i < len; i ++) {
        if (string[i] == '\\') {
            numBackslashes ++;
        }
    }

    char *alloc = (char *) calloc(len + numBackslashes + 1, sizeof(char));
    int idx = 0;

    for (int i = 0; i < len; i ++) {
        if (string[i] == '\\') {
            alloc[idx] = string[i];
            alloc[idx + 1] = string[i];
            idx += 2;
        } else {
            alloc[idx] = string[i];
            idx ++;
        }
    }

    return alloc;
}

char *escapePercentages(char *string) {
    int len = strlen(string);
    int numPercents = 0;
    for (int i = 0; i < len; i ++) {
        if (string[i] == '%') {
            numPercents ++;
        }
    }

    char *alloc = (char *) calloc(len + numPercents + 1, sizeof(char));
    int idx = 0;

    for (int i = 0; i < len; i ++) {
        if (string[i] == '%') {
            alloc[idx] = string[i];
            alloc[idx + 1] = string[i];
            idx += 2;
        } else {
            alloc[idx] = string[i];
            idx ++;
        }
    }

    return alloc;
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

int stringContainsAny(char *string, char *possibilities) {
    int len = strlen(string);
    int iLen = strlen(possibilities);
    for (int i = 0; i < len; i ++) {
        for (int j = 0; j < iLen; j ++) {
            if (string[i] == possibilities[j]) {
                return 1;
            }
        }
    }
    return 0;
}

char *shortenStringWithEllipses(const char *string, unsigned int amount) {
    int len = strlen(string);
    char *cpy = makeStrCpy(string);
    if (amount >= len - 2) {
        return cpy;
    }
    cpy[amount] = '.';
    cpy[amount + 1] = '.';
    cpy[amount + 2] = '.';
    cpy[amount + 3] = '\0';
    return cpy;
}

char *toLowerCase(const char *text) {
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

char *toUpperCase(const char *text) {
    int len = strlen(text);
    char *allocated = (char *) calloc(len + 1, sizeof(char));
    for (int i = 0; i < len; i ++) {
        if (text[i] >= 'a' && text[i] <= 'z') {
            allocated[i] = text[i] - 32;
        } else {
            allocated[i] = text[i];
        }
    }
    return allocated;
}



#endif
