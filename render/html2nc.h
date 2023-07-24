#include "../xml/html.h"
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
    int res = !strcmp(lower, "div") || !strcmp(lower, "ul") || !strcmp(lower, "p") || !strcmp(lower, "hr") || !strcmp(lower, "header") || !strcmp(lower, "aside") || !strcmp(lower, "h1") || !strcmp(lower, "h2") || !strcmp(lower, "h3") || !strcmp(lower, "h4") || !strcmp(lower, "h5") || !strcmp(lower, "h6") || !strcmp(lower, "blockquote") || !strcmp(lower, "dt") || !strcmp(lower, "tspan") || !strcmp(lower, "desc");
    free(lower);
    return res;
}

int isInline(const char *nodeName) {
    char *lower = xml_toLowerCase(nodeName);
    int res = !strcmp(lower, "a") || !strcmp(lower, "span") || !strcmp(lower, "font");
    free(lower);
    return res;
}

char *parseHTMLEscapes(const char *content) {
    char *allocated = (char *) calloc(strlen(content) + 1, sizeof(char));
    int len = strlen(content);
    int realIndex = 0;
    for (int i = 0; i < len; i ++) {
        char curChar = content[i];

        if (curChar == '&') {
            if (!strncmp(content + i + 1, "lt", 2)) {
                allocated[realIndex] = '<';
                i += 3;
                realIndex ++;
                if (content[i] != ';') i --;
            } else if (!strncmp(content + i + 1, "gt", 2)) {
                allocated[realIndex] = '>';
                i += 3;
                realIndex ++;
                if (content[i] != ';') i --;
            } else if (!strncmp(content + i + 1, "amp", 3)) {
                allocated[realIndex] = '&';
                i += 4;
                realIndex ++;
                if (content[i] != ';') i --;
            } else if (!strncmp(content + i + 1, "nbsp", 4)) {
                allocated[realIndex] = ' ';
                i += 5;
                realIndex ++;
                if (content[i] != ';') i --;
            } else if (!strncmp(content + i + 1, "#305", 4)) {
                // display-handling doesn't support this, unfortunately
                strcat(allocated, "ı");
                i += 5;
                realIndex += strlen("ı");
                if (content[i] != ';') i --;
            } else if (!strncmp(content + i + 1, "copy", 4)) {
                // display-handling doesn't support the actual non-ascii symbols (copyright), unfortunately
                allocated[realIndex] = 'C';
                i += 5;
                realIndex ++;
                if (content[i] != ';') i --;
            } else if (!strncmp(content + i + 1, "raquo", 5)) {
                // display-handling doesn't support these, unfortunately
                allocated[realIndex] = 0xbb;
                i += 6;
                realIndex ++;
                if (content[i] != ';') i --;
            } else {
                allocated[realIndex] = curChar;
                realIndex ++;
            }
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
    int hasBlocked;
    char *title;
};

struct html2nc_result {
    char *text;
    char *title;
};

char *recursiveXMLToText(struct xml_node *parent, struct xml_list xml, struct html2nc_state *state, char *originalHTML, int isRenderingSVG) {
    char *alloc = (char *) calloc(strlen(originalHTML) + 2, sizeof(char));
    int currentOrderedListNum = 1;
    int justHadInlineInsideBlockWithText = 0;
    for (int i = 0; i < xml.count; i ++) {
        struct xml_node node = xml.nodes[i];
        if (node.type == NODE_DOCTYPE) {
            // ignore
        } else if (node.type == NODE_TEXT) {
            // SVGs should not have text within them
            if (!isRenderingSVG) {
                char *text = getXMLTrimmedTextContent(node.text_content, justHadInlineInsideBlockWithText);
                char *allocated = parseHTMLEscapes(text);
                strcat(alloc, allocated);
                free(text);
                free(allocated);
            }
            //printf(strcat(getTabsRepeated(depth), "text node (#%d). contents: '%s'\n"), i, node.text_content);
            //printf(strcat(getTabsRepeated(depth), "text node (#%d). contents: '...'\n"), i);
        } else if (node.type == NODE_COMMENT) {
            // ignore
        } else if (node.type == NODE_ELEMENT) {
            char *lower = xml_toLowerCase(node.name);
            if (isBlock(&node) && !state->hasBlocked) {
                strcat(alloc, "\n");
            }
            if (!strcmp(lower, "br")) {
                strcat(alloc, "\n");
            } else if (!strcmp(lower, "hr")) {
                strcat(alloc, "---------------------");
            } else if (shouldBeDisplayed(node.name)) {
                if (!strcmp(lower, "li")) {
                    char *parentName = xml_toLowerCase(parent->name);
                    if (!strcmp(parentName, "ol")) {
                        char *newBuffer = (char *) calloc(currentOrderedListNum + 1, sizeof(char));
                        sprintf(newBuffer, "\n%d. ", currentOrderedListNum);
                        strcat(alloc, newBuffer);
                        free(newBuffer);
                        currentOrderedListNum ++;
                    } else {
                        strcat(alloc, "\n - ");
                    }
                    free(parentName);
                }

                int isSVG = 0;
                if (!strcmp(lower, "svg")) {
                    isSVG = 1;
                }
                char *text = recursiveXMLToText(&node, node.children, state, originalHTML, isSVG || isRenderingSVG);
                isSVG = 0;

                // Only the first <title> is taken into account- the rest are displayed
                if (!strcmp(lower, "title") && state->title == NULL) {
                    state->title = XML_makeStrCpy(text);
                    free(text);
                    free(lower);
                    continue;
                }
                
                strcat(alloc, text);
                if (isInline(node.name) && strlen(text) && isBlock(parent)) {
                    free(text);
                    free(lower);
                    justHadInlineInsideBlockWithText = 1;
                    continue;
                }
                free(text);
            }
            if (isBlock(&node) && !state->hasBlocked) {
                strcat(alloc, "\n");
            }
            state->hasBlocked = 0;
            free(lower);
        }
        justHadInlineInsideBlockWithText = 0;
    }
    
    char *copy = XML_makeStrCpy(alloc);
    free(alloc);
    return copy;
}

struct html2nc_result htmlToText(struct xml_list xml, char *originalHTML) {
    struct html2nc_state state;
    state.hasBlocked = 1;
    state.title = NULL;
    
    struct html2nc_result result;
    result.text = recursiveXMLToText(NULL, xml, &state, originalHTML, 0);

    // Default title, if title wasn't set
    if (state.title == NULL) {
        result.title = XML_makeStrCpy("Page has no title");
    } else {
        result.title = XML_makeStrCpy(state.title);
    }

    free(state.title);

    return result;
}
