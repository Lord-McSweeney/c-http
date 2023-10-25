#include "../utils/string.h"

#ifndef _HTML_ENTITY
    #define _HTML_ENTITY 1

char *XML_parseHTMLEntities(const char *content) {
    int numEscapes = 43;
    char entities[][8] = {
        "lt",     "<",
        "gt",     ">",
        "amp",    "&",
        "nbsp",   " ",
        "quot",   "\"",
        "copy",   "C",
        "laquo",  "<<",
        "raquo",  ">>",
        "ldquo",  "\"",
        "rdquo",  "\"",
        "rsaquo", ">",
        "hellip", "...",
        "mdash",  "---",
        "ndash",  "--",
        "reg",    "(R)",
        "larr",   "<--",
        "rarr",   "-->",
        "apos",   "'",
        "#x27",   "'",
        "#x2F",   "/",
        "#32",    " ",
        "#39",    "'",
        "#039",   "'",
        "#58",    ":",
        "#91",    "[",
        "#93",    "]",
        "#160",   " ",
        "#169",   "C",
        "#171",   "<<",
        "#187",   ">>",
        "#333",   "ō",
        "#914",   "B",
        "#946",   "B",
        "#8211",  "--",
        "#8212",  "---",
        "#8216",  "'",
        "#8217",  "'",
        "#8226",  "-",
        "#8230",  "...",
        "#8250",  ">",
        "#8260",  "/",
        "#9662",  "V",
        "#10094", "<",
    };


    int numSlashes = 0;
    for (int i = 0; i < strlen(content); i ++) {
        if (content[i] == '\\') {
            numSlashes ++;
        }
    }

    char *allocated = (char *) calloc(strlen(content) + numSlashes + 1, sizeof(char));
    int len = strlen(content);
    int realIndex = 0;
    int foundEscape = 0;
    for (int i = 0; i < len; i ++) {
        char curChar = content[i];

        if (curChar == '&') {
            foundEscape = 0;
            for (int j = (numEscapes * 2) - 1; j >= 0; j -= 2) {
                char *entity = entities[j - 1];
                char *replaceWith = entities[j];
                int entLen = strlen(entity);
                // i + 1 to skip over the '&'
                if (!strncmp(content + i + 1, entity, entLen)) {
                    int replaceLength = strlen(replaceWith);
                    int k;
                    for (k = 0; k < replaceLength; k ++) {
                        allocated[realIndex + k] = replaceWith[k];
                    }

                    i += entLen + 1;
                    realIndex += k;

                    if (content[i] != ';') i --;
                    foundEscape = 1;
                }
            }

            if (!foundEscape) {
                allocated[realIndex] = curChar;
                realIndex ++;
            }
        } else {
            allocated[realIndex] = curChar;
            realIndex ++;
        }
    }
    char *newText = makeStrCpy(allocated);
    free(allocated);
    return newText;
}

#endif
