#include "../css/block-inline.h"
#include "../utils/string.h"

#ifndef _XML_HTML
    #define _XML_HTML 1

#define html_numVoidElements 18
#define html_numClosingElements 2
#define html_numChildlessElements 3
char html_voidElements[][16] = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source", "track", "wbr", "command", "keygen", "menuitem", "frame"};
char html_childlessElements[][8] = {"script", "style", "title"};

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
    char *attribute_content;

    struct xml_node *parent;
    struct xml_list children;

    enum xml_node_type type;
};

struct xml_response {
    struct xml_list list;
    int error;
    int bytesParsed;
    int closes_two;
};

struct xml_node XML_createXMLElement(char *name, struct xml_node *parent) {
    struct xml_node node;
    node.name = name;
    node.text_content = NULL;
    node.attribute_content = (char *) calloc(1, sizeof(char));

    struct xml_list children;
    children.count = 0;
    children.nodes = NULL;

    node.children = children;
    node.parent = parent;
    node.type = NODE_ELEMENT;
    return node;
}

struct xml_node XML_createXMLText(char *text_content, struct xml_node *parent) {
    struct xml_node node;
    node.name = NULL;
    node.text_content = text_content;
    node.attribute_content = NULL;

    struct xml_list children;
    children.count = 0;
    children.nodes = NULL;

    node.children = children;
    node.parent = parent;
    node.type = NODE_TEXT;
    return node;
}

struct xml_node XML_createDoctype(char *text_content, struct xml_node *parent) {
    struct xml_node node;
    node.name = NULL;
    node.text_content = text_content;
    node.attribute_content = NULL; // TODO: can DOCTYPE's have attribs?

    struct xml_list children;
    children.count = 0;
    children.nodes = NULL;

    node.children = children;
    node.parent = parent;
    node.type = NODE_DOCTYPE;
    return node;
}

struct xml_node XML_createComment(char *text_content, struct xml_node *parent) {
    struct xml_node node;
    node.name = NULL;
    node.text_content = text_content;
    node.attribute_content = NULL;

    struct xml_list children;
    children.count = 0;
    children.nodes = NULL;

    node.children = children;
    node.parent = parent;
    node.type = NODE_COMMENT;
    return node;
}

struct xml_list XML_createEmptyXMLList() {
    struct xml_list list;
    list.count = 0;
    list.nodes = (struct xml_node *) calloc(0, sizeof(struct xml_node));
    return list;
}

void XML_appendChild(struct xml_list *parent, struct xml_node child) {
    // TODO: Check that child is not in ancestors/descendants of parent
    parent->count ++;
    parent->nodes = (struct xml_node *) realloc(parent->nodes, sizeof(struct xml_node) * parent->count);
    parent->nodes[parent->count - 1] = child;
}

struct xml_list XML_ancestors(struct xml_node *node) {
    struct xml_list response = XML_createEmptyXMLList();
    int loop_guard = 0;
    while (node->parent) {
        if (loop_guard > 8192) {
            log_err("Too high recursion (>8192) in XML_ancestors! Possible recursive XML");
            return response;
        }
        XML_appendChild(&response, *node->parent);
        loop_guard ++;
    }
    return response;
}

char *XML_nodeTypeToString(enum xml_node_type type) {
    if (type == NODE_DOCTYPE) {
        return makeStrCpy("doctype");
    }
    if (type == NODE_TEXT) {
        return makeStrCpy("text");
    }
    if (type == NODE_ELEMENT) {
        return makeStrCpy("element");
    }
    if (type == NODE_COMMENT) {
        return makeStrCpy("comment");
    }
    return makeStrCpy("!CORRUPT!");
}

int html_isVoidElement(const char *name) {
    int i = html_numVoidElements - 1;
    char *lower = toLowerCase(name);
    while (i >= 0) {
        if (!strcmp(lower, html_voidElements[i])) {
            free(lower);
            return 1;
        }
        i --;
    }
    free(lower);
    return 0;
}

int html_isChildlessElement(const char *name) {
    int i = html_numChildlessElements - 1;
    char *lower = toLowerCase(name);
    while (i >= 0) {
        if (!strcmp(lower, html_childlessElements[i])) {
            free(lower);
            return 1;
        }
        i --;
    }
    free(lower);
    return 0;
}

struct xml_data XML_xmlDataFromString(char *string) {
    struct xml_data data;
    data.data = string;
    data.length = strlen(string);
    return data;
}

