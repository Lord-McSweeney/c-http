#include "../xml/html.h"
#include "../xml/attributes.h"
#include <stdlib.h>
#include <string.h>

int shouldBeDisplayed(const char *nodeName) {
    return strcmp(nodeName, "script") && strcmp(nodeName, "style");
}

int isBlock(struct xml_node *node) {
    if (node == NULL) {
        return 1;
    }
    char *lower = xml_toLowerCase(node->name);
    int res = !strcmp(lower, "div") || !strcmp(lower, "p") || !strcmp(lower, "hr") || !strcmp(lower, "header") || !strcmp(lower, "aside") || !strcmp(lower, "h1") || !strcmp(lower, "h2") || !strcmp(lower, "h3") || !strcmp(lower, "h4") || !strcmp(lower, "h5") || !strcmp(lower, "h6") || !strcmp(lower, "blockquote") || !strcmp(lower, "dt") || !strcmp(lower, "tspan") || !strcmp(lower, "desc") || !strcmp(lower, "li") || !strcmp(lower, "dd") || !strcmp(lower, "tr");
    free(lower);
    return res;
}

int isInline(const char *nodeName) {
    char *lower = xml_toLowerCase(nodeName);
    int res = !strcmp(lower, "a") || !strcmp(lower, "span") || !strcmp(lower, "font") || !strcmp(lower, "b") || !strcmp(lower, "i") || !strcmp(lower, "img") || !strcmp(lower, "strong") || !strcmp(lower, "em") || !strcmp(lower, "sup") || !strcmp(lower, "td");
    free(lower);
    return res;
}

char *HTML_getTabsRepeated(int amount) {
    char *spaces = (char *) calloc(128 + (amount * 4), sizeof(char));
    for (int i = 0; i < amount * 4; i ++) {
        spaces[i] = ' ';
    }
    return spaces;
}

// 0: Not a header
// 1: Bold
// 2: Bold + uppercase
int HTML_headerLevel(const char *nodeName) {
    char *lower = xml_toLowerCase(nodeName);
    int res = 0;
    if (!strcmp(lower, "h1")) res = 2;
    if (!strcmp(lower, "h2")) res = 2;
    if (!strcmp(lower, "h3")) res = 1;
    if (!strcmp(lower, "h4")) res = 1;
    if (!strcmp(lower, "h5")) res = 1;
    if (!strcmp(lower, "h6")) res = 1;
    free(lower);
    return res;
}

