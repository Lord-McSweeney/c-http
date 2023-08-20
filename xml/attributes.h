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
    attrib.name = XML_makeStrCpy(name);
    attrib.value = XML_makeStrCpy(value);
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

    int len = strlen(inputString);

    for (int i = 0; i < len; i ++) {
        char curChar = inputString[i];
        switch(currentState) {
            case APARSE_ATTRIBUTE_NAME:
                if (curChar == '=') {
                    currentAttrib.name = xml_toLowerCase(currentDataContent);
                    currentDataUsage = 0;
                    XML_clrStr(currentDataContent);

                    currentIndex = 0;
                    currentState = APARSE_ATTRIBUTE_VALUE;
                } else if (curChar == ' ') {
                    currentAttrib.name = xml_toLowerCase(currentDataContent);
                    currentAttrib.value = XML_makeStrCpy("");
                    currentDataUsage = 0;
                    XML_clrStr(currentDataContent);

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
                    currentAttrib.value = XML_makeStrCpy(currentDataContent);
                    currentDataUsage = 0;
                    XML_clrStr(currentDataContent);

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
                    currentAttrib.value = XML_makeStrCpy(currentDataContent);
                    currentDataUsage = 0;
                    XML_clrStr(currentDataContent);

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
                if (curChar == '"' && currentIndex == 1) {
                    currentIndex = 0;
                    currentState = APARSE_ATTRIBUTE_DOUBLE_QUOTED_VALUE;
                } else if (curChar == '\'' && currentIndex == 1) {
                    currentIndex = 0;
                    currentState = APARSE_ATTRIBUTE_SINGLE_QUOTED_VALUE;
                } else if (curChar == ' ') {
                    if (currentIndex == 1) {
                        // this should be unreachable
                    } else {
                        currentAttrib.value = XML_makeStrCpy(currentDataContent);
                        currentDataUsage = 0;
                        XML_clrStr(currentDataContent);

                        currentIndex = 0;
                        currentState = APARSE_UNKNOWN;
                    }

                    XML_appendAttribute(&attribs, currentAttrib);
                    currentAttrib.name = NULL;
                    currentAttrib.value = NULL;
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
                struct xml_attribute attrib1 = XML_makeAttribute(xml_toLowerCase(currentAttrib.name), XML_makeStrCpy(currentDataContent));
                XML_appendAttribute(&attribs, attrib1);
                XML_clrStr(currentDataContent);
                break;
            case APARSE_ATTRIBUTE_NAME:
                currentDataUsage = 0;
                struct xml_attribute attrib2 = XML_makeAttribute(XML_makeStrCpy(currentDataContent), XML_makeStrCpy(""));
                XML_appendAttribute(&attribs, attrib2);
                XML_clrStr(currentDataContent);
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