void recursiveFreeXML(struct xml_list xml) {
    for (int i = 0; i < xml.count; i ++) {
        struct xml_node node = xml.nodes[i];
        if (node.type == NODE_DOCTYPE) {
            free(node.text_content);
        } else if (node.type == NODE_TEXT) {
            free(node.text_content);
        } else if (node.type == NODE_COMMENT) {
            free(node.text_content);
        } else if (node.type == NODE_ELEMENT) {
            free(node.name);
            free(node.attribute_content);
            if (node.children.count) {
                recursiveFreeXML(node.children);
            }
        }
    }
    free(xml.nodes);
}

/*
error codes:
0: no error
1: generic error
2: xml too large
3: xml parsing failure: encountered a `...</`
*/

struct xml_response recursive_parse_xml_node(struct xml_node *parent, struct xml_data xml_string, char *closingTag, char *parentClosingTag, int is_svg) {
    struct xml_response error_xml;
    error_xml.error = 1;

    if (xml_string.length == 0) {
        struct xml_response response;
        response.list = XML_createEmptyXMLList();
        response.error = 0;
        response.closes_two = 0;
        return response;
    }

    struct xml_list response;
    response.count = 0;
    response.nodes = NULL;

    int currentState = PARSE_UNKNOWN;
    int currentIndex = 1;

    int maxTextContent = 256; // This will be dynamically resized
    int maxDoctypeContent = 256;
    int maxElementName = 256;
    int maxAttributeContent = 256; // This will be dynamically resized

    char *currentTextContent = (char *) calloc(maxTextContent, sizeof(char));
    char *currentDoctypeContent = (char *) calloc(maxDoctypeContent, sizeof(char));
    char *currentElementName = (char *) calloc(maxElementName, sizeof(char));
    char *currentAttributeContent = (char *) calloc(maxAttributeContent, sizeof(char));

    int currentTextContentUsage = 0;
    int currentDoctypeContentUsage = 0;
    int currentElementNameUsage = 0;
    int currentAttributeContentUsage = 0;

    int startedParsingName = 0;
    int doneParsingName = 0;
    int isEscapeEnabled = 0;
    int isInsideString = 0;
    int isInsideSingleQuoteString = 0;

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

