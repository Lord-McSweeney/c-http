// main.c
// Compile with gcc main.c -lncurses -lssl -lcrypto
#include <stdlib.h>

#include "http/http.h"
#include "navigation/links.h"
#include "render/display-handling.h"
#include "utils/string.h"

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
    createNewText(state, 0, 0, "This is the browser's home page.\n\nNavigation tools:\nCTRL+O: Go to site\nCTRL+X: Close current page\nUp/Down arrows: scroll current document\nTab/Shift+Tab (or left arrow/right arrow): Cycle through buttons/links/text fields\n\nWhen a document is loaded:\nCTRL+O: Go to site (same as UI)\nCTRL+K: View host cookies\nCTRL+X: Close document\n\nCookies are not saved to disk.\n\nCreated by uqers.", "helpText");
    createNewTextarea(state, 1, 3, 29, 1, "urltextarea");
    createNewButton(state, 1, 5, "OK", ongotourl, "gotobutton");
    createNewText(state, 1, 13, "Use a custom user agent", "userAgentDetail");
    createNewText(state, 0, 0, "Cookies:\n\n(not initialized)", "cookiesDetail");
    createNewTextarea(state, 1, 14, 29, 1, "userAgent");
    createNewTextarea(state, 0, 1, -5, 1, "urlField");
    createNewButton(state, -3, 1, "Go!", ongotourlfield, "smallgobutton");
    getTextByDescriptor(state, "gotoURL")->visible = 0;
    getButtonByDescriptor(state, "gotobutton")->visible = 0;
    getTextAreaByDescriptor(state, "urltextarea")->visible = 0;
    getTextByDescriptor(state, "documentText")->visible = 0;
    getTextByDescriptor(state, "helpText")->visible = 1;
    getTextByDescriptor(state, "userAgentDetail")->visible = 0;
    getTextByDescriptor(state, "cookiesDetail")->visible = 0;
    getTextAreaByDescriptor(state, "userAgent")->visible = 0;
    setTextOf(getTextAreaByDescriptor(state, "userAgent"), "uqers");
    getTextAreaByDescriptor(state, "urlField")->visible = 0;
    getButtonByDescriptor(state, "smallgobutton")->visible = 0;
    getTextAreaByDescriptor(state, "userAgent")->allowMoreChars = 1;
}

void openGotoPageDialog(struct nc_state *state) {
    state->selectableIndex = -1;
    struct nc_text_area *textarea = getTextAreaByDescriptor(state, "urltextarea");
    if (state->currentPage == PAGE_DOCUMENT_LOADED) {
        state->currentPage = PAGE_GOTO_OVER_DOCUMENT;
        hideButtons(state);
    } else {
        freeButtons(state);
        state->currentPage = PAGE_GOTO_DIALOG;
    }
    getTextByDescriptor(state, "helpText")->visible = 0;
    getTextByDescriptor(state, "gotoURL")->visible = 1;
    textarea->visible = 1;
    getButtonByDescriptor(state, "gotobutton")->visible = 1;
    getTextByDescriptor(state, "documentText")->visible = 0;
    getTextAreaByDescriptor(state, "urlField")->visible = 0;
    getButtonByDescriptor(state, "smallgobutton")->visible = 0;
    getTextByDescriptor(state, "userAgentDetail")->visible = 1;
    getTextAreaByDescriptor(state, "userAgent")->visible = 1;
    state->selectableIndex = 0;

    // Should this save the global scroll when appearing over the current document?
    state->globalScrollX = 0;
    state->globalScrollY = 0;

    clearCharsOf(textarea);
    textarea->scrolledAmount = 0;
}

