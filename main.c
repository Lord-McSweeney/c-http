// main.c

#include "render/display-handling.h"
#include "navigation/links.h"

#include <stdlib.h>

void freeButtons(struct nc_state *state) {
    for (int i = state->numButtons - 1; i >= state->initialButtons; i --) {
        state->buttons[i].descriptor = NULL;
        state->buttons[i].text = NULL;
        state->buttons[i].visible = 0;
        state->buttons[i].enabled = 0;
        state->numButtons --;
    }
}

void hideButtons(struct nc_state *state) {
    for (int i = state->numButtons - 1; i >= state->initialButtons; i --) {
        state->buttons[i].visible = 0;
    }
}

void showButtons(struct nc_state *state) {
    for (int i = state->numButtons - 1; i >= state->initialButtons; i --) {
        state->buttons[i].visible = 1;
    }
}

void initializeDisplayObjects(struct nc_state *state) {
    createNewText(state, 1, 1, "Go to a URL:", "gotoURL");
    createNewText(state, 0, 0, "No document loaded", "documentText");
    createNewText(state, 0, 0, "This is the browser's home page.\n\nNavigation tools:\nCTRL+O: Go to site\nCTRL+X: Close current page\nUp/Down arrows: scroll current document\nTab: Cycle through buttons/text fields\nEnter: Click current button\n\n\nCreated by uqers.", "helpText");
    createNewTextarea(state, 1, 3, 29, 1, "urltextarea");
    createNewButton(state, 1, 5, "OK", ongotourl, "gotobutton");
    createNewText(state, 1, 13, "Use a custom user agent", "userAgentDetail");
    createNewTextarea(state, 1, 14, 29, 1, "userAgent");
    getTextByDescriptor(state, "gotoURL")->visible = 0;
    getButtonByDescriptor(state, "gotobutton")->visible = 0;
    getTextAreaByDescriptor(state, "urltextarea")->visible = 0;
    getTextByDescriptor(state, "documentText")->visible = 0;
    getTextByDescriptor(state, "helpText")->visible = 1;
    getTextByDescriptor(state, "userAgentDetail")->visible = 0;
    getTextAreaByDescriptor(state, "userAgent")->visible = 0;
    getTextAreaByDescriptor(state, "userAgent")->currentText = HTTP_makeStrCpy("uqers");
}

void openGotoPageDialog(struct nc_state *state) {
    state->selectableIndex = -1;
    struct nc_text_area *textarea = getTextAreaByDescriptor(state, "urltextarea");
    if (state->currentPage == PAGE_DOCUMENT_LOADED) {
        state->currentPage = PAGE_GOTO_OVER_DOCUMENT;
        hideButtons(state);
    } else {
        state->currentPage = PAGE_GOTO_DIALOG;
    }
    getTextByDescriptor(state, "helpText")->visible = 0;
    getTextByDescriptor(state, "gotoURL")->visible = 1;
    textarea->visible = 1;
    getButtonByDescriptor(state, "gotobutton")->visible = 1;
    getTextByDescriptor(state, "documentText")->visible = 0;
    getTextByDescriptor(state, "userAgentDetail")->visible = 1;
    getTextAreaByDescriptor(state, "userAgent")->visible = 1;
    state->selectableIndex = 0;
    
    int curlen = strlen(textarea->currentText);
    while(curlen > 0) {
        textarea->currentText[curlen - 1] = '\0';
        curlen = strlen(textarea->currentText);
    }
    textarea->scrolledAmount = 0;
}

void closeGotoPageDialog(struct nc_state *state) {
    state->selectableIndex = -1;
    if (state->currentPage == PAGE_GOTO_OVER_DOCUMENT) {
        state->currentPage = PAGE_DOCUMENT_LOADED;
        getTextByDescriptor(state, "documentText")->visible = 1;
        showButtons(state);
    } else {
        getTextByDescriptor(state, "documentText")->visible = 0;
        state->currentPage = PAGE_EMPTY;
        getTextByDescriptor(state, "helpText")->visible = 1;
    }
    getTextByDescriptor(state, "gotoURL")->visible = 0;
    getTextAreaByDescriptor(state, "urltextarea")->visible = 0;
    getButtonByDescriptor(state, "gotobutton")->visible = 0;
    getTextByDescriptor(state, "userAgentDetail")->visible = 0;
    getTextAreaByDescriptor(state, "userAgent")->visible = 0;
}

