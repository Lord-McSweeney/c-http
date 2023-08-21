// Despite the file name, this is for general page navigation.

#ifndef _NAVIGATION
    #define _NAVIGATION 1

#include <string.h>
#include <stdlib.h>

#include "../render/display-handling.h"
#include "../http/http.h"
#include "../xml/html.h"
#include "../render/html2nc.h"

char *downloadAndOpenPage(struct nc_state *state, char *url, dataReceiveHandler handler, dataReceiveHandler finishHandler, int redirect_depth) {
    if (redirect_depth > 15) {
        return HTTP_makeStrCpy("Too many redirects (>15)");
    }

    char *userAgent = getTextAreaByDescriptor(state, "userAgent")->currentText;
    struct http_response parsedResponse = http_makeHTTPRequest(url, userAgent, handler, finishHandler, (void *) state);
    
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
        } else if (parsedResponse.error == 194) {
            return HTTP_makeStrCpy("Unsupported protocol.\n");
        } else if (parsedResponse.error == 195) {
            return HTTP_makeStrCpy("Invalid HTTP response (possibly HTTPS).\n");
        } else if (parsedResponse.error == 196) {
            return HTTP_makeStrCpy("Read access denied.\n");
        } else if (parsedResponse.error == 197) {
            return HTTP_makeStrCpy("File is a directory.\n");
        } else if (parsedResponse.error == 198) {
            return HTTP_makeStrCpy("No such file or directory.\n");
        } else if (parsedResponse.error == 199) {
            return HTTP_makeStrCpy("Invalid chunked response (possible memory corruption?).\n");
        } else {
            return HTTP_makeStrCpy("Error while connecting to host or receiving data from host.\n");
        }
    }

    if (parsedResponse.do_redirect) {
        strcat(getTextByDescriptor(state, "documentText")->text, "\nEncountered redirect to \"");
        strcat(getTextByDescriptor(state, "documentText")->text, parsedResponse.redirect);
        strcat(getTextByDescriptor(state, "documentText")->text, "\".");
        render_nc(state);
        return downloadAndOpenPage(state, parsedResponse.redirect, handler, finishHandler, redirect_depth + 1);
    }

    if (!parsedResponse.is_html) {
        char *lowerData = HTTP_toLowerCase(parsedResponse.response_body.data);
        if (strncmp(lowerData, "<!doctype html", 14)) {
            for (int i = 0; i < parsedResponse.num_headers; i ++) {
                free(parsedResponse.headers[i].name);
                free(parsedResponse.headers[i].value);
            }
            free(parsedResponse.headers);
            free(parsedResponse.response_description);
            free(lowerData);
            return parsedResponse.response_body.data;
        }
        free(lowerData);
    }

    strcat(getTextByDescriptor(state, "documentText")->text, "\nBeginning to parse page as HTML...");

    // todo: handle headers

    struct xml_response xml = XML_parseXmlNodes(
                                  XML_xmlDataFromString(
                                      parsedResponse.response_body.data
                                  )
                              );

    strcat(getTextByDescriptor(state, "documentText")->text, "\nDone parsing page as HTML.\nConverting HTML to rich text...");

    if (xml.error) {
        return HTTP_makeStrCpy("Encountered an error while parsing the page.");
    }

    struct html2nc_result result = htmlToText(xml.list, parsedResponse.response_body.data);
    char *total = (char *) calloc(strlen(result.text) + strlen(result.title) + 8, sizeof(char));
    strcpy(total, result.title);
    strcat(total, "\n\n");
    strcat(total, result.text);

    strcat(getTextByDescriptor(state, "documentText")->text, "\nDone parsing HTML as rich text.");

    free(parsedResponse.response_body.data);
    for (int i = 0; i < parsedResponse.num_headers; i ++) {
        free(parsedResponse.headers[i].name);
        free(parsedResponse.headers[i].value);
    }
    free(parsedResponse.headers);
    free(parsedResponse.response_description);
    free(result.title);
    free(result.text);
    if (parsedResponse.redirect) free(parsedResponse.redirect);

    recursiveFreeXML(xml.list);
    return total;
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

    for (int i = realState->numButtons - 1; i >= realState->initialButtons; i --) {
        realState->buttons[i].descriptor = NULL;
        realState->buttons[i].text = NULL;
        realState->buttons[i].visible = 0;
        realState->buttons[i].enabled = 0;
        realState->numButtons --;
    }

    realState->selectableIndex = -1;
    realState->currentPage = PAGE_DOCUMENT_LOADED;
    realState->globalScrollX = 0;
    realState->globalScrollY = 0;
    getTextByDescriptor(realState, "gotoURL")->visible = 0;
    getTextAreaByDescriptor(realState, "urltextarea")->visible = 0;
    getButtonByDescriptor(realState, "gotobutton")->visible = 0;
    getTextByDescriptor(state, "userAgentDetail")->visible = 0;
    getTextAreaByDescriptor(state, "userAgent")->visible = 0;
    getTextByDescriptor(realState, "documentText")->visible = 1;
    getTextByDescriptor(state, "helpText")->visible = 0;

    getTextByDescriptor(realState, "documentText")->text = (char *) calloc(8192, sizeof(char));
    strcpy(getTextByDescriptor(realState, "documentText")->text, "The document is downloading...");
    render_nc(realState);
    char *data = downloadAndOpenPage(state, getTextAreaByDescriptor(realState, "urltextarea")->currentText, onReceiveData, onFinishData, 0);
    free(getTextByDescriptor(realState, "documentText")->text);
    getTextByDescriptor(realState, "documentText")->text = data;
    if (data == NULL) {
        getTextByDescriptor(realState, "documentText")->text = HTTP_makeStrCpy("ERROR: Serialized document was NULL!");
    }
    realState->currentPageUrl = getTextAreaByDescriptor(realState, "urltextarea")->currentText;
}