void openCookieDialog(struct nc_state *state) {
    struct nc_text *cookiesDetail = getTextByDescriptor(state, "cookiesDetail");

    state->selectableIndex = -1;
    state->currentPage = PAGE_VIEW_COOKIES;
    hideButtons(state);
    getTextByDescriptor(state, "helpText")->visible = 0;
    getTextByDescriptor(state, "gotoURL")->visible = 0;
    getButtonByDescriptor(state, "gotobutton")->visible = 0;
    getTextByDescriptor(state, "documentText")->visible = 0;
    getTextAreaByDescriptor(state, "urlField")->visible = 0;
    getButtonByDescriptor(state, "smallgobutton")->visible = 0;
    getTextByDescriptor(state, "userAgentDetail")->visible = 0;
    getTextAreaByDescriptor(state, "userAgent")->visible = 0;
    cookiesDetail->visible = 1;

    // Should this save the global scroll?
    state->globalScrollX = 0;
    state->globalScrollY = 0;

    char *hostname = http_url_from_string(state->currentPageUrl)->hostname;
    char *cookieText = HTTP_cookieStoreToStringPretty(HTTP_getGlobalCookieStore(), hostname);

    cookiesDetail->text = (char *) calloc(strlen(cookieText) + 16, sizeof(char));
    strcpy(cookiesDetail->text, "Cookies: \n\n");
    strcat(cookiesDetail->text, cookieText);
}

void closeCookieDialog(struct nc_state *state) {
    state->selectableIndex = -1;
    state->currentPage = PAGE_DOCUMENT_LOADED;
    getTextByDescriptor(state, "documentText")->visible = 1;
    getTextAreaByDescriptor(state, "urlField")->visible = 1;
    getButtonByDescriptor(state, "smallgobutton")->visible = 1;
    showButtons(state);
    getTextByDescriptor(state, "gotoURL")->visible = 0;
    getTextAreaByDescriptor(state, "urltextarea")->visible = 0;
    getButtonByDescriptor(state, "gotobutton")->visible = 0;
    getTextByDescriptor(state, "userAgentDetail")->visible = 0;
    getTextAreaByDescriptor(state, "userAgent")->visible = 0;
    getTextByDescriptor(state, "cookiesDetail")->visible = 0;
}