void closeDocumentPage(struct nc_state *state) {
    state->selectableIndex = -1;
    state->currentPage = PAGE_EMPTY;
    getTextByDescriptor(state, "helpText")->visible = 1;
    getTextByDescriptor(state, "documentText")->visible = 0;
    getTextByDescriptor(state, "documentText")->text = HTTP_makeStrCpy("");
    freeButtons(state);
}

void closeCurrentWindow(struct nc_state *state) {
    switch(state->currentPage) {
        case PAGE_GOTO_OVER_DOCUMENT:
        case PAGE_GOTO_DIALOG:
            closeGotoPageDialog(state);
            break;
        case PAGE_DOCUMENT_LOADED:
            closeDocumentPage(state);
            break;
        case PAGE_EMPTY:
            endProgram();
            break;
    }
}

void onKeyPress(struct nc_state *browserState, char ch) {
    switch(ch) {
        case 15: // CTRL+O
            openGotoPageDialog(browserState);
            break;
        case '\t': // tab
            browserState->selectableIndex ++;
            if (browserState->selectableIndex >= browserState->numSelectables) {
                browserState->selectableIndex = 0;
            }
            int i = 0;
            while(!isSelectable(browserState->selectables[browserState->selectableIndex])) {
                browserState->selectableIndex ++;
                if (browserState->selectableIndex >= browserState->numSelectables) {
                    browserState->selectableIndex = 0;
                }
                i ++;
                if (i > browserState->numSelectables + 1) {
                    // all selectables are disabled
                    browserState->selectableIndex = -1;
                    break;
                }
            }
            break;
        case 24: // CTRL+X
            closeCurrentWindow(browserState);
            break;
    }
    struct nc_text_area *textarea = getCurrentSelectedTextarea(browserState);
    if (textarea != NULL) {
        int curlen = strlen(textarea->currentText);
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '.' || ch == '/' || ch == ':' || ch == '#' || ch == '-' || ch == '=' || ch == '?' || ch == '_' || ch == ',' || ch == '<' || ch == '>' || ch == '%' || ch == '&' || ch == '+' || ch == ';' || ch == '~') {
            if (curlen <= 1023) {
                if (curlen >= textarea->width) {
                    textarea->scrolledAmount ++;
                }
                textarea->currentText[curlen] = ch;
            }
        } else if (ch == 7) {
            if (curlen > 0) {
                textarea->currentText[curlen - 1] = '\0';
            }
            if (textarea->scrolledAmount > 0) {
                textarea->scrolledAmount --;
            }
        }
    } else if (browserState->currentPage == PAGE_DOCUMENT_LOADED) {
        struct nc_text *documentText = getTextByDescriptor(browserState, "documentText");
        if (ch == 3) {
            if (documentText->y < 0) {
               documentText->y ++;
            }
        } else if (ch == 2) {
            if (/*todo: make sure the page doesn't scroll too far down*/true) {
                documentText->y --;
            }
        } else if (ch == 4) {
            // left
            if (documentText->x < 0) {
               documentText->x ++;
            }
        } else if (ch == 5) {
            if (/*todo: make sure the page doesn't scroll too far right*/true) {
                documentText->x --;
            }
        }
    }
    
    struct nc_button *button = getCurrentSelectedButton(browserState);
    if (button != NULL) {
        if (ch == 10) {
            button->onclick(browserState, button->descriptor);
        }
    }
}

void *eventLoop(struct nc_state *state) {
    while (1) {
        char ch;
        updateFocus_nc(state);
        render_nc(state);
        if (ch = getch()) {
            onKeyPress(state, ch);
        }
    }
}

