// ncurses ref: https://www.sbarjatiya.com/notes_wiki/index.php/Using_ncurses_library_with_C
// https://jbwyatt.com/ncurses.html is much better tho

// using gcc, compile with -lncurses

/*
timeline
day 1: implement http url parsing
day 2: implement basic http response parsing
day 3: implement http chunked response parsing
day 4: implement xml parser
day 5: implement ncurses renderer
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "http/http.h"
#include "render/display-handling.h"
#include "xml/html.h"
#include "render/html2nc.h"

// error codes:
/*
0: no error
1: malformed http response
2: HTTP version did not match
3: malformed http status
4: URL parse error
5: Failed to lookup host
6: Error while obtaining host IP
7: Temporary error in name resolution
*/

char *downloadAndOpenPage(struct nc_state *state, char *url, dataReceiveHandler handler, dataReceiveHandler finishHandler) {
    struct http_response parsedResponse = http_makeHTTPRequest(url, handler, finishHandler, (void *) state);
    
    if (parsedResponse.error) {
        if (parsedResponse.error == 4) {
            return HTTP_makeStrCpy("Please enter a valid URL.\n");
        } else if (parsedResponse.error < 4) {
            return HTTP_makeStrCpy("Malformed HTTP response.\n");
        } else if (parsedResponse.error == 5) {
            return HTTP_makeStrCpy("Host not found.\n");
        } else if (parsedResponse.error == 6) {
            return HTTP_makeStrCpy("Error while resolving domain.\n");
        } else if (parsedResponse.error == 7) {
            return HTTP_makeStrCpy("Temporary error in name resolution.\n");
        } else if (parsedResponse.error == 101) {
            return HTTP_makeStrCpy("Network unreachable.\n");
        } else if (parsedResponse.error == 104) {
            return HTTP_makeStrCpy("Connection reset by peer.\n");
        } else if (parsedResponse.error == 111) {
            return HTTP_makeStrCpy("Connection refused.\n");
        } else if (parsedResponse.error == 113) {
            return HTTP_makeStrCpy("No route to host.\n");
        } else if (parsedResponse.error == 199) {
            return HTTP_makeStrCpy("Invalid chunked response (possible memory corruption?).\n");
        } else if (parsedResponse.error == 196) {
            return HTTP_makeStrCpy("Read access denied.\n");
        } else if (parsedResponse.error == 197) {
            return HTTP_makeStrCpy("File is a directory.\n");
        } else if (parsedResponse.error == 198) {
            return HTTP_makeStrCpy("No such file or directory.\n");
        } else {
            return HTTP_makeStrCpy("Error while connecting to host or receiving data from host.\n");
        }
    }

    if (!parsedResponse.is_html) {
        return parsedResponse.response_body.data;
    }

    // todo: handle headers

    struct xml_response xml = XML_parseXmlNodes(
                                  XML_xmlDataFromString(
                                      parsedResponse.response_body.data
                                  )
                              );
    if (xml.error) {
        return HTTP_makeStrCpy("Encountered an error while parsing the page.");
    }

    struct html2nc_result result = htmlToText(xml.list, parsedResponse.response_body.data);
    char *total = (char *) calloc(strlen(result.text) + strlen(result.title) + 8, sizeof(char));
    strcpy(total, result.title);
    strcat(total, "\n\n");
    strcat(total, result.text);

    free(parsedResponse.response_body.data);
    for (int i = 0; i < parsedResponse.num_headers; i ++) {
        free(parsedResponse.headers[i].name);
        free(parsedResponse.headers[i].value);
    }
    free(parsedResponse.headers);
    free(result.title);
    free(result.text);
    recursiveFreeXML(xml.list);
    return total;
    /*
    printf("response details:\n");
    printf("    code: %d\n", parsedResponse.response_code);
    printf("    desc: %s\n", parsedResponse.response_description);
    printf("    chunked: %d\n", parsedResponse.is_chunked);
    printf("    content length: %d\n", parsedResponse.content_length);
    for (int i = 0; i < parsedResponse.num_headers; i ++) {
        printf("    header name: %s; header value: %s\n", parsedResponse.headers[i].name, parsedResponse.headers[i].value);
    }*/
}

// gets called with current state pointer and pressed button's descriptor

void onReceiveData(void *ptrState) {
    struct nc_state *state = (struct nc_state *) ptrState;
    strcat(getTextByDescriptor(state, "documentText")->text, ".");
    render_nc(state);
}

void onFinishData(void *ptrState) {
    struct nc_state *state = (struct nc_state *) ptrState;
    strcat(getTextByDescriptor(state, "documentText")->text, "\nDownloaded entire document.");
    render_nc(state);
}

void ongotourl(void *state, char *_) {
    struct nc_state *realState = (struct nc_state *) state;
    realState->currentPage = PAGE_DOCUMENT_LOADED;
    getTextByDescriptor(realState, "gotoURL")->visible = 0;
    getTextAreaByDescriptor(realState, "urltextarea")->visible = 0;
    getButtonByDescriptor(realState, "gotobutton")->visible = 0;
    getTextByDescriptor(realState, "documentText")->visible = 1;
    
    getTextByDescriptor(realState, "documentText")->text = (char *) calloc(8192, sizeof(char));
    strcpy(getTextByDescriptor(realState, "documentText")->text, "The document is downloading...");
    render_nc(realState);
    char *data = downloadAndOpenPage(state, getTextAreaByDescriptor(realState, "urltextarea")->currentText, onReceiveData, onFinishData);
    free(getTextByDescriptor(realState, "documentText")->text);
    getTextByDescriptor(realState, "documentText")->text = data;
}

