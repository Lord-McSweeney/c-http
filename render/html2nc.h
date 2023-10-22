// Converts XML/HTML nodes to rich text.

#include "../css/block-inline.h"
#include "../css/parse-selectors.h"
#include "../css/styles.h"
#include "../http/url.h"
#include "../navigation/download.h"
#include "../utils/log.h"
#include "../utils/string.h"
#include "../xml/html.h"
#include "../xml/attributes.h"

// Ptr passed, along with URL of stylesheet
typedef void (*onStyleSheetDownloadStart)(void *, char *);

// Ptr passed
typedef void (*onStyleSheetDownloadProgress)(void *);

// Ptr passed
typedef void (*onStyleSheetDownloadComplete)(void *);

// Ptr passed, along with HTTP error code: returns default response in case of error
typedef char *(*onStyleSheetDownloadError)(void *, int);

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
    char *lower = toLowerCase(nodeName);
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

char *getXMLTrimmedTextContent(const char *content, int removeStart) {
    int len = strlen(content);
    char *allocated = (char *) calloc(len + 1, sizeof(char));
    char *textContent = makeStrCpy(content);

    int alreadyEncounteredSpace = 0;
    int alreadyEncounteredText = 0;
    if (removeStart) {
        alreadyEncounteredText = 1;
    }

    int curAllocIdx = 0;
    for (int i = 0; i < len; i ++) {
        if (textContent[i] == '\n') textContent[i] = ' ';
        if (textContent[i] == '\r') textContent[i] = ' ';
        if (textContent[i] == '\t') textContent[i] = ' ';
        if ((alreadyEncounteredSpace == 0 && alreadyEncounteredText) || textContent[i] != ' ') {
            if (textContent[i] != ' ') {
                alreadyEncounteredSpace = 0;
                alreadyEncounteredText = 1;
            }
            allocated[curAllocIdx] = textContent[i];
            curAllocIdx ++;
        }
        if (textContent[i] == ' ') {
            alreadyEncounteredSpace = 1;
        }
    }
    char *newText = makeStrCpy(allocated);
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

// This function duplicates getTextAreaRendered in display-handling: it will be unnecessary once input is actually implemented.
char *getInputRendered(char *text, int width) {
    char *resultingText = (char *) calloc(width + 2, sizeof(char));

    int under = width - strlen(text);
    if (under < 0) under = 0;
    for (int i = 0; i < width; i ++) {
        if (text[i] == 0) {
            break;
        }
        resultingText[i] = text[i];
    }

    for (int i = 0; i < under; i ++) {
        resultingText[strlen(resultingText)] = '_';
    }

    return resultingText;
}

char *recursiveXMLToText(
    struct xml_list xml,
    struct html2nc_state *state,
    int originalHTMLLen,
    int uppercase,
    int listNestAmount,
    int *jhiibwt,
    int preformatted,
    struct css_persistent_styles *persistentStyles,
    char *baseURL,
    void *ptr,
    onStyleSheetDownloadStart onStart,
    onStyleSheetDownloadProgress onProgress,
    onStyleSheetDownloadComplete onComplete,
    onStyleSheetDownloadError onError
) {
    char *alloc = (char *) calloc(originalHTMLLen + 2, sizeof(char));

    int currentOrderedListNum = 1;

    int alreadySetOrderedListNum = 0;
    int justHadInlineInsideBlockWithText = *jhiibwt;
    int hasBlocked = 0;

    for (int i = 0; i < xml.count; i ++) {
        struct xml_node node = xml.nodes[i];
        if (node.type == NODE_DOCTYPE || node.type == NODE_COMMENT) {
            // ignore
        } else if (node.type == NODE_TEXT) {
            char *text;
            if (preformatted) {
                text = makeStrCpy(node.text_content);
            } else {
                text = getXMLTrimmedTextContent(node.text_content, justHadInlineInsideBlockWithText);
            }
            char *allocated_no_double = XML_parseHTMLEscapes(text);
            char *allocated = doubleStringBackslashes(allocated_no_double);
            free(allocated_no_double);

            if (uppercase) {
                char *upper = toUpperCase(allocated);
                strcat(alloc, upper);
                free(upper);
            } else {
                strcat(alloc, allocated);
            }
            free(text);
            free(allocated);
            hasBlocked = 0;
            justHadInlineInsideBlockWithText = 1;
            *jhiibwt = 1;
            continue; // necesssary because justHadInlineInsideBlockWithText gets reset at the end of the loop
        } else if (node.type == NODE_ELEMENT) {
            struct xml_attrib_result attrib_result = XML_parseAttributes(node.attribute_content);
            struct xml_attributes *attributes;
            if (attrib_result.error) {
                attributes = XML_makeEmptyAttributes();
            } else {
                attributes = attrib_result.attribs;
            }

            struct xml_attributes *parent_attributes;
            if (node.parent == NULL) {
                parent_attributes = XML_makeEmptyAttributes();
            } else {
                struct xml_attrib_result parent_attrib_result = XML_parseAttributes(node.parent->attribute_content);
                if (parent_attrib_result.error) {
                    parent_attributes = XML_makeEmptyAttributes();
                } else {
                    parent_attributes = parent_attrib_result.attribs;
                }
            }

            struct css_styling elementStyling = CSS_getDefaultStylesFromElement(node, attributes, persistentStyles);
            struct css_styling parentStyling;
            if (node.parent) {
                parentStyling = CSS_getDefaultStylesFromElement(*node.parent, parent_attributes, persistentStyles);
            } else {
                parentStyling = CSS_getDefaultStyles();
            }

            char *lower = toLowerCase(node.name);
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
                !CSS_isStyleHidden(elementStyling) // Hidden elements shouldn't count
                 &&
                !(
                    i == 0
                     && 
                    node.parent
                     && 
                    CSS_isStyleBlock(parentStyling) // If we already had a block-level element as our parent and this is the first child, it doesn't count
                )
               ) {
                strcat(alloc, "\n");
            }

            // Only the first <title> is taken into account- the rest aren't special
            if (!strcmp(lower, "title") && state->title == NULL) {
                state->title = recursiveXMLToText(
                    node.children,
                    state,
                    originalHTMLLen,
                    0,
                    0,
                    (int *) calloc(1, sizeof(int)),
                    0,
                    persistentStyles,
                    baseURL,
                    ptr,
                    onStart,
                    onProgress,
                    onComplete,
                    onError
                );
                free(lower);
                freeXMLAttributes(attributes);
                freeXMLAttributes(parent_attributes);
                hasBlocked = 0;
                continue;
            }

            if (!strcmp(lower, "style")) {
                char *lowerParent = !node.parent ? makeStrCpy("no parent") : toLowerCase(node.parent->name);
                if (strcmp(lowerParent, "head") && strcmp(lowerParent, "noscript")) {
                    log_warn("<style> tag in invalid place (%s)\n", lowerParent);
                }
                char *type = XML_getAttributeByName(attributes, "type");
                if (!type || !strcmp(type, "text/css")) {
                    if (node.children.count) {
                        if (node.children.nodes[0].text_content) {
                            CSS_applyStyleData(persistentStyles, node.children.nodes[0].text_content);
                        } else {
                            log_warn("<style> tag had child with no text content\n");
                        }
                    } else {
                        log_warn("<style> tag had no child nodes\n");
                    }
                } else {
                    log_warn("<style> tag with unknown type (%s)\n", type);
                }
                free(lowerParent);
            }

            if (!strcmp(lower, "link")) {
                char *rel = XML_getAttributeByName(attributes, "rel");
                if (rel) {
                    char *lowerRel = toLowerCase(rel);
                    if (!strcmp(lowerRel, "stylesheet")) {
                        char *type = XML_getAttributeByName(attributes, "type");
                        if (!type || (!strcmp(type, "text/css") || !strcmp(type, ""))) {
                            char *href = XML_getAttributeByName(attributes, "href");
                            if (href) {
                                onStart(ptr, href);
                                struct http_url *url = http_url_from_string(baseURL);

                                char *absoluteURL = http_resolveRelativeURL(url, baseURL, href);

                                struct http_response initialResponse = downloadPage(
                                    ptr,
                                    "uqers",
                                    &absoluteURL,
                                    onProgress,
                                    NULL,
                                    0,
                                    onError,
                                    defaultonredirecthandler,
                                    defaultonredirectsuccesshandler,
                                    defaultonredirecterrorhandler
                                );
                                // Should this check the content type of the response?

                                char *styling = initialResponse.response_body.data;
                                CSS_applyStyleData(persistentStyles, styling);

                                onComplete(ptr);
                            } else {
                                log_warn("<link rel=\"stylesheet\"> with no href attribute\n");
                            }
                        } else {
                            log_warn("<link rel=\"stylesheet\"> with invalid type attribute (%s)\n", type);
                        }
                    } else {
                        log_warn("<link> with unknown rel attribute (%s)\n", rel);
                    }
                    free(lowerRel);
                } else {
                    log_warn("<link> without rel attribute\n");
                }
            }

            char *id = XML_getAttributeByName(attributes, "id");
            if (id) {
                char *encoded = safeEncodeString(id);

                strcat(alloc, "\\nP");
                strcat(alloc, encoded);

                free(encoded);
            }

            if (!strcmp(lower, "br") && !CSS_isStyleHidden(elementStyling)) {
                strcat(alloc, "\n");
            } else if (!strcmp(lower, "hr") && !CSS_isStyleHidden(elementStyling)) {
                strcat(alloc, "\\h");
            } else {
                int isVisible = !CSS_isStyleHidden(elementStyling);

                if (elementStyling.pointer_events == POINTER_EVENTS_ALL) {
                    strcat(alloc, "\\e");
                } else if (elementStyling.pointer_events == POINTER_EVENTS_NONE) {
                    strcat(alloc, "\\d");
                }
                if (isVisible && !strcmp(lower, "a") && XML_getAttributeByName(attributes, "href")) {
                    strcat(alloc, "\\m");
                }
                if (isVisible && !strcmp(lower, "button")) {
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
                    case CSS_COLOR_WHEAT:
                        strcat(alloc, "\\9");
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

                    if (node.parent == NULL) {
                        strcat(alloc, "\n - ");
                    } else {
                        char *parentName = toLowerCase(node.parent->name);
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

                int had = justHadInlineInsideBlockWithText;

                char *text = recursiveXMLToText(
                    node.children,
                    state,
                    originalHTMLLen,
                    (hLevel >= 2) || uppercase,
                    listNestAmount,
                    jhiibwt,
                    !strcmp(lower, "pre") || preformatted,
                    persistentStyles,
                    baseURL,
                    ptr,
                    onStart,
                    onProgress,
                    onComplete,
                    onError
                );

                justHadInlineInsideBlockWithText = had || *jhiibwt;
                *jhiibwt = justHadInlineInsideBlockWithText;

                int wasDisplayed = text[0] != 0;

                if (!strcmp(lower, "img") && isVisible) {
                    char *altText = XML_getAttributeByName(attributes, "alt");
                    if (altText && strlen(altText)) {
                        strcat(alloc, "[");
                        strcat(alloc, altText);
                        strcat(alloc, "]");

                        // DO NOT free(altText) here! getAttributeByName returns a reference to the attribute value,
                        // not a copy of the attribute value! freeXMLAttributes will free it anyway!
                    } else {
                        strcat(alloc, "[IMAGE]");
                    }
                    free(text);
                    free(lower);
                    freeXMLAttributes(attributes);
                    freeXMLAttributes(parent_attributes);
                    justHadInlineInsideBlockWithText = 1;
                    *jhiibwt = 1;
                    hasBlocked = 0;
                    continue;
                }

                if (!strcmp(lower, "select") && isVisible) {
                    strcat(alloc, "\\m");

                    strcat(alloc, "[SELECT]");

                    char *encoded = safeEncodeString("Select an option");

                    strcat(alloc, "\\nN");
                    strcat(alloc, encoded);
                    free(encoded);

                    free(text);
                    free(lower);
                    freeXMLAttributes(attributes);
                    freeXMLAttributes(parent_attributes);
                    justHadInlineInsideBlockWithText = 1;
                    *jhiibwt = 1;
                    hasBlocked = 0;
                    continue;
                }

                if (!strcmp(lower, "input") && isVisible) {
                    char *type = XML_getAttributeByName(attributes, "type");
                    if (type) {
                            if (!strcmp(type, "checkbox")) {
                            strcat(alloc, "\\m");

                            char *checked = XML_getAttributeByName(attributes, "checked");
                            if (!checked) {
                                strcat(alloc, "[\\q \\r]");
                            } else {
                                strcat(alloc, "[\\qX\\r]");
                            }

                            char *encoded = safeEncodeString("Checkbox");

                            strcat(alloc, "\\nN");
                            strcat(alloc, encoded);
                            free(encoded);
                        } else if (!strcmp(type, "text")) {
                            char *isReadOnly = XML_getAttributeByName(attributes, "readonly");
                            strcat(alloc, "\\m");

                            strcat(alloc, "[_________]");

                            char *encoded = NULL;
                            if (isReadOnly) {
                                encoded = safeEncodeString("Text input (read-only)");
                            } else {
                                encoded = safeEncodeString("Text input");
                            }

                            strcat(alloc, "\\nN");
                            strcat(alloc, encoded);
                            free(encoded);
                        } else if (!strcmp(type, "hidden")) {
                            // nothing
                        } else if (!strcmp(type, "submit") || !strcmp(type, "button")) {
                            strcat(alloc, "\\m");

                            char *value = XML_getAttributeByName(attributes, "value");
                            if (value && *value) {
                                strcat(alloc, "[");
                                strcat(alloc, value);
                                strcat(alloc, "]");
                            }

                            char *encoded = safeEncodeString("Submit form");

                            strcat(alloc, "\\nN");
                            strcat(alloc, encoded);
                            free(encoded);
                        } else if (!strcmp(type, "file")) {
                            strcat(alloc, "\\m");

                            strcat(alloc, "[UPLOAD]");

                            char *encoded = safeEncodeString("File upload");

                            strcat(alloc, "\\nN");
                            strcat(alloc, encoded);
                            free(encoded);
                        } else if (!strcmp(type, "number")) {
                            strcat(alloc, "\\m");

                            char *value = XML_getAttributeByName(attributes, "value");
                            if (!value) {
                                value = "";
                            }
                            strcat(alloc, "[");
                            strcat(alloc, getInputRendered(value, 9));
                            strcat(alloc, "]");

                            char *encoded = safeEncodeString("Numerical input");

                            strcat(alloc, "\\nN");
                            strcat(alloc, encoded);
                            free(encoded);
                        } else {
                            strcat(alloc, "[INPUT]");
                        }
                    } else {
                        strcat(alloc, "\\m");
                        strcat(alloc, "[_________]");
                        char *encoded = safeEncodeString("Text input");

                        strcat(alloc, "\\nN");
                        strcat(alloc, encoded);
                        free(encoded);
                    }
                    free(text);
                    free(lower);
                    freeXMLAttributes(attributes);
                    freeXMLAttributes(parent_attributes);
                    justHadInlineInsideBlockWithText = 1;
                    *jhiibwt = 1;
                    hasBlocked = 0;

                    if (elementStyling.bold || hLevel >= 1) {
                        strcat(alloc, "\\c");
                    }
                    switch (elementStyling.color) {
                        case CSS_COLOR_RED:
                        case CSS_COLOR_GREEN:
                        case CSS_COLOR_BLUE:
                        case CSS_COLOR_WHEAT:
                            strcat(alloc, "\\0");
                            break;
                    }
                    if (elementStyling.underline) {
                        strcat(alloc, "\\r");
                    }
                    if (elementStyling.italic) {
                        strcat(alloc, "\\j");
                    }
                    if (elementStyling.tabbed) {
                        strcat(alloc, "\\u");
                    }

                    if (elementStyling.pointer_events == POINTER_EVENTS_ALL || elementStyling.pointer_events == POINTER_EVENTS_NONE) {
                        strcat(alloc, "\\f");
                    }

                    continue;
                }

                if (isVisible) {
                    strcat(alloc, text);
                }

                if (elementStyling.bold || hLevel >= 1) {
                    strcat(alloc, "\\c");
                }
                switch (elementStyling.color) {
                    case CSS_COLOR_RED:
                    case CSS_COLOR_GREEN:
                    case CSS_COLOR_BLUE:
                    case CSS_COLOR_WHEAT:
                        strcat(alloc, "\\0");
                        break;
                }
                if (elementStyling.underline) {
                    strcat(alloc, "\\r");
                }
                if (elementStyling.italic) {
                    strcat(alloc, "\\j");
                }
                if (elementStyling.tabbed) {
                    strcat(alloc, "\\u");
                }

                if (isVisible && !strcmp(lower, "a") && XML_getAttributeByName(attributes, "href")) {
                    char *encoded = safeEncodeString(XML_getAttributeByName(attributes, "href"));

                    strcat(alloc, "\\nL");
                    strcat(alloc, encoded);
                    free(encoded);
                }

                if (!strcmp(lower, "button") && isVisible) {
                    char *encoded = safeEncodeString("Button");

                    strcat(alloc, "\\nN");
                    strcat(alloc, encoded);
                    free(encoded);
                }

                if (elementStyling.pointer_events == POINTER_EVENTS_ALL || elementStyling.pointer_events == POINTER_EVENTS_NONE) {
                    strcat(alloc, "\\f");
                }

                if (!strcmp(lower, "li")) {
                    listNestAmount --;
                }

                // FIXME: Account for hidden elements
                if (CSS_isDefaultInline(node.name) && wasDisplayed) {
                    free(text);
                    free(lower);
                    freeXMLAttributes(attributes);
                    freeXMLAttributes(parent_attributes);
                    justHadInlineInsideBlockWithText = 1;
                    *jhiibwt = 1;
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
        *jhiibwt = 0;
    }

    char *copy = makeStrCpy(alloc);
    free(alloc);
    return copy;
}

struct html2nc_result htmlToText(
    struct xml_list xml,
    char *originalHTML,
    char *baseURL,
    void *ptr,
    onStyleSheetDownloadStart onStart,
    onStyleSheetDownloadProgress onProgress,
    onStyleSheetDownloadComplete onComplete,
    onStyleSheetDownloadError onError
) {
    struct css_persistent_styles persistentStyles = CSS_makePersistentStyles();

    struct html2nc_state state;
    state.title = NULL;

    struct html2nc_result result;
    result.text = recursiveXMLToText(
        xml,
        &state,
        strlen(originalHTML),
        0,
        0,
        (int *) calloc(1, sizeof(int)),
        0,
        &persistentStyles,
        baseURL,
        ptr,
        onStart,
        onProgress,
        onComplete,
        onError
    );

    // Default title, if title wasn't set
    if (state.title == NULL) {
        result.title = makeStrCpy("Page has no title");
    } else {
        result.title = makeStrCpy(state.title);
    }

    CSS_freePersistentStyles(&persistentStyles);

    free(state.title);

    return result;
}
