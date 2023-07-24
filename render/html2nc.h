#include "../xml/html.h"
#include <stdlib.h>
#include <string.h>

int shouldBeDisplayed(char *nodeName) {
    return strcmp(nodeName, "script") && strcmp(nodeName, "style");
}

int isBlock(struct xml_node *node) {
    if (node == NULL) {
        return 1;
    }
    char *lower = xml_toLowerCase(node->name);
    return !strcmp(lower, "div") || !strcmp(lower, "ul") || !strcmp(lower, "p") || !strcmp(lower, "hr") || !strcmp(lower, "header") || !strcmp(lower, "aside") || !strcmp(lower, "h1") || !strcmp(lower, "h2") || !strcmp(lower, "h3") || !strcmp(lower, "h4") || !strcmp(lower, "h5") || !strcmp(lower, "h6") || !strcmp(lower, "blockquote") || !strcmp(lower, "dt") || !strcmp(lower, "tspan") || !strcmp(lower, "desc");
}

int isInline(char *nodeName) {
    char *lower = xml_toLowerCase(nodeName);
    return !strcmp(lower, "a") || !strcmp(lower, "span");
}

// NOTE: This will free() the `content` passed into it
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

char *getXMLTrimmedTextContent(char *content, int removeStart) {
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

char *recursiveXMLToText(struct xml_node *parent, struct xml_list xml, struct html2nc_state *state, char *originalHTML) {
    char *alloc = (char *) calloc(strlen(originalHTML) + 2, sizeof(char));
    int currentOrderedListNum = 1;
    int justHadInlineInsideBlockWithText = 0;
    for (int i = 0; i < xml.count; i ++) {
        struct xml_node node = xml.nodes[i];
        if (node.type == NODE_DOCTYPE) {
            // ignore
        } else if (node.type == NODE_TEXT) {
            char *text = getXMLTrimmedTextContent(node.text_content, justHadInlineInsideBlockWithText);
            strcat(alloc, parseHTMLEscapes(text));
            free(text);
            //printf(strcat(getTabsRepeated(depth), "text node (#%d). contents: '%s'\n"), i, node.text_content);
            //printf(strcat(getTabsRepeated(depth), "text node (#%d). contents: '...'\n"), i);
        } else if (node.type == NODE_COMMENT) {
            // ignore
        } else if (node.type == NODE_ELEMENT) {
            if (isBlock(&node) && !state->hasBlocked) {
                strcat(alloc, "\n");
            }
            if (!strcmp(xml_toLowerCase(node.name), "br")) {
                strcat(alloc, "\n");
            } else if (!strcmp(xml_toLowerCase(node.name), "hr")) {
                strcat(alloc, "---------------------");
            } else if (shouldBeDisplayed(node.name)) {
                if (!strcmp(xml_toLowerCase(node.name), "li")) {
                    if (!strcmp(xml_toLowerCase(parent->name), "ol")) {
                        char *newBuffer = (char *) calloc(currentOrderedListNum + 1, sizeof(char));
                        sprintf(newBuffer, "\n%d. ", currentOrderedListNum);
                        strcat(alloc, newBuffer);
                        free(newBuffer);
                        currentOrderedListNum ++;
                    } else {
                        strcat(alloc, "\n - ");
                    }
                }
                
                char *text = recursiveXMLToText(&node, node.children, state, originalHTML);
                if (!strcmp(xml_toLowerCase(node.name), "title")) {
                    state->title = XML_makeStrCpy(text);
                    continue;
                }
                
                strcat(alloc, text);
                if (isInline(node.name) && strlen(text) && isBlock(parent)) {
                    justHadInlineInsideBlockWithText = 1;
                    continue;
                }
            }
            if (isBlock(&node) && !state->hasBlocked) {
                strcat(alloc, "\n");
            }
            state->hasBlocked = 0;
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
    state.title = XML_makeStrCpy("Page has no title");
    
    struct html2nc_result result;
    result.text = recursiveXMLToText(NULL, xml, &state, originalHTML);
    result.title = state.title;
    return result;
}
