// Despite the file name, this is for general page navigation.

#ifndef _NAVIGATION
    #define _NAVIGATION 1

#include "../http/http.h"
#include "../http/response.h"
#include "../http/url.h"
#include "../render/display-handling.h"
#include "../render/html2nc.h"
#include "../utils/string.h"
#include "../xml/html.h"

#include "download.h"


void onStyleSheetStart(void *ptr, char *href) {
    struct nc_state *state = (struct nc_state *) ptr;
    strcat(getTextByDescriptor(state, "documentText")->text, "\nDownloading stylesheet from \"");
    strcat(getTextByDescriptor(state, "documentText")->text, href);
    strcat(getTextByDescriptor(state, "documentText")->text, "\".");
    render_nc(state);
}

void onStyleSheetProgress(void *ptr) {
    struct nc_state *state = (struct nc_state *) ptr;
    strcat(getTextByDescriptor(state, "documentText")->text, ".");
    render_nc(state);
}

void onStyleSheetComplete(void *ptr) {
    struct nc_state *state = (struct nc_state *) ptr;
    strcat(getTextByDescriptor(state, "documentText")->text, "\n    Finished downloading stylesheet.\n");
    render_nc(state);
}

char *onStyleSheetError(void *ptr, int _) {
    struct nc_state *state = (struct nc_state *) ptr;
    strcat(getTextByDescriptor(state, "documentText")->text, "\n    Error downloading stylesheet.\n");
    render_nc(state);
    return makeStrCpy("");
}


char *onerrorhandler(void *state, int code) {
    char *err;
    if (code == 4) {
        err = makeStrCpy("Please enter a valid URL.\n");
    } else if (code < 4) {
        err = makeStrCpy("Malformed HTTP response.\n");
    } else if (code == 5) {
        err = makeStrCpy("Host not found.\n");
    } else if (code == 6) {
        err = makeStrCpy("Error while resolving domain.\n");
    } else if (code == 7) {
        err = makeStrCpy("Temporary error in name resolution.\n");
    } else if (code == 101) {
        err = makeStrCpy("Network unreachable.\n");
    } else if (code == 104) {
        err = makeStrCpy("Connection reset by peer.\n");
    } else if (code == 111) {
        err = makeStrCpy("Connection refused.\n");
    } else if (code == 113) {
        err = makeStrCpy("No route to host.\n");
    } else if (code == 192) {
        err = makeStrCpy("Error initializing SSL/TLS.\n");
    } else if (code == 193) {
        err = makeStrCpy("SSL/TLS error.\n");
    } else if (code == 194) {
        err = makeStrCpy("Unsupported protocol.\n");
    } else if (code == 195) {
        err = makeStrCpy("Invalid HTTP response (possibly HTTPS).\n");
    } else if (code == 196) {
        err = makeStrCpy("Read access denied.\n");
    } else if (code == 197) {
        err = makeStrCpy("File is a directory.\n");
    } else if (code == 198) {
        err = makeStrCpy("No such file or directory.\n");
    } else if (code == 199) {
        err = makeStrCpy("Invalid chunked response (possible memory corruption?).\n");
    } else {
        err = makeStrCpy("Error while connecting to host or receiving data from host.\n");
    }
    return err;
}

char *onredirecthandler(void *ptr, char *to, int num) {
    struct nc_state *state = (struct nc_state *) ptr;
    if (num > 15) {
        return makeStrCpy("Too many redirects (>15)");
    }
    strcat(getTextByDescriptor(state, "documentText")->text, "\nEncountered redirect to \"");
    strcat(getTextByDescriptor(state, "documentText")->text, to);
    strcat(getTextByDescriptor(state, "documentText")->text, "\".");
    render_nc(state);

    return NULL;
}

void onredirectsuccesshandler(void *ptr, char *to, int num) {
    struct nc_state *state = (struct nc_state *) ptr;
    state->currentPageUrl = makeStrCpy(to);
    setTextOf(getTextAreaByDescriptor(state, "urlField"), to);
}

char *onredirecterrorhandler(void *ptr, char *to, int num) {
    return makeStrCpy("Invalid redirected URL.");
}

