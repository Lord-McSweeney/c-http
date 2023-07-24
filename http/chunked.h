#include <stdlib.h>
#include "response.h"

#ifndef _HTTP_CHUNKED
    #define _HTTP_CHUNKED 1

struct chunked_response_state {
    struct http_data current_chunked_data;
    struct http_data current_parsed_data;
    int finished;
    int was_incomplete;
    int error;
};


// was_incomplete gets set if the chunk data was not enough. In that case, you need to manually append another data chunk. For example:
/*
```
b\r\n
hello, wor
```

--> {
    curchunk: length=16 (the data)
    curdata: length=0 (parsing failure due to incomplete data)
    finished: 0
    was_incomplete: 1 
}


```
b\r\n
hello, world\r\n
7\r\n
goodby
```

--> {
    curchunk: length=26 (the data)
    curdata: length=12 ("hello, world")
    finished: 0
    was_incomplete: 1 
}
*/

/*
    error codes:
    1: bad chunk length
*/

enum _http_internal_chunk_parser_state {
    CHUNK_LENGTH,
    CHUNK_BODY,
};

struct chunked_response_state parseChunkedResponse(struct http_data body) {
    struct chunked_response_state errorStruct;
    errorStruct.error = 1;
    
    struct http_data currentParsedData;
    
    struct chunked_response_state responseStruct;
    responseStruct.finished = 0;
    responseStruct.current_chunked_data = body;
    responseStruct.was_incomplete = 0;
    responseStruct.error = 0;
    
    char *overallBody = (char *) calloc(body.length, sizeof(char));
    int overallBodyLength = 0;

    char *currentLength = (char *) calloc(body.length, sizeof(char)); // this should probably be optimized and made smaller, but eh
    int currentLen = 0;
    int finished = 0;
    
    int currentState = CHUNK_LENGTH;
    for (int i = 0; i < body.length; i ++) {
        char curChar = body.data[i];
        char nextChar;
        if (i + 1 < body.length) {
            nextChar = body.data[i + 1];
        } else {
            nextChar = '\0';
        }
        switch(currentState) {
            case CHUNK_LENGTH:
                if (curChar == '\r' && nextChar == '\n') {
                    currentLen = strtol(currentLength, NULL, 16);
                    if (currentLen == 0) { // the terminating 0-length chunk
                        finished = 1;
                        break;
                    }
                    
                    int length = strlen(currentLength);
                    for (int j = 0; j < length; j ++) {
                        currentLength[j] = '\0';
                    }
                    currentState = CHUNK_BODY;
                    i ++; // CR+LF, skip over the \n
                    break;
                }
                if ((curChar >= '0' && curChar <= '9') || (curChar >= 'a' && curChar <= 'z') || (curChar >= 'A' && curChar <= 'Z')) {
                    currentLength[strlen(currentLength)] = curChar;
                } else {
                    return errorStruct;
                }
                break;
            case CHUNK_BODY:
                if (currentLen > 0) {
                    overallBody[overallBodyLength] = curChar;
                    overallBodyLength ++;
                    currentLen --;
                    break;
                }
                if (curChar == '\r' && nextChar == '\n') {
                    currentState = CHUNK_LENGTH;
                    i ++; // CR+LF, skip over the \n
                    break;
                }
        }
        if (finished) {
            break;
        }
    }

    free(currentLength);

    currentParsedData.data = overallBody;
    currentParsedData.length = overallBodyLength;
    
    responseStruct.finished = finished;
    responseStruct.current_parsed_data = currentParsedData;
    if (currentLen > 0) {
        responseStruct.was_incomplete = 1;
    }
    
    return responseStruct;
}

void appendChunkToBody(struct chunked_response_state *state, struct http_data chunk) {
    struct chunked_response_state curState = *state;
    int newLength = curState.current_chunked_data.length + chunk.length;
    char *newData = (char *) calloc(newLength + 2, sizeof(char));
    memcpy(newData, curState.current_chunked_data.data, curState.current_chunked_data.length);
    memcpy(newData + curState.current_chunked_data.length, chunk.data, chunk.length);
    
    struct http_data data;
    data.length = newLength;
    data.data = newData;
    
    *state = parseChunkedResponse(data);
}

#endif