void initializeDisplayObjects(struct nc_state *state) {
    createNewText(state, 1, 1, "Go to a URL:", "gotoURL");
    createNewText(state, 0, 0, "Document is loading...", "documentText");
    createNewTextarea(state, 1, 3, 29, 1, "urltextarea");
    createNewButton(state, 1, 5, "OK", ongotourl, "gotobutton");
    getTextByDescriptor(state, "gotoURL")->visible = 0;
    getButtonByDescriptor(state, "gotobutton")->visible = 0;
    getTextAreaByDescriptor(state, "urltextarea")->visible = 0;
    getTextByDescriptor(state, "documentText")->visible = 0;
}

void openGotoPageDialog(struct nc_state *state) {
    struct nc_text_area *textarea = getTextAreaByDescriptor(state, "urltextarea");
    state->currentPage = PAGE_GOTO_DIALOG;
    getTextByDescriptor(state, "gotoURL")->visible = 1;
    textarea->visible = 1;
    getButtonByDescriptor(state, "gotobutton")->visible = 1;
    getTextByDescriptor(state, "documentText")->visible = 0;
    
    int curlen = strlen(textarea->currentText);
    while(curlen > 0) {
        textarea->currentText[curlen - 1] = '\0';
        curlen = strlen(textarea->currentText);
    }
}

void closeGotoPageDialog(struct nc_state *state) {
    state->currentPage = PAGE_EMPTY;
    getTextByDescriptor(state, "gotoURL")->visible = 0;
    getTextAreaByDescriptor(state, "urltextarea")->visible = 0;
    getButtonByDescriptor(state, "gotobutton")->visible = 0;
    
    struct nc_text_area *textarea = getTextAreaByDescriptor(state, "urltextarea");
}

void closeDocumentPage(struct nc_state *state) {
    state->currentPage = PAGE_EMPTY;
    getTextByDescriptor(state, "documentText")->visible = 0;
}

void closeCurrentWindow(struct nc_state *state) {
    switch(state->currentPage) {
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
    int h, w;
    switch(ch) {
        case 15: // CTRL+O
            openGotoPageDialog(browserState);
            break;
        case '\t': // tab
            browserState->selectableIndex ++;
            if (browserState->selectableIndex >= browserState->numSelectables) {
                browserState->selectableIndex = 0;
            }
            break;
        case 24: // CTRL+X
            closeCurrentWindow(browserState);
            break;
    }
    struct nc_text_area *textarea = getCurrentSelectedTextarea(browserState);
    if (textarea != NULL) {
        int curlen = strlen(textarea->currentText);
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '.' || ch == '/' || ch == ':' || ch == '#' || ch == '-' || ch == '=' || ch == '?' || ch == '_') {
            if (curlen >= textarea->width) {
                textarea->scrolledAmount ++;
            }
            textarea->currentText[curlen] = ch;
        } else if (ch == 7) {
            if (curlen > 0) {
                textarea->currentText[curlen - 1] = '\0';
            }
            if (textarea->scrolledAmount > 0) {
                textarea->scrolledAmount --;
            }
        } else {/*
            textarea->currentText[0] = ch/10 + 48;
            textarea->currentText[1] = ch%10 + 48;//("ch: %d\n", ch);
            textarea->currentText[2] = 0;*/
        }
    } else if (browserState->currentPage == PAGE_DOCUMENT_LOADED) {
        if (ch == 3) {
            if (getTextByDescriptor(browserState, "documentText")->y < 0) {
               getTextByDescriptor(browserState, "documentText")->y ++;
            }
        }
        if (ch == 2) {
            if (/*todo: make sure the page doesn't scroll too far down*/true) {
                getTextByDescriptor(browserState, "documentText")->y --;
            }
        }
        if (ch == 4) {
            // left
            if (getTextByDescriptor(browserState, "documentText")->x < 0) {
               getTextByDescriptor(browserState, "documentText")->x ++;
            }
        }
        if (ch == 5) {
            if (/*todo: make sure the page doesn't scroll too far right*/true) {
                getTextByDescriptor(browserState, "documentText")->x --;
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

void eventLoop(struct nc_state *browserState) {
    while (1) {
        char ch;
        updateFocus_nc(browserState);
        render_nc(browserState);
        if (ch = getch()) {
            onKeyPress(browserState, ch);
        }
        
        switch(browserState->currentPage) {
            case PAGE_DOCUMENT_LOADED:
                break;
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

int main(int argc, char **argv) {/*
    char *url = NULL;
    if (argc == 2) {
        url = argv[1];
    }

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
    //browserState.currentLoadedDOM = xml;
    browserState.numTextAreas = 0;
    browserState.numButtons = 0;
    browserState.numTexts = 0;
    browserState.selectables = (struct nc_selected *) calloc(0, sizeof(struct nc_selected));
    browserState.numSelectables = 0;
    browserState.selectableIndex = 0;
    initWindow(&browserState);
    initializeDisplayObjects(&browserState);
    eventLoop(&browserState);
    /*
    if (url != NULL) {
        getTextAreaByDescriptor(&browserState, "urltextarea")->currentText = url;
        ongotourl(&browserState, "<unused>");
    }
    */
    return 0;
}
