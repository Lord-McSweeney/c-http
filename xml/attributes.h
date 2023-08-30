#include "../utils/string.h"

#include "html.h"

#ifndef _XML_ATTRIBUTE
    #define _XML_ATTRIBUTE 1

struct xml_attribute {
    char *name;
    char *value;
};

struct xml_attributes {
    struct xml_attribute *attributes;
    int count;
};

struct xml_attrib_result {
    struct xml_attributes *attribs;
    int error;
};

char *XML_getAttributeByName(struct xml_attributes *attribs, const char *name) {
    char *n = xml_toLowerCase(name);
    for (int i = 0; i < attribs->count; i ++) {
        if (!strcmp(attribs->attributes[i].name, n)) {
            free(n);
            return attribs->attributes[i].value;
        }
    }
    free(n);
    return NULL;
}

struct xml_attributes *XML_makeEmptyAttributes() {
    struct xml_attributes *empty = (struct xml_attributes *) calloc(1, sizeof(struct xml_attributes));
    empty->attributes = NULL;
    empty->count = 0;

    return empty;
}

void XML_appendAttribute(struct xml_attributes *attribs, struct xml_attribute attrib) {
    if (XML_getAttributeByName(attribs, attrib.name) != NULL) {
       return;
    }

    attribs->count ++;
    attribs->attributes = (struct xml_attribute *) realloc(attribs->attributes, sizeof(struct xml_attribute) * attribs->count);
    attribs->attributes[attribs->count - 1] = attrib;
}

struct xml_attribute XML_makeAttribute(const char *name, const char *value) {
    struct xml_attribute attrib;
    attrib.name = makeStrCpy(name);
    attrib.value = makeStrCpy(value);
    return attrib;
}

enum _xml_internal_node_aparser_state {
    APARSE_ATTRIBUTE_NAME,
    APARSE_ATTRIBUTE_SINGLE_QUOTED_VALUE,
    APARSE_ATTRIBUTE_DOUBLE_QUOTED_VALUE,
    APARSE_ATTRIBUTE_VALUE,
    APARSE_UNKNOWN,
};

void freeXMLAttributes(struct xml_attributes *attribs) {
    for (int i = 0; i < attribs->count; i ++) {
        free(attribs->attributes[i].name);
        free(attribs->attributes[i].value);
    }
    free(attribs->attributes);
    free(attribs);
}

