// Converts XML/HTML nodes to rich text.

#include <stdlib.h>
#include <string.h>

#include "../xml/html.h"
#include "../xml/attributes.h"
#include "../css/styles.h"

int shouldBeDisplayed(const char *nodeName) {
    return strcmp(nodeName, "script") && strcmp(nodeName, "style");
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

            struct css_styling elementStyling = CSS_getDefaultStylesFromElement(node, attributes);

            char *lower = xml_toLowerCase(node.name);
            int hLevel = HTML_headerLevel(node.name);
            if (
                (
                    (
                        CSS_isStyleBlock(elementStyling)
                         && 
                        (!hasBlocked || !strcmp(lower, "p") || hLevel)
                    )
                    ||
                    (
                        !CSS_isStyleBlock(elementStyling)
                         && 
                        hasBlocked
                    )
                )
                 && 
                !isAvoidingDisplay
               ) {
                strcat(alloc, "\n");
            }

            // Only the first <title> is taken into account- the rest aren't special
            if (!strcmp(lower, "title") && state->title == NULL) {
                state->title = recursiveXMLToText(&node, node.children, state, originalHTML, 0, 0, 0);
                free(lower);
                freeXMLAttributes(attributes);
                freeXMLAttributes(parent_attributes);
                hasBlocked = 0;
                continue;
            }

            if (!strcmp(lower, "br") && !isAvoidingDisplay) {
                strcat(alloc, "\n");
            } else if (!strcmp(lower, "hr") && !isAvoidingDisplay) {
                strcat(alloc, "\\h");
            } else {
                int isVisible = !CSS_isStyleHidden(elementStyling);

                if (isVisible && !strcmp(lower, "a") && XML_getAttributeByName(attributes, "href")) {
                    strcat(alloc, "\\m");
                }

                if (elementStyling.tabbed) {
                    strcat(alloc, "\\t");
                }
                if (elementStyling.italic) {
                    strcat(alloc, "\\i");
                }
                if (elementStyling.underline) {
                    strcat(alloc, "\\q");
                }
                switch (elementStyling.color) {
                    case CSS_COLOR_RED:
                        strcat(alloc, "\\1");
                        break;
                    case CSS_COLOR_GREEN:
                        strcat(alloc, "\\2");
                        break;
                    case CSS_COLOR_BLUE:
                        strcat(alloc, "\\4");
                        break;
                }
                if (elementStyling.bold || hLevel >= 1) {
                    strcat(alloc, "\\b");
                }

                if (!strcmp(lower, "li") && isVisible) {
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
                if (!strcmp(lower, "svg") || !strcmp(lower, "select") || !isVisible) {
                    avoidDisplay = 1;
                }

                char *text = recursiveXMLToText(&node, node.children, state, originalHTML, avoidDisplay || isAvoidingDisplay, (hLevel >= 2) || uppercase, listNestAmount);
                avoidDisplay = 0;

                if (!strcmp(lower, "img") && isVisible) {
                    char *altText = XML_getAttributeByName(attributes, "alt");
                    if (altText) {
                        if (strlen(altText)) {
                            strcat(alloc, "[");
                            strcat(alloc, altText);
                            strcat(alloc, "]");
                        }
                        // DO NOT free(altText) here! getAttributeByName returns a reference to the attribute value,
                        // not a copy of the attribute value! freeXMLAttributes will free it anyway!
                    } else {
                        strcat(alloc, "[IMAGE]");
                    }
                    free(text);
                    free(lower);
                    freeXMLAttributes(attributes);
                    freeXMLAttributes(parent_attributes);
                    hasBlocked = 0;
                    continue;
                }

                if (!strcmp(lower, "select") && isVisible) {
                    strcat(alloc, "[SELECT]");
                    free(text);
                    free(lower);
                    freeXMLAttributes(attributes);
                    freeXMLAttributes(parent_attributes);
                    hasBlocked = 0;
                    continue;
                }

                strcat(alloc, text);

                if (elementStyling.bold || hLevel >= 1) {
                    strcat(alloc, "\\c");
                }
                if (elementStyling.italic) {
                    strcat(alloc, "\\j");
                }
                if (elementStyling.tabbed) {
                    strcat(alloc, "\\u");
                }
                switch (elementStyling.color) {
                    case CSS_COLOR_RED:
                    case CSS_COLOR_GREEN:
                    case CSS_COLOR_BLUE:
                        strcat(alloc, "\\0");
                        break;
                }
                if (elementStyling.underline) {
                    strcat(alloc, "\\r");
                }

                if (isVisible && !strcmp(lower, "a") && XML_getAttributeByName(attributes, "href")) {
                    strcat(alloc, "\\n");
                    strcat(alloc, safeEncodeString(XML_getAttributeByName(attributes, "href")));
                }

                if (!strcmp(lower, "li")) {
                    listNestAmount --;
                }

                if (CSS_isDefaultInline(node.name) && strlen(text)) {
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

            if (CSS_isStyleBlock(elementStyling) && !CSS_isStyleHidden(elementStyling)) {
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