void onlinkpressed(void *state, char *url) {
    if (strncmp(url, "linkto_", 7)) {
        return;
    }

    struct nc_state *realState = (struct nc_state *) state;

    for (int i = realState->numButtons - 1; i >= realState->initialButtons; i --) {
        realState->buttons[i].descriptor = NULL;
        realState->buttons[i].text = NULL;
        realState->buttons[i].visible = 0;
        realState->buttons[i].enabled = 0;
        realState->numButtons --;
    }

    char *realURL = url + 8;

    struct http_url *curURL = http_url_from_string(realState->currentPageUrl);
    char *absoluteURL = http_resolveRelativeURL(curURL, realState->currentPageUrl, realURL);

    realState->selectableIndex = -1;
    realState->currentPage = PAGE_DOCUMENT_LOADED;
    realState->globalScrollX = 0;
    realState->globalScrollY = 0;
    getTextByDescriptor(realState, "gotoURL")->visible = 0;
    getTextAreaByDescriptor(realState, "urltextarea")->visible = 0;
    getButtonByDescriptor(realState, "gotobutton")->visible = 0;
    getTextByDescriptor(state, "userAgentDetail")->visible = 0;
    getTextAreaByDescriptor(state, "userAgent")->visible = 0;
    getTextByDescriptor(realState, "documentText")->visible = 1;
    getTextByDescriptor(state, "helpText")->visible = 0;

    getTextByDescriptor(realState, "documentText")->text = (char *) calloc(8192, sizeof(char));
    strcpy(getTextByDescriptor(realState, "documentText")->text, "The document is downloading...");
    render_nc(realState);
    char *data = downloadAndOpenPage(state, absoluteURL, onReceiveData, onFinishData, 0);
    free(getTextByDescriptor(realState, "documentText")->text);
    getTextByDescriptor(realState, "documentText")->text = data;
    if (data == NULL) {
        getTextByDescriptor(realState, "documentText")->text = HTTP_makeStrCpy("ERROR: Serialized document was NULL!");
    }
    realState->currentPageUrl = absoluteURL;
}

#endif