char *parseHTMLEscapes(const char *content) {
    int numEscapes = 18;
    char entities[][8] =    {"lt", "gt", "amp", "nbsp", "quot", "copy", "raquo", "#32", "#039", "#91", "#93", "#160", "#171", "#187", "#8211", "#8212", "#8230", "#8260"};
    char replaceWith[][4] = {"<",  ">",  "&",   " ",    "\"",   "C",    ">>",    " ",   "'",    "[",    "]",  " ",    "<<",   ">>",   "--",    "--",    "...",   "/"};


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
            for (int j = 0; j < numEscapes; j ++) {
                int entLen = strlen(entities[j]);
                // i + 1 to skip over the '&'
                if (!strncmp(content + i + 1, entities[j], entLen)) {
                    int replaceLength = strlen(replaceWith[j]);
                    int k;
                    for (k = 0; k < replaceLength; k ++) {
                        allocated[realIndex + k] = replaceWith[j][0];
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
        } else if (curChar == '\\') {
            allocated[realIndex] = curChar;
            allocated[realIndex + 1] = curChar;
            realIndex += 2;
        } else {
            allocated[realIndex] = curChar;
            realIndex ++;
        }
    }
    char *newText = XML_makeStrCpy(allocated);
    free(allocated);
    return newText;
}

char *getXMLTrimmedTextContent(const char *content, int removeStart) {
    char *allocated = (char *) calloc(strlen(content) + 1, sizeof(char));
    char *textContent = XML_makeStrCpy(content);
    int len = strlen(textContent);
    
    int alreadyEncounteredSpace = 0;
    int alreadyEncounteredText = 0;
    if (removeStart) {
        alreadyEncounteredText = 1;
    }
    int wasOnlyWhitespace = len == 0 ? 0 : 1;
    for (int i = 0; i < len; i ++) {
        if (textContent[i] == '\n') textContent[i] = ' ';
        if (textContent[i] == '\t') textContent[i] = ' ';
        if ((alreadyEncounteredSpace == 0 && alreadyEncounteredText) || textContent[i] != ' ') {
            if (textContent[i] != ' ') {
                alreadyEncounteredSpace = 0;
                alreadyEncounteredText = 1;
            }
            allocated[strlen(allocated)] = textContent[i];
        }
        if (textContent[i] == ' ') {
            alreadyEncounteredSpace = 1;
        }
    }
    char *newText = XML_makeStrCpy(allocated);
    free(allocated);
    free(textContent);
    return newText;
}


struct html2nc_state {
    char *title;
};

struct html2nc_result {
    char *text;
    char *title;
};

char *recursiveXMLToText(struct xml_node *parent, struct xml_list xml, struct html2nc_state *state, char *originalHTML, int isAvoidingDisplay, int uppercase, int listNestAmount) {
    char *alloc = (char *) calloc(strlen(originalHTML) + 2, sizeof(char));

    int currentOrderedListNum = 1;

    int alreadySetOrderedListNum = 0;
    int justHadInlineInsideBlockWithText = 0;
    int hasBlocked = 0;

    for (int i = 0; i < xml.count; i ++) {
        struct xml_node node = xml.nodes[i];
        if (node.type == NODE_DOCTYPE || node.type == NODE_COMMENT) {
            // ignore
        } else if (node.type == NODE_TEXT) {
            // SVGs should not have text within them
            if (!isAvoidingDisplay) {
                char *text = getXMLTrimmedTextContent(node.text_content, justHadInlineInsideBlockWithText);
                char *allocated = parseHTMLEscapes(text);
                if (uppercase) {
                    char *upper = xml_toUpperCase(allocated);
                    strcat(alloc, upper);
                    free(upper);
                } else {
                    strcat(alloc, allocated);
                }
                free(text);
                free(allocated);
            }
            hasBlocked = 0;
            //printf(strcat(getTabsRepeated(depth), "text node (#%d). contents: '%s'\n"), i, node.text_content);
            //printf(strcat(getTabsRepeated(depth), "text node (#%d). contents: '...'\n"), i);
        } else if (node.type == NODE_ELEMENT) {
            struct xml_attrib_result attrib_result = XML_parseAttributes(node.attribute_content);
            struct xml_attributes *attributes;
            if (attrib_result.error) {
                attributes = XML_makeEmptyAttributes();
            } else {
                attributes = attrib_result.attribs;
            }

            struct xml_attributes *parent_attributes;
            if (parent == NULL) {
                parent_attributes = XML_makeEmptyAttributes();
            } else {
                struct xml_attrib_result parent_attrib_result = XML_parseAttributes(parent->attribute_content);
                if (parent_attrib_result.error) {
                    parent_attributes = XML_makeEmptyAttributes();
                } else {
                    parent_attributes = parent_attrib_result.attribs;
                }
            }

            char *lower = xml_toLowerCase(node.name);
            int hLevel = HTML_headerLevel(node.name);
            if ((isBlock(&node) && (!hasBlocked || !strcmp(lower, "p") || hLevel)) || (!isBlock(&node) && hasBlocked)) {
                strcat(alloc, "\n");
            }

            if (!strcmp(lower, "br")) {
                strcat(alloc, "\n");
            } else if (!strcmp(lower, "hr")) {
                strcat(alloc, "\\h");
            } else if (shouldBeDisplayed(node.name)) {
                if (!strcmp(lower, "dd")) {
                    strcat(alloc, "\\t");
                }
                if (!strcmp(lower, "i") || !strcmp(lower, "em")) {
                    strcat(alloc, "\\i");
                }
                if (!strcmp(lower, "a")) {
                    strcat(alloc, "\\4\\q");
                }
                if (!strcmp(lower, "b") || !strcmp(lower, "strong") || hLevel >= 1) {
                    strcat(alloc, "\\b");
                }

                if (!strcmp(lower, "li") && parent != NULL) {
                    char *tabs = HTML_getTabsRepeated(listNestAmount);
                    strcat(alloc, tabs);
                    free(tabs);
                    listNestAmount ++;

                    if (parent == NULL) {
                        strcat(alloc, "\n - ");
                    } else {
                        char *parentName = xml_toLowerCase(parent->name);
                        if (!strcmp(parentName, "ol")) {
                            if (!alreadySetOrderedListNum) {
                                char *startString = XML_getAttributeByName(parent_attributes, "start");
                                if (startString != NULL) {
                                    currentOrderedListNum = atoi(startString);
                                }
                                alreadySetOrderedListNum = 1;
                            }
                            char *newBuffer = (char *) calloc(currentOrderedListNum + 5, sizeof(char));
                            sprintf(newBuffer, "%d. ", currentOrderedListNum);
                            strcat(alloc, newBuffer);
                            free(newBuffer);
                            currentOrderedListNum ++;
                        } else {
                            strcat(alloc, " - ");
                        }
                        free(parentName);
                    }
                }

                int avoidDisplay = 0;
                if (!strcmp(lower, "svg") || !strcmp(lower, "select")) {
                    avoidDisplay = 1;
                }

                char *text = recursiveXMLToText(&node, node.children, state, originalHTML, avoidDisplay || isAvoidingDisplay, (hLevel >= 2) || uppercase, listNestAmount);
                avoidDisplay = 0;

                // Only the first <title> is taken into account- the rest are displayed
                if (!strcmp(lower, "title") && state->title == NULL) {
                    state->title = XML_makeStrCpy(text);
                    free(text);
                    free(lower);
                    freeXMLAttributes(attributes);
                    freeXMLAttributes(parent_attributes);
                    hasBlocked = 0;
                    continue;
                }

                if (!strcmp(lower, "img")) {
                    strcat(alloc, "[IMAGE]");
                    free(text);
                    free(lower);
                    freeXMLAttributes(attributes);
                    freeXMLAttributes(parent_attributes);
                    hasBlocked = 0;
                    continue;
                }

                if (!strcmp(lower, "select")) {
                    strcat(alloc, "[SELECT]");
                    free(text);
                    free(lower);
                    freeXMLAttributes(attributes);
                    freeXMLAttributes(parent_attributes);
                    hasBlocked = 0;
                    continue;
                }

                strcat(alloc, text);

                if (!strcmp(lower, "b") || !strcmp(lower, "strong") || hLevel >= 1) {
                    strcat(alloc, "\\c");
                }
                if (!strcmp(lower, "i") || !strcmp(lower, "em")) {
                    strcat(alloc, "\\j");
                }
                if (!strcmp(lower, "dd")) {
                    strcat(alloc, "\\u");
                }
                if (!strcmp(lower, "a")) {
                    strcat(alloc, "\\r\\0");
                }
                if (!strcmp(lower, "li")) {
                    listNestAmount --;
                }

                if (isInline(node.name) && strlen(text)) {
                    free(text);
                    free(lower);
                    freeXMLAttributes(attributes);
                    freeXMLAttributes(parent_attributes);
                    justHadInlineInsideBlockWithText = 1;
                    hasBlocked = 0;
                    continue;
                }
                free(text);
            }

            if (isBlock(&node)) {
                strcat(alloc, "\n");
                hasBlocked = 1;
            } else {
                hasBlocked = 0;
            }
            free(lower);
            freeXMLAttributes(attributes);
            freeXMLAttributes(parent_attributes);
        }
        justHadInlineInsideBlockWithText = 0;
    }
    
    char *copy = XML_makeStrCpy(alloc);
    free(alloc);
    return copy;
}

struct html2nc_result htmlToText(struct xml_list xml, char *originalHTML) {
    struct html2nc_state state;
    state.title = NULL;
    
    struct html2nc_result result;
    result.text = recursiveXMLToText(NULL, xml, &state, originalHTML, 0, 0, 0);

    // Default title, if title wasn't set
    if (state.title == NULL) {
        result.title = XML_makeStrCpy("Page has no title");
    } else {
        result.title = XML_makeStrCpy(state.title);
    }

    free(state.title);

    return result;
}