/*
char *getTabsRepeated(int amount) {
    char *spaces = (char *) calloc(128 + (amount * 4), sizeof(char));
    for (int i = 0; i < amount * 4; i ++) {
        spaces[i] = ' ';
    }
    return spaces;
}

void recursiveDisplayXML(struct xml_list xml, int depth) {
    for (int i = 0; i < xml.count; i ++) {
        struct xml_node node = xml.nodes[i];
        if (node.type == NODE_DOCTYPE) {
            printf(strcat(getTabsRepeated(depth), "doctype node (#%d). contents: '%s'\n"), i, node.text_content);
        } else if (node.type == NODE_TEXT) {
            printf(strcat(getTabsRepeated(depth), "text node (#%d). contents: '%s'\n"), i, node.text_content);
            //printf(strcat(getTabsRepeated(depth), "text node (#%d). contents: '...'\n"), i);
        } else if (node.type == NODE_COMMENT) {
            printf(strcat(getTabsRepeated(depth), "comment node (#%d). contents: '%s'\n"), i, node.text_content);
            //printf(strcat(getTabsRepeated(depth), "comment node (#%d). contents: '...'\n"), i);
        } else if (node.type == NODE_ELEMENT) {
            printf(strcat(getTabsRepeated(depth), "element node (#%d). node (simplified): <%s>...</%s>\n"), i, node.name, node.name);
            if (node.children.count) {
                recursiveDisplayXML(node.children, depth + 1);
            }
            printf(strcat(getTabsRepeated(depth), "element node: end (%s)\n"), node.name);
        }
    }
}*/

int main(int argc, char **argv) {
    char *url = NULL;
    if (argc == 2) {
        url = argv[1];
    }
/*
    struct http_response parsedResponse = http_makeHTTPRequest("httpforever.com", NULL, NULL, NULL);
    
    if (parsedResponse.error) {
        if (parsedResponse.error == 4) {
            printf("Please enter a valid URL.\n");
        } else if (parsedResponse.error < 4) {
            printf("HTTP error while making request.\n");
        } else if (parsedResponse.error != 5) {
            printf("Encountered error while making HTTP request. Error code: %d\n", parsedResponse.error);
        }
        return 1;
    }
    
    
    printf("response details:\n");
    printf("    code: %d\n", parsedResponse.response_code);
    printf("    desc: %s\n", parsedResponse.response_description);
    printf("    chunked: %d\n", parsedResponse.is_chunked);
    printf("    content length: %d\n", parsedResponse.content_length);
    for (int i = 0; i < parsedResponse.num_headers; i ++) {
        printf("    header name: %s; header value: %s\n", parsedResponse.headers[i].name, parsedResponse.headers[i].value);
    }
    printf("    body: %s\n", parsedResponse.response_body.data);
    
    char *xmlfortest = (
"<p>TEXT!</w></p>DATA");
    
    
    struct xml_response xml = XML_parseXmlNodes(
                                  XML_xmlDataFromString(
                                      parsedResponse.response_body.data
                                  )
                              );
    if (xml.error) {
        printf("Error while parsing xml. error code: %d\n", xml.error);
        exit(1);
    }
    
    recursiveDisplayXML(xml.list, 0);
    exit(0);*/
    
    struct nc_state browserState;
    browserState.currentPage = PAGE_EMPTY;
    browserState.numTextAreas = 0;
    browserState.numButtons = 0;
    browserState.numTexts = 0;
    browserState.selectables = (struct nc_selected *) calloc(0, sizeof(struct nc_selected));
    browserState.numSelectables = 0;
    browserState.selectableIndex = -1;

    initWindow();
    initializeDisplayObjects(&browserState);

    nc_onInitFinish(&browserState);

    if (url != NULL) {
        int len = strlen(url);
        struct nc_text_area *textarea = getTextAreaByDescriptor(&browserState, "urltextarea");
        for (int i = 0; i < len; i ++) {
            if (strlen(textarea->currentText) >= textarea->width) {
                textarea->scrolledAmount ++;
            }
            textarea->currentText[strlen(textarea->currentText)] = url[i];
            if (i > 1022) {
                break;
            }
        }
        ongotourl(&browserState, "<unused>");
    }

    eventLoop(&browserState);
    return 0;
}