char *downloadAndOpenPage(struct nc_state *state, char *url, dataReceiveHandler handler, dataReceiveHandler finishHandler, int redirect_depth) {
    struct http_response parsedResponse = downloadPage(
        (void *) state,
        getTextAreaByDescriptor(state, "userAgent")->currentText,
        &url,
        handler,
        finishHandler,
        0,
        onerrorhandler,
        onredirecthandler,
        onredirectsuccesshandler,
        onredirecterrorhandler
    );

    if (!parsedResponse.is_html) {
        char *lowerData = HTTP_toLowerCase(parsedResponse.response_body.data);
        if (strncmp(lowerData, "<!doctype html", 14)) {
            for (int i = 0; i < parsedResponse.num_headers; i ++) {
                free(parsedResponse.headers[i].name);
                if (parsedResponse.headers[i].value) free(parsedResponse.headers[i].value);
            }
            free(parsedResponse.headers);
            free(parsedResponse.response_description);
            free(lowerData);

            char *total = (char *) calloc(parsedResponse.response_body.length + strlen(url) + 32, sizeof(char));
            strcpy(total, "Page has no title\n\n\\H\n");
            strcat(total, doubleStringBackslashes(parsedResponse.response_body.data));
            return total;
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

    struct html2nc_result result = htmlToText(
        xml.list,
        parsedResponse.response_body.data,
        url,
        (void *) state,
        onStyleSheetStart,
        onStyleSheetProgress,
        onStyleSheetComplete,
        onStyleSheetError
    );
    char *total = (char *) calloc(strlen(result.text) + strlen(url) + strlen(result.title) + 8, sizeof(char));
    strcpy(total, result.title);
    strcat(total, "\n\n\\H\n");
    strcat(total, result.text);

    strcat(getTextByDescriptor(state, "documentText")->text, "\nDone parsing HTML as rich text.");
    render_nc(state);

    free(parsedResponse.response_body.data);
    for (int i = 0; i < parsedResponse.num_headers; i ++) {
        free(parsedResponse.headers[i].name);
        if (parsedResponse.headers[i].value) free(parsedResponse.headers[i].value);
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
    freeButtons(realState);

    realState->selectableIndex = -1;
    realState->currentPage = PAGE_DOCUMENT_LOADED;
    realState->globalScrollX = 0;
    realState->globalScrollY = 0;
    getTextByDescriptor(realState, "gotoURL")->visible = 0;
    getTextAreaByDescriptor(realState, "urltextarea")->visible = 0;
    getButtonByDescriptor(realState, "gotobutton")->visible = 0;
    getTextByDescriptor(realState, "userAgentDetail")->visible = 0;
    getTextAreaByDescriptor(realState, "userAgent")->visible = 0;
    getTextByDescriptor(realState, "documentText")->visible = 1;
    getTextAreaByDescriptor(realState, "urlField")->visible = 0;
    getButtonByDescriptor(state, "smallgobutton")->visible = 0;
    getTextByDescriptor(realState, "helpText")->visible = 0;

    realState->currentPageUrl = getTextAreaByDescriptor(realState, "urltextarea")->currentText;

    struct http_url *curURL = http_url_from_string(realState->currentPageUrl);
    if (curURL == NULL) {
        char *total = (char *) calloc(64 + strlen(realState->currentPageUrl) + 8, sizeof(char));
        strcpy(total, "Page has no title\n");
        strcat(total, realState->currentPageUrl);
        strcat(total, "\n\\H\nPlease enter a valid URL.\n");
        free(getTextByDescriptor(realState, "documentText")->text);
        getTextByDescriptor(realState, "documentText")->text = makeStrCpy(total);
        free(total);
        setTextOf(getTextAreaByDescriptor(realState, "urlField"), realState->currentPageUrl);
    } else {
        setTextOf(getTextAreaByDescriptor(realState, "urlField"), http_urlToString(http_url_from_string(realState->currentPageUrl)));

        getTextByDescriptor(realState, "documentText")->text = (char *) calloc(8192, sizeof(char));
        strcpy(getTextByDescriptor(realState, "documentText")->text, "The document is downloading...");
        render_nc(realState);
        char *data = downloadAndOpenPage(realState, getTextAreaByDescriptor(realState, "urltextarea")->currentText, onReceiveData, onFinishData, 0);
        free(getTextByDescriptor(realState, "documentText")->text);
        getTextByDescriptor(realState, "documentText")->text = data;
        if (data == NULL) {
            getTextByDescriptor(realState, "documentText")->text = makeStrCpy("ERROR: Serialized document was NULL!");
        }
        render_nc(realState);

        struct http_url *finalURL = http_url_from_string(realState->currentPageUrl);
        log_err("Checking fragment... (cpu: %s)\n", realState->currentPageUrl);
        if (finalURL->had_explicit_fragment && finalURL->fragment[0]) {
            log_err("Existed! frag was %s\n", finalURL->fragment);
            int *scrollPoint = getScrollPoint(realState, finalURL->fragment);
            if (scrollPoint) {
                log_err("It existed!");
                realState->globalScrollY = -(*scrollPoint);
            }
        }
    }

    getTextAreaByDescriptor(realState, "urlField")->visible = 1;
    getButtonByDescriptor(state, "smallgobutton")->visible = 1;
}

void ongotourlfield(void *state, char *_) {
    struct nc_state *realState = (struct nc_state *) state;
    setTextOf(getTextAreaByDescriptor(realState, "urltextarea"), getTextAreaByDescriptor(realState, "urlField")->currentText);
    ongotourl(state, _);
}

void onlinkpressed(void *state, char *url) {
    if (strncmp(url, "_temp_linkto_", 13)) {
        return;
    }

    struct nc_state *realState = (struct nc_state *) state;
    freeButtons(realState);

    realState->selectableIndex = -1;
    realState->currentPage = PAGE_DOCUMENT_LOADED;
    realState->globalScrollX = 0;
    realState->globalScrollY = 0;
    getTextByDescriptor(realState, "gotoURL")->visible = 0;
    getTextAreaByDescriptor(realState, "urltextarea")->visible = 0;
    getButtonByDescriptor(realState, "gotobutton")->visible = 0;
    getTextByDescriptor(realState, "userAgentDetail")->visible = 0;
    getTextAreaByDescriptor(realState, "userAgent")->visible = 0;
    getTextByDescriptor(realState, "documentText")->visible = 1;
    getTextAreaByDescriptor(realState, "urlField")->visible = 0;
    getButtonByDescriptor(state, "smallgobutton")->visible = 0;
    getTextByDescriptor(realState, "helpText")->visible = 0;

    char *realURL = url + 15;

    struct http_url *curURL = http_url_from_string(realState->currentPageUrl);
    if (curURL == NULL) {
        char *total = (char *) calloc(64 + strlen(realState->currentPageUrl) + 8, sizeof(char));
        strcpy(total, "Page has no title\n");
        strcat(total, realState->currentPageUrl);
        strcat(total, "\n\\H\nInvalid URL.");
        free(getTextByDescriptor(realState, "documentText")->text);
        getTextByDescriptor(realState, "documentText")->text = makeStrCpy(total);
        free(total);
        setTextOf(getTextAreaByDescriptor(realState, "urlField"), realState->currentPageUrl);
    } else {
        char *absoluteURL = http_resolveRelativeURL(curURL, realState->currentPageUrl, realURL);

        realState->currentPageUrl = absoluteURL;
        setTextOf(getTextAreaByDescriptor(realState, "urlField"), absoluteURL);

        getTextByDescriptor(realState, "documentText")->text = (char *) calloc(8192, sizeof(char));
        strcpy(getTextByDescriptor(realState, "documentText")->text, "The document is downloading...");
        render_nc(realState);
        char *data = downloadAndOpenPage(realState, absoluteURL, onReceiveData, onFinishData, 0);
        free(getTextByDescriptor(realState, "documentText")->text);
        getTextByDescriptor(realState, "documentText")->text = data;
        if (data == NULL) {
            getTextByDescriptor(realState, "documentText")->text = makeStrCpy("ERROR: Serialized document was NULL!");
        }
        render_nc(realState);

        struct http_url *finalURL = http_url_from_string(realState->currentPageUrl);
        if (finalURL->had_explicit_fragment && finalURL->fragment[0]) {
            int *scrollPoint = getScrollPoint(realState, finalURL->fragment);
            if (scrollPoint) {
                realState->globalScrollY = -(*scrollPoint);
            }
        }
    }

    getTextAreaByDescriptor(realState, "urlField")->visible = 1;
    getButtonByDescriptor(state, "smallgobutton")->visible = 1;
}

#endif