char *XML_parseHTMLEscapes(const char *content) {
    int numEscapes = 33;
    char entities[][8] = {
        "lt",     "<",
        "gt",     ">",
        "amp",    "&",
        "nbsp",   " ",
        "quot",   "\"",
        "copy",   "C",
        "raquo",  ">>",
        "ldquo",  "\"",
        "rdquo",  "\"",
        "hellip", "...",
        "mdash",  "---",
        "ndash",  "--",
        "reg",    "(R)",
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
        "#914",   "B",
        "#946",   "B",
        "#8211",  "--",
        "#8212",  "---",
        "#8216",  "'",
        "#8217",  "'",
        "#8226",  "-",
        "#8230",  "...",
        "#8250",  ">",
        "#8260",  "/"
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

// Error codes:
/*
    1: Invalid XML
    2: XML too large
*/
struct xml_attrib_result XML_parseAttributes(char *inputString) {
    struct xml_attrib_result errResult;
    errResult.error = 1;
    
    struct xml_attributes attribs;
    attribs.count = 0;
    attribs.attributes = NULL;

    int currentState = APARSE_UNKNOWN;
    int currentIndex = 1;

    struct xml_attribute currentAttrib;
    currentAttrib.name = NULL;
    currentAttrib.value = NULL;

    char *currentDataContent = (char *) calloc(4096, sizeof(char));
    int currentDataUsage = 0;

    int hasStartedValue = 0;

    int len = strlen(inputString);

    for (int i = 0; i < len; i ++) {
        char curChar = inputString[i];
        switch(currentState) {
            case APARSE_ATTRIBUTE_NAME:
                if (curChar == '=') {
                    currentAttrib.name = xml_toLowerCase(currentDataContent);
                    currentDataUsage = 0;
                    clrStr(currentDataContent);

                    currentIndex = 0;
                    currentState = APARSE_ATTRIBUTE_VALUE;
                } else if (curChar == ' ') {
                    currentAttrib.name = xml_toLowerCase(currentDataContent);
                    currentAttrib.value = makeStrCpy("");
                    currentDataUsage = 0;
                    clrStr(currentDataContent);

                    XML_appendAttribute(&attribs, currentAttrib);
                    currentAttrib.name = NULL;
                    currentAttrib.value = NULL;


                    currentIndex = 0;
                    currentState = APARSE_UNKNOWN;
                } else {
                    if (currentDataUsage > 4094) {
                        free(currentDataContent);
    
                        errResult.error = 2;
                        return errResult;
                    }
                    currentDataContent[currentDataUsage] = curChar;
                    currentDataUsage ++;
                }
                break;
            case APARSE_ATTRIBUTE_SINGLE_QUOTED_VALUE:
                if (curChar == '\'') {
                    currentAttrib.value = XML_parseHTMLEscapes(currentDataContent);
                    currentDataUsage = 0;
                    clrStr(currentDataContent);

                    XML_appendAttribute(&attribs, currentAttrib);
                    currentAttrib.name = NULL;
                    currentAttrib.value = NULL;

                    currentIndex = 0;
                    currentState = APARSE_UNKNOWN;
                } else {
                    if (currentDataUsage > 4094) {
                        free(currentDataContent);
    
                        errResult.error = 2;
                        return errResult;
                    }
                    currentDataContent[currentDataUsage] = curChar;
                    currentDataUsage ++;
                }
                break;
            case APARSE_ATTRIBUTE_DOUBLE_QUOTED_VALUE:
                if (curChar == '"') {
                    currentAttrib.value = XML_parseHTMLEscapes(currentDataContent);
                    currentDataUsage = 0;
                    clrStr(currentDataContent);

                    XML_appendAttribute(&attribs, currentAttrib);
                    currentAttrib.name = NULL;
                    currentAttrib.value = NULL;

                    currentIndex = 0;
                    currentState = APARSE_UNKNOWN;
                } else {
                    if (currentDataUsage > 4094) {
                        free(currentDataContent);
    
                        errResult.error = 2;
                        return errResult;
                    }
                    currentDataContent[currentDataUsage] = curChar;
                    currentDataUsage ++;
                }
                break;
            case APARSE_ATTRIBUTE_VALUE:
                if (currentIndex == 1) {
                    hasStartedValue = 0;
                }
                if (curChar == '"' && currentIndex == 1) {
                    currentIndex = 0;
                    currentState = APARSE_ATTRIBUTE_DOUBLE_QUOTED_VALUE;
                    hasStartedValue = 1;
                } else if (curChar == '\'' && currentIndex == 1) {
                    currentIndex = 0;
                    currentState = APARSE_ATTRIBUTE_SINGLE_QUOTED_VALUE;
                    hasStartedValue = 1;
                } else if (curChar == ' ') {
                    if (!hasStartedValue) {
                        continue;
                    }

                    currentAttrib.value = XML_parseHTMLEscapes(currentDataContent);
                    currentDataUsage = 0;
                    clrStr(currentDataContent);

                    currentIndex = 0;
                    currentState = APARSE_UNKNOWN;

                    XML_appendAttribute(&attribs, currentAttrib);
                    currentAttrib.name = NULL;
                    currentAttrib.value = NULL;
                } else {
                    hasStartedValue = 1;
                    if (currentDataUsage > 4094) {
                        free(currentDataContent);
    
                        errResult.error = 2;
                        return errResult;
                    }
                    currentDataContent[currentDataUsage] = curChar;
                    currentDataUsage ++;
                }
                break;
            case APARSE_UNKNOWN:
                if (curChar != ' ' && curChar != '\n' && curChar != '\r') {
                    if (curChar == '=' || curChar == '\'' || curChar == '"') {
                        free(currentDataContent);
                        return errResult;
                    } else {
                        currentIndex = 0;
                        currentState = APARSE_ATTRIBUTE_NAME;
                        i --;
                    }
                }
                break;
        }
        currentIndex ++;
    }

    if (strlen(currentDataContent)) {
        switch(currentState) {
            case APARSE_ATTRIBUTE_VALUE:
                currentDataUsage = 0;
                struct xml_attribute attrib1 = XML_makeAttribute(xml_toLowerCase(currentAttrib.name), makeStrCpy(currentDataContent));
                XML_appendAttribute(&attribs, attrib1);
                clrStr(currentDataContent);
                break;
            case APARSE_ATTRIBUTE_NAME:
                currentDataUsage = 0;
                struct xml_attribute attrib2 = XML_makeAttribute(makeStrCpy(currentDataContent), makeStrCpy(""));
                XML_appendAttribute(&attribs, attrib2);
                clrStr(currentDataContent);
                break;
        }
    }

    if (currentAttrib.name) free(currentAttrib.name);

    free(currentDataContent);

    struct xml_attrib_result successResult;
    successResult.error = 0;
    successResult.attribs = (struct xml_attributes *) calloc(1, sizeof(struct xml_attributes));
    memcpy(successResult.attribs, &attribs, sizeof(struct xml_attributes));
    return successResult;
}

#endif