void closeGotoPageDialog(struct nc_state *state) {
    state->selectableIndex = -1;
    if (state->currentPage == PAGE_GOTO_OVER_DOCUMENT) {
        state->currentPage = PAGE_DOCUMENT_LOADED;
        getTextByDescriptor(state, "documentText")->visible = 1;
        getTextAreaByDescriptor(state, "urlField")->visible = 1;
        getButtonByDescriptor(state, "smallgobutton")->visible = 1;
        showButtons(state);
    } else {
        freeButtons(state);
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
    getTextAreaByDescriptor(state, "urlField")->visible = 0;
    getButtonByDescriptor(state, "smallgobutton")->visible = 0;
    getTextByDescriptor(state, "documentText")->text = makeStrCpy("");
    state->currentPageUrl = NULL;
    freeButtons(state);

    // Should this save the global scroll when appearing over the current document?
    state->globalScrollX = 0;
    state->globalScrollY = 0;
}

void closeCurrentWindow(struct nc_state *state) {
    switch(state->currentPage) {
        case PAGE_GOTO_OVER_DOCUMENT:
        case PAGE_GOTO_DIALOG:
            closeGotoPageDialog(state);
            break;
        case PAGE_VIEW_COOKIES:
            closeCookieDialog(state);
            break;
        case PAGE_DOCUMENT_LOADED:
            closeDocumentPage(state);
            break;
        case PAGE_EMPTY:
            endProgram();
            break;
    }
}

int isCharAllowed(int is_extended, char ch) {
    int cur = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '.' || ch == '/' || ch == ':' || ch == '#' || ch == '-' || ch == '=' || ch == '?' || ch == '_' || ch == ',' || ch == '<' || ch == '>' || ch == '%' || ch == '&' || ch == '+' || ch == ';' || ch == '~';
    if (is_extended) {
        cur = cur || (ch == ' ' || ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '^' || ch == '$');
    }
    return cur;
}

void onKeyPress(struct nc_state *browserState, int ch) {
    switch(ch) {
        case 11:
            if (browserState->currentPage == PAGE_DOCUMENT_LOADED) {
                openCookieDialog(browserState);
            }
            break;
        case 15: // CTRL+O
            if (browserState->currentPage != PAGE_VIEW_COOKIES) {
                openGotoPageDialog(browserState);
            }
            break;
        case KEY_BTAB: // SHIFT+TAB
        case KEY_LEFT:
            browserState->selectableIndex --;
            if (browserState->selectableIndex < 0) {
                browserState->selectableIndex = browserState->numSelectables - 1;
            }
            int i1 = 0;
            while(!isSelectable(browserState, &browserState->selectables[browserState->selectableIndex])) {
                browserState->selectableIndex --;
                if (browserState->selectableIndex < 0) {
                    browserState->selectableIndex = browserState->numSelectables - 1;
                }
                i1 ++;
                if (i1 > browserState->numSelectables + 1) {
                    // all selectables are disabled
                    browserState->selectableIndex = -1;
                    break;
                }
            }
            browserState->shouldCheckAutoScroll = 1;
            break;
        case '\t':
        case KEY_RIGHT:
            browserState->selectableIndex ++;
            if (browserState->selectableIndex >= browserState->numSelectables) {
                browserState->selectableIndex = 0;
            }
            int i2 = 0;
            while(!isSelectable(browserState, &browserState->selectables[browserState->selectableIndex])) {
                browserState->selectableIndex ++;
                if (browserState->selectableIndex >= browserState->numSelectables) {
                    browserState->selectableIndex = 0;
                }
                i2 ++;
                if (i2 > browserState->numSelectables + 1) {
                    // all selectables are disabled
                    browserState->selectableIndex = -1;
                    break;
                }
            }
            browserState->shouldCheckAutoScroll = 1;
            break;
        case 24: // CTRL+X
            closeCurrentWindow(browserState);
            break;
    }
    struct nc_text_area *textarea = getCurrentSelectedTextarea(browserState);
    if (textarea != NULL) {
        if (isCharAllowed(textarea->allowMoreChars, ch)) {
            appendCharTo(textarea, ch);
        } else if (ch == KEY_BACKSPACE) { // backspace
            popCharFrom(textarea);
        }
        if (ch != KEY_UP && ch != KEY_DOWN) {
            browserState->shouldCheckAutoScroll = 1;
        }
    }
    if (browserState->currentPage == PAGE_DOCUMENT_LOADED) {
        struct nc_text *documentText = getTextByDescriptor(browserState, "documentText");
        if (ch == KEY_UP) {
            if (browserState->globalScrollY < 0) {
               browserState->globalScrollY ++;
            }
        } else if (ch == KEY_DOWN) {
            if (/*todo: make sure the page doesn't scroll too far down*/true) {
               browserState->globalScrollY --;
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
        int ch;
        updateFocus_nc(state);

        for (int i = 0; i < state->numButtons; i ++) {
            if (state->buttons[i].onclick == nc_templinkpresshandler) {
                state->buttons[i].onclick = onlinkpressed;
            }
        }
        render_nc(state);
        state->shouldCheckAutoScroll = 0;
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
    struct http_response parsedResponse = http_makeHTTPRequest("file:///home/batuhan/test.html", "uqers", NULL, NULL, NULL);
    
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
    browserState.globalScrollX = 0;
    browserState.globalScrollY = 0;
    browserState.shouldCheckAutoScroll = 0;
    browserState.currentPageUrl = NULL;

    browserState.buttons = (struct nc_button *) calloc(1, sizeof(struct nc_button));

    initWindow();
    initializeDisplayObjects(&browserState);

    nc_onInitFinish(&browserState);

    HTTP_setGlobalCookieStore(HTTP_makeCookieStore());

    if (url != NULL) {
        struct nc_text_area *textarea = getTextAreaByDescriptor(&browserState, "urltextarea");
        struct nc_text_area *textarea2 = getTextAreaByDescriptor(&browserState, "urlField");
        setTextOf(textarea, url);
        setTextOf(textarea2, url);
        ongotourl(&browserState, "<unused>");
    }

    eventLoop(&browserState);
    return 0;
}
