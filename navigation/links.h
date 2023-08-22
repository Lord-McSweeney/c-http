// Despite the file name, this is for general page navigation.

#ifndef _NAVIGATION
    #define _NAVIGATION 1

#include <string.h>
#include <stdlib.h>

#include "../http/http.h"
#include "../http/url.h"
#include "../render/display-handling.h"
#include "../render/html2nc.h"
#include "../utils/string.h"
#include "../xml/html.h"

char *downloadAndOpenPage(struct nc_state *state, char *url, dataReceiveHandler handler, dataReceiveHandler finishHandler, int redirect_depth) {
    if (redirect_depth > 15) {
        return makeStrCpy("Too many redirects (>15)");
    }

    char *userAgent = getTextAreaByDescriptor(state, "userAgent")->currentText;
    struct http_response parsedResponse = http_makeHTTPRequest(url, userAgent, handler, finishHandler, (void *) state);
    
    if (parsedResponse.error) {
        if (parsedResponse.error == 4) {
            return makeStrCpy("Please enter a valid URL.\n");
        } else if (parsedResponse.error < 4) {
            return makeStrCpy("Malformed HTTP response.\n");
        } else if (parsedResponse.error == 5) {
            return makeStrCpy("Host not found.\n");
        } else if (parsedResponse.error == 6) {
            return makeStrCpy("Error while resolving domain.\n");
        } else if (parsedResponse.error == 7) {
            return makeStrCpy("Temporary error in name resolution.\n");
        } else if (parsedResponse.error == 101) {
            return makeStrCpy("Network unreachable.\n");
        } else if (parsedResponse.error == 104) {
            return makeStrCpy("Connection reset by peer.\n");
        } else if (parsedResponse.error == 111) {
            return makeStrCpy("Connection refused.\n");
        } else if (parsedResponse.error == 113) {
            return makeStrCpy("No route to host.\n");
        } else if (parsedResponse.error == 192) {
            return makeStrCpy("Error initializing SSL/TLS.\n");
        } else if (parsedResponse.error == 193) {
            return makeStrCpy("SSL/TLS error.\n");
        } else if (parsedResponse.error == 194) {
            return makeStrCpy("Unsupported protocol.\n");
        } else if (parsedResponse.error == 195) {
            return makeStrCpy("Invalid HTTP response (possibly HTTPS).\n");
        } else if (parsedResponse.error == 196) {
            return makeStrCpy("Read access denied.\n");
        } else if (parsedResponse.error == 197) {
            return makeStrCpy("File is a directory.\n");
        } else if (parsedResponse.error == 198) {
            return makeStrCpy("No such file or directory.\n");
        } else if (parsedResponse.error == 199) {
            return makeStrCpy("Invalid chunked response (possible memory corruption?).\n");
        } else {
            return makeStrCpy("Error while connecting to host or receiving data from host.\n");
        }
    }

    if (parsedResponse.do_redirect) {
        strcat(getTextByDescriptor(state, "documentText")->text, "\nEncountered redirect to \"");
        strcat(getTextByDescriptor(state, "documentText")->text, parsedResponse.redirect);
        strcat(getTextByDescriptor(state, "documentText")->text, "\".");
        render_nc(state);

        struct http_url *curURL = http_url_from_string(state->currentPageUrl);
        char *absoluteURL = http_resolveRelativeURL(curURL, state->currentPageUrl, parsedResponse.redirect);
        state->currentPageUrl = absoluteURL;
        getTextAreaByDescriptor(state, "urlField")->currentText = absoluteURL;

        return downloadAndOpenPage(state, absoluteURL, handler, finishHandler, redirect_depth + 1);
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
    render_nc(state);

    // todo: handle headers

    struct xml_response xml = XML_parseXmlNodes(
                                  XML_xmlDataFromString(
                                      parsedResponse.response_body.data
                                  )
                              );

    strcat(getTextByDescriptor(state, "documentText")->text, "\nDone parsing page as HTML.\nConverting HTML to rich text...");
    render_nc(state);

    if (xml.error) {
        char *t = (char *) calloc(strlen("Encountered an error while parsing the page. Error code: %d.") + 1, sizeof(char));
        sprintf(t, "Encountered an error while parsing the page. Error code: %d.", xml.error);
        return t;
    }

    struct html2nc_result result = htmlToText(xml.list, parsedResponse.response_body.data);
    char *total = (char *) calloc(strlen(result.text) + strlen(url) + strlen(result.title) + 8, sizeof(char));
    strcpy(total, result.title);
    strcat(total, "\n");
    strcat(total, url);
    strcat(total, "\n");
    strcat(total, "\\H");
    strcat(total, "\n");
    strcat(total, result.text);

    strcat(getTextByDescriptor(state, "documentText")->text, "\nDone parsing HTML as rich text.");
    render_nc(state);

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
    getTextAreaByDescriptor(state, "urlField")->visible = 0;
    getTextByDescriptor(state, "helpText")->visible = 0;

    realState->currentPageUrl = getTextAreaByDescriptor(realState, "urltextarea")->currentText;
    getTextAreaByDescriptor(state, "urlField")->currentText = realState->currentPageUrl;

    getTextByDescriptor(realState, "documentText")->text = (char *) calloc(8192, sizeof(char));
    strcpy(getTextByDescriptor(realState, "documentText")->text, "The document is downloading...");
    render_nc(realState);
    char *data = downloadAndOpenPage(state, getTextAreaByDescriptor(realState, "urltextarea")->currentText, onReceiveData, onFinishData, 0);
    free(getTextByDescriptor(realState, "documentText")->text);
    getTextByDescriptor(realState, "documentText")->text = data;
    if (data == NULL) {
        getTextByDescriptor(realState, "documentText")->text = makeStrCpy("ERROR: Serialized document was NULL!");
    }

    getTextAreaByDescriptor(state, "urlField")->visible = 1;
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

    char *realURL = url + 9;

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
    getTextAreaByDescriptor(state, "urlField")->visible = 0;
    getTextByDescriptor(state, "helpText")->visible = 0;

    realState->currentPageUrl = absoluteURL;
    getTextAreaByDescriptor(state, "urlField")->currentText = absoluteURL;

    getTextByDescriptor(realState, "documentText")->text = (char *) calloc(8192, sizeof(char));
    strcpy(getTextByDescriptor(realState, "documentText")->text, "The document is downloading...");
    render_nc(realState);
    char *data = downloadAndOpenPage(state, absoluteURL, onReceiveData, onFinishData, 0);
    free(getTextByDescriptor(realState, "documentText")->text);
    getTextByDescriptor(realState, "documentText")->text = data;
    if (data == NULL) {
        getTextByDescriptor(realState, "documentText")->text = makeStrCpy("ERROR: Serialized document was NULL!");
    }

    getTextAreaByDescriptor(state, "urlField")->visible = 1;
}

#endif