        switch(currentState) {
            case PARSE_ELEMENT_TEXT:
                if (curChar == '<') {
                    int isScriptTag = !strncmp(xml_string.data + i + 1, "/script>", 8);
                    int isStyleTag = !strncmp(xml_string.data + i + 1, "/style>", 7);
                    int isTitleTag = !strncmp(xml_string.data + i + 1, "/title>", 7);

                    // why, HTML?
                    if ((strcmp(closingTag, "script") || isScriptTag) && (strcmp(closingTag, "style") || isStyleTag) && (strcmp(closingTag, "title") || isTitleTag)) {
                        struct xml_node node = XML_createXMLText(makeStrCpy(currentTextContent), parent);
                        XML_appendChild(&response, node);
                        currentTextContentUsage = 0;
                        clrStr(currentTextContent);
                        currentIndex = 0;
                        currentState = PARSE_ELEMENT_NAME;
                        break;
                    }
                }
                currentTextContentUsage ++;
                if (currentTextContentUsage > maxTextContent - 2) {
                    currentTextContent = (char *) realloc(currentTextContent, maxTextContent * 2);
                    memset(currentTextContent + maxTextContent, 0, maxTextContent);
                    maxTextContent *= 2;
                }
                currentTextContent[currentTextContentUsage - 1] = curChar;
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

                if (curChar == '"' && !isInsideSingleQuoteString) {
                    if (isInsideString) {
                        if (!isEscapeEnabled) {
                            isInsideString = 0;
                        }
                    } else {
                        isInsideString = 1;
                    }
                }
                if (curChar == '\'' && !isInsideString) {
                    if (isInsideSingleQuoteString) {
                        if (!isEscapeEnabled) {
                            isInsideSingleQuoteString = 0;
                        }
                    } else {
                        isInsideSingleQuoteString = 1;
                    }
                }

                if (curChar == '\\' && !isEscapeEnabled) {
                    isEscapeEnabled = 1;
                }

                if (isWhiteSpace(curChar)) {
                    if (doneParsingName) {
                        currentAttributeContentUsage ++;
                        if (currentAttributeContentUsage > maxAttributeContent - 2) {
                            currentAttributeContent = (char *) realloc(currentAttributeContent, maxAttributeContent * 2);
                            memset(currentAttributeContent + maxAttributeContent, 0, maxAttributeContent);
                            maxAttributeContent *= 2;
                        }
                        currentAttributeContent[strlen(currentAttributeContent)] = curChar;
                    }
                    if (startedParsingName) {
                        doneParsingName = 1;
                    }
                    break;
                }
                if (curChar == '/' && !isInsideString && !isInsideSingleQuoteString) {
                    if (startedParsingName) {
                        doneParsingName = 1;
                    } else {
                        currentIndex = 0;
                        currentState = PARSE_ELEMENT_SLASH_END;
                        //bytesParsed --;
                        break;
                    }

                    if (is_svg) {
                        // SVG follows normal XML processing rules, including treating <tag/> as a self-closing tag.
                        currentState = PARSE_ELEMENT_END_SLASH;
                    }
                }
                if (curChar == '>' && !isInsideString && !isInsideSingleQuoteString) {
                    if (startedParsingName) {
                        doneParsingName = 1;
                        // FIXME: <a>s have similar behavior
                        char *lowerNameParent = toLowerCase(parentClosingTag);
                        char *lowerName = toLowerCase(closingTag);
                        char *lowerCurName = toLowerCase(currentElementName);
                        int isParagraphClose = (!strcmp(lowerName, "p") || !strcmp(lowerNameParent, "p")) && CSS_isDefaultBlock(currentElementName);
                        int isListClose = !strcmp(lowerName, "li") && !strcmp(lowerCurName, "li");
                        int isDtClose = !strcmp(lowerName, "dt") && !strcmp(lowerCurName, "dd");
                        int isDdClose = !strcmp(lowerName, "dd") && !strcmp(lowerCurName, "dt");

                        free(lowerNameParent);

                        if (isParagraphClose || isListClose || isDtClose || isDdClose) {
                            int amt = strlen(currentElementName) + 1;
                            if (currentAttributeContent[0] != 0) {
                                amt += strlen(currentAttributeContent) + 1;
                            }
                            struct xml_response realResponse;
                            realResponse.error = 0;
                            realResponse.bytesParsed = bytesParsed - amt;
                            realResponse.list = response;
                            realResponse.closes_two = 0;

                            free(currentTextContent);
                            free(currentDoctypeContent);
                            free(currentElementName);
                            free(currentAttributeContent);
                            free(lowerName);
                            free(lowerCurName);

                            return realResponse;
                        }
                        free(lowerName);
                        free(lowerCurName);
                        if (html_isVoidElement(currentElementName)) {
                            struct xml_node node = XML_createXMLElement(makeStrCpy(currentElementName), parent);
                            node.attribute_content = makeStrCpy(currentAttributeContent);

                            currentAttributeContentUsage = 0;
                            clrStr(currentAttributeContent);

                            XML_appendChild(&response, node);
                            currentElementNameUsage = 0;
                            clrStr(currentElementName);
                            currentIndex = 0;
                            currentState = PARSE_UNKNOWN;
                            break;
                        }

                        struct xml_node *node = (struct xml_node *) calloc(1, sizeof(struct xml_node));
                        struct xml_node cnode = XML_createXMLElement(makeStrCpy(currentElementName), parent);
                        memcpy(node, &cnode, sizeof(struct xml_node));

                        struct xml_data newData;
                        newData.data = xml_string.data + i + 1;
                        newData.length = xml_string.length - i;

                        char *copy = makeStrCpy(currentElementName);
                        char *lowerCopy = toLowerCase(currentElementName);
                        struct xml_response childResponse = recursive_parse_xml_node(node, newData, copy, closingTag, !strcmp(lowerCopy, "svg") || is_svg);
                        free(lowerCopy);
                        free(copy);

                        i += childResponse.bytesParsed;
                        bytesParsed += childResponse.bytesParsed;

                        if (childResponse.error) {
                            free(currentTextContent);
                            free(currentDoctypeContent);
                            free(currentElementName);
                            free(currentAttributeContent);

                            error_xml.error = childResponse.error;
                            return error_xml;
                        }

                        node->attribute_content = makeStrCpy(currentAttributeContent);
                        currentAttributeContentUsage = 0;
                        clrStr(currentAttributeContent);

                        node->children = childResponse.list;
                        XML_appendChild(&response, *node);
                        currentElementNameUsage = 0;
                        clrStr(currentElementName);
                        currentIndex = 0;
                        currentState = PARSE_UNKNOWN;

                        if (childResponse.closes_two) {
                            free(currentTextContent);
                            free(currentDoctypeContent);
                            free(currentElementName);
                            free(currentAttributeContent);

                            struct xml_response realResponse;
                            realResponse.error = 0;
                            realResponse.bytesParsed = bytesParsed;
                            realResponse.list = response;
                            realResponse.closes_two = 0;

                            return realResponse;
                        }
                    } else {
                        free(currentTextContent);
                        free(currentDoctypeContent);
                        free(currentElementName);
                        free(currentAttributeContent);

                        error_xml.error = 3;
                        return error_xml;
                    }
                } else {
                    if (!doneParsingName) {
                        currentElementNameUsage ++;
                        if (currentElementNameUsage > maxElementName - 2) {
                            free(currentTextContent);
                            free(currentDoctypeContent);
                            free(currentElementName);
                            free(currentAttributeContent);

                            error_xml.error = 2;
                            return error_xml;
                        }
                        startedParsingName = 1;
                        currentElementName[strlen(currentElementName)] = curChar;
                    } else {
                        currentAttributeContentUsage ++;
                        if (currentAttributeContentUsage > maxAttributeContent - 2) {
                            currentAttributeContent = (char *) realloc(currentAttributeContent, maxAttributeContent * 2);
                            memset(currentAttributeContent + maxAttributeContent, 0, maxAttributeContent);
                            maxAttributeContent *= 2;
                        }
                        currentAttributeContent[strlen(currentAttributeContent)] = curChar;
                    }
                }
                break;
            case PARSE_ELEMENT_END_SLASH:
                if (curChar == '>') {
                    struct xml_node node = XML_createXMLElement(makeStrCpy(currentElementName), parent);

                    node.attribute_content = makeStrCpy(currentAttributeContent);
                    currentAttributeContentUsage = 0;
                    clrStr(currentAttributeContent);

                    XML_appendChild(&response, node);
                    currentElementNameUsage = 0;
                    clrStr(currentElementName);
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
                if (isWhiteSpace(curChar)) {
                    if (startedParsingName) {
                        doneParsingName = 1;
                    }
                    break;
                }
                if (curChar == '>') {
                    if (!startedParsingName) {
                        free(currentTextContent);
                        free(currentDoctypeContent);
                        free(currentElementName);
                        free(currentAttributeContent);

                        error_xml.error = 1;
                        return error_xml;
                    }
                    if (html_isVoidElement(currentElementName)) {
                        // Early bail- read below comments
                        if (response.count) {
                            char *lastName = toLowerCase(response.nodes[response.count - 1].name);
                            char *currentLowerName = toLowerCase(currentElementName);
                            if (!strcmp(lastName, currentLowerName)) {
                                // We were just parsing an element with the same name as this one-
                                // for example, <br></br> or <param></param>. Browsers seem to
                                // ignore the second tag, and only count the first (but only
                                // for void elements).
                                if (strcmp(currentLowerName, "br")) {
                                    // For some reason, <br></br> appears as
                                    // two separate <br> tags. None of the
                                    // other void elements seem to do this.

                                    // Just ignore the </element-name> tag.
                                    free(lastName);
                                    free(currentLowerName);

                                    currentElementNameUsage = 0;
                                    clrStr(currentElementName);
                                    currentIndex = 0;
                                    currentState = PARSE_UNKNOWN;
                                    break;
                                }
                            }

                            free(lastName);
                            free(currentLowerName);
                        }

                        struct xml_node node = XML_createXMLElement(makeStrCpy(currentElementName), parent);
                        XML_appendChild(&response, node);
                        currentElementNameUsage = 0;
                        clrStr(currentElementName);
                        currentIndex = 0;
                        currentState = PARSE_UNKNOWN;
                        break;
                    }

                    char *lowerName = toLowerCase(currentElementName);
                    char *lowerClose = toLowerCase(closingTag);
                    char *lowerParent = toLowerCase(parentClosingTag);
                    // HTML is very lenient
                    if (closingTag[0] != 0) {
                        bytesParsed += 1;
                        int specialNum;
                        if (!strcmp(lowerParent, lowerName) && strcmp(lowerClose, lowerName)) {
                            specialNum = 1;
                        } else {
                            specialNum = 0;
                        }

                        struct xml_response realResponse;
                        realResponse.error = 0;
                        realResponse.bytesParsed = bytesParsed + specialNum;
                        realResponse.list = response;
                        realResponse.closes_two = specialNum;

                        free(currentTextContent);
                        free(currentDoctypeContent);
                        free(currentElementName);
                        free(currentAttributeContent);

                        free(lowerName);
                        free(lowerClose);
                        free(lowerParent);

                        return realResponse;
                    } else if (strcmp(lowerClose, "p") && !strcmp(lowerName, "p")) {
                        struct xml_node elem = XML_createXMLElement(makeStrCpy(currentElementName), parent);
                        XML_appendChild(&response, elem);

                        currentElementNameUsage = 0;
                        clrStr(currentElementName);
                        currentIndex = 0;
                        currentState = PARSE_UNKNOWN;
                    } else {
                        // AFAICS Chrome ignores these tags, so I do too
                        currentElementNameUsage = 0;
                        clrStr(currentElementName);
                        currentIndex = 0;
                        currentState = PARSE_UNKNOWN;

                        free(lowerName);
                        free(lowerClose);
                        free(lowerParent);
                    }
                    break;
                }
                if (!doneParsingName) {
                    currentElementNameUsage ++;
                    if (currentElementNameUsage > 254) {
                        free(currentTextContent);
                        free(currentDoctypeContent);
                        free(currentElementName);
                        free(currentAttributeContent);

                        error_xml.error = 2;
                        return error_xml;
                    }
                    startedParsingName = 1;
                    currentElementName[strlen(currentElementName)] = curChar;
                }
                break;
            case PARSE_DOCTYPE:
                if (curChar == '>') {
                    char *doctypeContent = makeStrCpy(currentDoctypeContent);
                    struct xml_node node = XML_createDoctype(doctypeContent, parent);
                    XML_appendChild(&response, node);
                    currentDoctypeContentUsage = 0;
                    clrStr(currentDoctypeContent);
                    currentIndex = 0;
                    currentState = PARSE_UNKNOWN;
                    break;
                }
                currentDoctypeContentUsage ++;
                if (currentDoctypeContentUsage > maxDoctypeContent - 2) {
                    free(currentTextContent);
                    free(currentDoctypeContent);
                    free(currentElementName);
                    free(currentAttributeContent);

                    error_xml.error = 2;
                    return error_xml;
                }
                currentDoctypeContent[strlen(currentDoctypeContent)] = curChar;
                break;
            case PARSE_COMMENT:
                if (curChar == '-' && nextChar == '-' && nextChar2 == '>') {
                    struct xml_node node = XML_createComment(makeStrCpy(currentTextContent), parent);
                    XML_appendChild(&response, node);
                    currentTextContentUsage = 0;
                    clrStr(currentTextContent);
                    currentIndex = 0;
                    currentState = PARSE_UNKNOWN;
                    i += 2;
                    bytesParsed += 2;
                    break;
                }
                currentTextContentUsage ++;
                if (currentTextContentUsage > maxTextContent - 2) {
                    currentTextContent = (char *) realloc(currentTextContent, maxTextContent * 2);
                    memset(currentTextContent + maxTextContent, 0, maxTextContent);
                    maxTextContent *= 2;
                }
                currentTextContent[strlen(currentTextContent)] = curChar;
                break;
            case PARSE_UNKNOWN:
                {
                    char *lower = toLowerCase(parentClosingTag);
                    if (curChar == '<' && !html_isChildlessElement(lower)) {
                        currentIndex = 0;
                        currentState = PARSE_ELEMENT_NAME;
                    } else {
                        i --;
                        bytesParsed --;
                        currentIndex = 0;
                        currentState = PARSE_ELEMENT_TEXT;
                    }
                    free(lower);
                }
                break;
        }
        currentIndex ++;
        bytesParsed ++;
        isEscapeEnabled = 0;
    }

    if (strlen(currentTextContent)) {
        struct xml_node node = XML_createXMLText(makeStrCpy(currentTextContent), parent);
        XML_appendChild(&response, node);
        currentTextContentUsage = 0;
        clrStr(currentTextContent);
    }

    free(currentTextContent);
    free(currentDoctypeContent);
    free(currentElementName);
    free(currentAttributeContent);

    struct xml_response realResponse;
    realResponse.error = 0;
    realResponse.bytesParsed = bytesParsed;
    realResponse.list = response;
    realResponse.closes_two = 0;

    return realResponse;
}

struct xml_response XML_parseXmlNodes(struct xml_data xml_string) {
    return recursive_parse_xml_node(NULL, xml_string, "", "", 0);
}

#endif
