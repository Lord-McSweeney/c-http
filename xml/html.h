#include <string.h>

#ifndef _XML_HTML
    #define _XML_HTML 1

#define html_numVoidElements 18
#define html_numChildlessElements 2
#define html_numClosingElements 2
char html_voidElements[][16] = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source", "track", "wbr", "command", "keygen", "menuitem", "frame"};
char html_closingElements[][8] = {"p", "li"};
char html_childlessElements[][8] = {"script", "style"};

struct xml_data {
    char *data;
    int length;
};

enum xml_node_type {
    NODE_DOCTYPE, // special DOCTYPE
    NODE_TEXT,
    NODE_ELEMENT,
    NODE_COMMENT,
};

enum _xml_internal_node_parser_state {
    PARSE_ELEMENT_NAME,
    PARSE_ELEMENT_END_SLASH,
    PARSE_ELEMENT_SLASH_END,
    PARSE_ELEMENT_TEXT,
    PARSE_ELEMENT_COMMENT,
    PARSE_DOCTYPE,
    PARSE_COMMENT,
    PARSE_UNKNOWN,
};

struct xml_list {
    struct xml_node *nodes;
    int count;
    int error;
};

struct xml_node {
    char *name;         // should only be present for element nodes
    char *text_content; // should only be present for text nodes/doctype nodes
    struct xml_list children;
    enum xml_node_type type;
};

struct xml_response {
    struct xml_list list;
    int error;
    int bytesParsed;
};

char *xml_toLowerCase(char *text) {
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

struct xml_node XML_createXMLElement(char *name) {
    struct xml_node node;
    node.name = name;
    node.text_content = NULL;
    
    struct xml_list children;
    children.count = 0;
    children.nodes = (struct xml_node *) calloc(0, sizeof(struct xml_node));
    
    node.children = children;
    node.type = NODE_ELEMENT;
    return node;
}

struct xml_node XML_createXMLText(char *text_content) {
    struct xml_node node;
    node.name = NULL;
    node.text_content = text_content;
    
    struct xml_list children;
    children.count = 0;
    children.nodes = (struct xml_node *) calloc(0, sizeof(struct xml_node));
    
    node.children = children;
    node.type = NODE_TEXT;
    return node;
}

struct xml_node XML_createDoctype(char *text_content) {
    struct xml_node node;
    node.name = NULL;
    node.text_content = text_content;
    
    struct xml_list children;
    children.count = 0;
    children.nodes = (struct xml_node *) calloc(0, sizeof(struct xml_node));
    
    node.children = children;
    node.type = NODE_DOCTYPE;
    return node;
}

struct xml_node XML_createComment(char *text_content) {
    struct xml_node node;
    node.name = NULL;
    node.text_content = text_content;
    
    struct xml_list children;
    children.count = 0;
    children.nodes = (struct xml_node *) calloc(0, sizeof(struct xml_node));
    
    node.children = children;
    node.type = NODE_COMMENT;
    return node;
}

struct xml_list XML_createEmptyXMLList() {
    struct xml_list list;
    list.count = 0;
    list.nodes = (struct xml_node *) calloc(0, sizeof(struct xml_node));
    return list;
}

char *XML_makeStrCpy(char *string) {
    char *destString = (char *) calloc(strlen(string) + 1, sizeof(string));
    strcpy(destString, string);
    return destString;
}

void XML_clrStr(char *string) {
    int len = strlen(string);
    for (int i = 0; i < len; i ++) {
        string[i] = 0;
    }
}

void XML_appendChild(struct xml_list *parent, struct xml_node child) {
    parent->count ++;
    parent->nodes = (struct xml_node *) realloc(parent->nodes, sizeof(struct xml_node) * parent->count);
    parent->nodes[parent->count - 1] = child;
}

char *XML_nodeTypeToString(enum xml_node_type type) {
    if (type == NODE_DOCTYPE) {
        return XML_makeStrCpy("doctype");
    }
    if (type == NODE_TEXT) {
        return XML_makeStrCpy("text");
    }
    if (type == NODE_ELEMENT) {
        return XML_makeStrCpy("element");
    }
    if (type == NODE_COMMENT) {
        return XML_makeStrCpy("comment");
    }
    return XML_makeStrCpy("!CORRUPT!");
}

int html_isVoidElement(char *name) {
    int i = html_numVoidElements - 1;
    while (i >= 0) {
        if (!strcmp(xml_toLowerCase(name), html_voidElements[i])) {
            return 1;
        }
        i --;
    }
    return 0;
}

int html_isClosingElement(char *name) {
    int i = html_numClosingElements - 1;
    while (i >= 0) {
        if (!strcmp(xml_toLowerCase(name), html_closingElements[i])) {
            return 1;
        }
        i --;
    }
    return 0;
}

int html_isChildlessElement(char *name) {
    int i = html_numChildlessElements - 1;
    while (i >= 0) {
        if (!strcmp(xml_toLowerCase(name), html_childlessElements[i])) {
            return 1;
        }
        i --;
    }
    return 0;
}

struct xml_data XML_xmlDataFromString(char *string) {
    struct xml_data data;
    data.data = string;
    data.length = strlen(string);
    return data;
}

/*
error codes:
0: no error
1: generic error
2: xml too large
3: xml parsing failure: encountered a `...</`
*/

struct xml_response recursive_parse_xml_node(struct xml_data xml_string, char *closingTag, char *parentClosingTag) {
    struct xml_response error_xml;
    error_xml.error = 1;

    if (xml_string.length == 0) {
        struct xml_response response;
        response.list = XML_createEmptyXMLList();
        return response;
    }
    
    struct xml_list response;
    response.count = 0;
    response.nodes = (struct xml_node *) calloc(0, sizeof(struct xml_node));
    
    int currentState = PARSE_UNKNOWN;
    int currentIndex = 1;
    
    char *currentTextContent = (char *) calloc(131072, sizeof(char));
    char *currentDoctypeContent = (char *) calloc(256, sizeof(char));
    char *currentElementName = (char *) calloc(256, sizeof(char));
    
    int currentTextContentUsage = 0;
    int currentDoctypeContentUsage = 0;
    int currentElementNameUsage = 0;
    
    int startedParsingName = 0;
    int doneParsingName = 0;
    int isEscapeEnabled = 0;
    int isInsideString = 0;
    
    int bytesParsed = 0;
    
    for (int i = 0; i < xml_string.length; i ++) {
        char curChar = xml_string.data[i];
        char nextChar;
        if (i + 1 < xml_string.length) {
            nextChar = xml_string.data[i + 1];
        } else {
            nextChar = '\0';
        }
        char nextChar2;
        if (i + 2 < xml_string.length) {
            nextChar2 = xml_string.data[i + 2];
        } else {
            nextChar2 = '\0';
        }
        char nextChar3;
        if (i + 3 < xml_string.length) {
            nextChar3 = xml_string.data[i + 3];
        } else {
            nextChar3 = '\0';
        }
        char nextChar4;
        if (i + 4 < xml_string.length) {
            nextChar4 = xml_string.data[i + 4];
        } else {
            nextChar4 = '\0';
        }
        char nextChar5;
        if (i + 5 < xml_string.length) {
            nextChar5 = xml_string.data[i + 5];
        } else {
            nextChar5 = '\0';
        }
        char nextChar6;
        if (i + 6 < xml_string.length) {
            nextChar6 = xml_string.data[i + 6];
        } else {
            nextChar6 = '\0';
        }
        char nextChar7;
        if (i + 7 < xml_string.length) {
            nextChar7 = xml_string.data[i + 7];
        } else {
            nextChar7 = '\0';
        }
        char nextChar8;
        if (i + 8 < xml_string.length) {
            nextChar8 = xml_string.data[i + 8];
        } else {
            nextChar8 = '\0';
        }
        
        switch(currentState) {
            case PARSE_ELEMENT_TEXT:
                if (curChar == '<') {
                    int isScriptTag = (nextChar == '/' && nextChar2 == 's' && nextChar3 == 'c' && nextChar4 == 'r' && nextChar5 == 'i' && nextChar6 == 'p' && nextChar7 == 't' && nextChar8 == '>');
                    int isStyleTag = (nextChar == '/' && nextChar2 == 's' && nextChar3 == 't' && nextChar4 == 'y' && nextChar5 == 'l' && nextChar6 == 'e' && nextChar7 == '>');
                    
                    // why, HTML?
                    if ((strcmp(closingTag, "script") || isScriptTag) && (strcmp(closingTag, "style") || isStyleTag)) {
                        struct xml_node node = XML_createXMLText(XML_makeStrCpy(currentTextContent));
                        XML_appendChild(&response, node);
                        currentTextContentUsage = 0;
                        XML_clrStr(currentTextContent);
                        currentIndex = 0;
                        currentState = PARSE_ELEMENT_NAME;
                        break;
                    }
                }
                currentTextContentUsage ++;
                if (currentTextContentUsage > 131070) {
                    error_xml.error = 2;
                    return error_xml;
                }
                currentTextContent[strlen(currentTextContent)] = curChar;
                break;
            case PARSE_ELEMENT_NAME:
                if (currentIndex == 1) {
                    if (curChar == '!') {
                        if (nextChar == '-' && nextChar2 == '-') {
                            i += 2;
                            bytesParsed += 2;
                            currentIndex = 0;
                            currentState = PARSE_COMMENT;
                        } else {
                            currentIndex = 0;
                            currentState = PARSE_DOCTYPE;
                        }
                        break;
                    }
                    doneParsingName = 0;
                    startedParsingName = 0;
                    isEscapeEnabled = 0;
                    isInsideString = 0;
                }
                
                // TODO: this should parse attributes instead of just looking at quotes
                if (curChar == '"') {
                    if (isInsideString) {
                        if (!isEscapeEnabled) {
                            isInsideString = 0;
                        }
                    } else {
                        isInsideString = 1;
                    }
                }
                if (curChar == '\\' && !isEscapeEnabled) {
                    isEscapeEnabled = 1;
                }
                
                if (curChar == ' ') {
                    if (startedParsingName) {
                        doneParsingName = 1;
                        // TODO: attributes
                    }
                    break;
                }
                if (curChar == '/' && !isInsideString) {
                    if (startedParsingName) {
                        doneParsingName = 1;
                    } else {
                        currentIndex = 0;
                        currentState = PARSE_ELEMENT_SLASH_END;
                        //bytesParsed --;
                        break;
                    }
                    currentState = PARSE_ELEMENT_END_SLASH;
                }
                if (curChar == '>' && !isInsideString) {
                    if (startedParsingName) {
                        doneParsingName = 1;
                        if (html_isClosingElement(currentElementName) && !strcmp(currentElementName, closingTag)) {
                            struct xml_response realResponse;
                            realResponse.error = 0;
                            realResponse.bytesParsed = bytesParsed - (strlen(currentElementName) + 1);
                            realResponse.list = response;

                            return realResponse;
                        }
                        if (html_isVoidElement(currentElementName)) {
                            struct xml_node node = XML_createXMLElement(XML_makeStrCpy(currentElementName));
                            XML_appendChild(&response, node);
                            currentElementNameUsage = 0;
                            XML_clrStr(currentElementName);
                            currentIndex = 0;
                            currentState = PARSE_UNKNOWN;
                            break;
                        }
                        // ohhhh...dear. actual xml parsing. that will be fun.
                        struct xml_data newData;
                        newData.data = xml_string.data + i + 1;
                        newData.length = xml_string.length - i;
                        struct xml_response childResponse = recursive_parse_xml_node(newData, XML_makeStrCpy(currentElementName), closingTag);
                        i += childResponse.bytesParsed;
                        bytesParsed += childResponse.bytesParsed;
                        
                        if (childResponse.error) {
                            error_xml.error = childResponse.error;
                            return error_xml;
                        }
                        
                        struct xml_node node = XML_createXMLElement(XML_makeStrCpy(currentElementName));
                        node.children = childResponse.list;
                        XML_appendChild(&response, node);
                        currentElementNameUsage = 0;
                        XML_clrStr(currentElementName);
                        currentIndex = 0;
                        currentState = PARSE_UNKNOWN;
                    } else {
                        error_xml.error = 3;
                        return error_xml;
                    }
                }
                if (!doneParsingName) {
                    currentElementNameUsage ++;
                    if (currentElementNameUsage > 254) {
                        error_xml.error = 2;
                        return error_xml;
                    }
                    startedParsingName = 1;
                    currentElementName[strlen(currentElementName)] = curChar;
                }
                break;
            case PARSE_ELEMENT_END_SLASH:
                if (curChar == '>') {
                    //i ++;
                    bytesParsed ++;
                    struct xml_node node = XML_createXMLElement(XML_makeStrCpy(currentElementName));
                    XML_appendChild(&response, node);
                    currentElementNameUsage = 0;
                    XML_clrStr(currentElementName);
                    currentIndex = 0;
                    currentState = PARSE_UNKNOWN;
                    break;
                }
                // TODO: there can be attributes here: <abcd /qwerty> = <abcd qwerty></abcd>
                break;
            case PARSE_ELEMENT_SLASH_END:
                if (currentIndex == 1) {
                    doneParsingName = 0;
                    startedParsingName = 0;
                }
                if (curChar == ' ') {
                    if (startedParsingName) {
                        doneParsingName = 1;
                    }
                    break;
                }
                if (curChar == '>') {
                    if (!startedParsingName) {
                        error_xml.error = 1;
                        return error_xml;
                    }
                    if (html_isVoidElement(currentElementName)) {
                        struct xml_node node = XML_createXMLElement(XML_makeStrCpy(currentElementName));
                        XML_appendChild(&response, node);
                        currentElementNameUsage = 0;
                        XML_clrStr(currentElementName);
                        currentIndex = 0;
                        currentState = PARSE_UNKNOWN;
                        break;
                    }
                    if (!strcmp(closingTag, currentElementName)) {
                        bytesParsed += 1;
                        struct xml_response realResponse;
                        realResponse.error = 0;
                        realResponse.bytesParsed = bytesParsed;
                        realResponse.list = response;

                        return realResponse;
                    } else if (!strcmp(parentClosingTag, currentElementName) && html_isClosingElement(closingTag)) {
                        struct xml_response realResponse;
                        realResponse.error = 0;
                        realResponse.bytesParsed = bytesParsed - (strlen(currentElementName) + 2);
                        realResponse.list = response;

                        return realResponse;
                    } else {
                        // AFAICS Chrome ignores these tags, so I do too
                    }
                    break;
                }
                if (!doneParsingName) {
                    currentElementNameUsage ++;
                    if (currentElementNameUsage > 254) {
                        error_xml.error = 2;
                        return error_xml;
                    }
                    startedParsingName = 1;
                    currentElementName[strlen(currentElementName)] = curChar;
                }
                break;
            case PARSE_DOCTYPE:
                if (curChar == '>') {
                    char *doctypeContent = XML_makeStrCpy(currentDoctypeContent);
                    struct xml_node node = XML_createDoctype(doctypeContent);
                    XML_appendChild(&response, node);
                    currentDoctypeContentUsage = 0;
                    XML_clrStr(currentDoctypeContent);
                    currentIndex = 0;
                    currentState = PARSE_UNKNOWN;
                    break;
                }
                currentDoctypeContentUsage ++;
                if (currentDoctypeContentUsage > 254) {
                    error_xml.error = 2;
                    return error_xml;
                }
                currentDoctypeContent[strlen(currentDoctypeContent)] = curChar;
                break;
            case PARSE_COMMENT:
                if (curChar == '-' && nextChar == '-' && nextChar2 == '>') {
                    struct xml_node node = XML_createComment(XML_makeStrCpy(currentTextContent));
                    XML_appendChild(&response, node);
                    currentTextContentUsage = 0;
                    XML_clrStr(currentTextContent);
                    currentIndex = 0;
                    currentState = PARSE_UNKNOWN;
                    i += 2;
                    bytesParsed += 2;
                    break;
                }
                currentTextContentUsage ++;
                if (currentTextContentUsage > 131070) {
                    error_xml.error = 2;
                    return error_xml;
                }
                currentTextContent[strlen(currentTextContent)] = curChar;
                break;
            case PARSE_UNKNOWN:
                if (curChar == '<') {
                    currentIndex = 0;
                    currentState = PARSE_ELEMENT_NAME;
                } else {
                    i --;
                    bytesParsed --;
                    currentIndex = 0;
                    currentState = PARSE_ELEMENT_TEXT;
                }
                break;
        }
        currentIndex ++;
        bytesParsed ++;
        isEscapeEnabled = 0;
    }
    struct xml_response realResponse;
    realResponse.error = 0;
    realResponse.bytesParsed = bytesParsed;
    realResponse.list = response;

    return realResponse;
}

struct xml_response XML_parseXmlNodes(struct xml_data xml_string) {
    return recursive_parse_xml_node(xml_string, "", "");
}

#endif
