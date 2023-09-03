#include "../utils/log.h"
#include "../utils/string.h"

#ifndef _CSS_SELECTORS
    #define _CSS_SELECTORS 1


struct css_selector_info {
    char *tagName; // NULL to match all tags

    char **classes; // NULL to match all classes
    int numClasses;

    char *id; // NULL to match all ids

    char any; // asterisk
    char matches_nothing; // two ids in a single selector results in the selector matching nothing
};

enum _css_internal_selector_parser_state {
    PARSE_SELECTOR_TAG_NAME,
    PARSE_SELECTOR_CLASS_NAME,
    PARSE_SELECTOR_ID,
    PARSE_SELECTOR_UNKNOWN
};

// Note: This function expects a selector trimmed of all surrounding whitespace!
struct css_selector_info CSS_parseSelector(char *string) {
    struct css_selector_info result;
    result.tagName = NULL;
    result.classes = NULL;
    result.numClasses = 0;
    result.id = NULL;
    result.matches_nothing = 0;
    result.any = 0;

    if (!strcmp(string, "*")) {
        result.any = 1;
        return result;
    }

    int alreadyMatchedId = 0;
    int currentState = PARSE_SELECTOR_UNKNOWN;
    int len = strlen(string);
    int currentIndex = 1;
    char *currentContent = (char *) calloc(512, sizeof(char));
    int currentContentUsage = 0;
    for (int i = 0; i < len; i ++) {
        char curChar = string[i];
        if (curChar == '|' ||
            curChar == ':' ||
            curChar == '+' ||
            curChar == '~' ||
            curChar == '>' ||
            curChar == '^' ||
            curChar == '$' ||
            curChar == '[' ||
            curChar == ']' ||
            curChar == '*' ||
            curChar == '=') {

            // unsupported style
            result.tagName = NULL;
            result.id = NULL;
            result.numClasses = 0;
            result.matches_nothing = 1;
            return result;
        }

        switch (currentState) {
            case PARSE_SELECTOR_TAG_NAME:
                if (curChar == '#') {
                    result.tagName = makeStrCpy(currentContent);
                    clrStr(currentContent);
                    currentContentUsage = 0;

                    currentState = PARSE_SELECTOR_ID;
                    currentIndex = 0;
                } else if (curChar == '.') {
                    result.tagName = makeStrCpy(currentContent);
                    clrStr(currentContent);
                    currentContentUsage = 0;

                    currentState = PARSE_SELECTOR_CLASS_NAME;
                    currentIndex = 0;
                } else {
                    if (currentContentUsage > 510) {
                        log_warn("Content usage exceeded maximum content usage while recording a class!");
                    } else {
                        currentContent[currentContentUsage] = curChar;
                        currentContentUsage ++;
                    }
                }
                break;
            case PARSE_SELECTOR_CLASS_NAME:
                if (currentIndex == 1) {
                    result.numClasses ++;
                    result.classes = (char **) realloc(result.classes, result.numClasses * sizeof(char *));
                }

                if (curChar == '#') {
                    result.classes[result.numClasses - 1] = makeStrCpy(currentContent);
                    clrStr(currentContent);
                    currentContentUsage = 0;

                    currentState = PARSE_SELECTOR_ID;
                    currentIndex = 0;
                } else if (curChar == '.') {
                    result.classes[result.numClasses - 1] = makeStrCpy(currentContent);
                    clrStr(currentContent);
                    currentContentUsage = 0;

                    currentState = PARSE_SELECTOR_CLASS_NAME;
                    currentIndex = 0;
                } else {
                    if (currentContentUsage > 510) {
                        log_warn("Content usage exceeded maximum content usage while recording a class!");
                    } else {
                        currentContent[currentContentUsage] = curChar;
                        currentContentUsage ++;
                    }
                }
                break;
            case PARSE_SELECTOR_ID:
                if (currentIndex == 1) {
                    if (alreadyMatchedId) {
                        result.matches_nothing = 1;
                        free(result.id);
                    }
                    alreadyMatchedId = 1;
                }

                if (curChar == '#') {
                    result.id = makeStrCpy(currentContent);
                    clrStr(currentContent);
                    currentContentUsage = 0;

                    currentState = PARSE_SELECTOR_ID;
                    currentIndex = 0;
                } else if (curChar == '.') {
                    result.id = makeStrCpy(currentContent);
                    clrStr(currentContent);
                    currentContentUsage = 0;

                    currentState = PARSE_SELECTOR_CLASS_NAME;
                    currentIndex = 0;
                } else {
                    if (currentContentUsage > 510) {
                        log_warn("Content usage exceeded maximum content usage while recording ID!");
                    } else {
                        currentContent[currentContentUsage] = curChar;
                        currentContentUsage ++;
                    }
                }
                break;
            case PARSE_SELECTOR_UNKNOWN:
                if (curChar == '#') {
                    currentState = PARSE_SELECTOR_ID;
                } else if (curChar == '.') {
                    currentState = PARSE_SELECTOR_CLASS_NAME;
                } else {
                    currentState = PARSE_SELECTOR_TAG_NAME;
                    i --;
                }
                currentIndex = 0;
                break;
        }
        currentIndex ++;
    }
    switch (currentState) {
        case PARSE_SELECTOR_ID:
            result.id = makeStrCpy(currentContent);
            clrStr(currentContent);
            break;
        case PARSE_SELECTOR_CLASS_NAME:
            result.classes[result.numClasses - 1] = makeStrCpy(currentContent);
            clrStr(currentContent);
            break;
        case PARSE_SELECTOR_TAG_NAME:
            result.tagName = makeStrCpy(currentContent);
            clrStr(currentContent);
            break;
    }

    free(currentContent);
    return result;
}



struct css_selectors {
    struct css_selector_info *selectors; // Selectors separated by commas
    char *styleContent;
    int count;
};

struct css_persistent_styles {
    struct css_selectors *selectors;
    int count;
};

enum _css_internal_style_tag_parser_state {
    CSS_PARSE_SELECTOR,
    CSS_PARSE_INSIDE_BRACKET_CONTENT,
    CSS_PARSE_OUTSIDE_BRACKET_WHITESPACE,
    CSS_PARSE_COMMENT_OUTSIDE_BRACKET,
};

void CSS_addSelectorTo(struct css_selectors *selectors, struct css_selector_info toAdd) {
    selectors->count ++;
    selectors->selectors = (struct css_selector_info *) realloc(selectors->selectors, selectors->count * sizeof(struct css_selector_info));
    selectors->selectors[selectors->count - 1] = toAdd;
}

void CSS_addStyleTo(struct css_persistent_styles *styles, struct css_selectors toAdd) {
    styles->count ++;
    styles->selectors = (struct css_selectors *) realloc(styles->selectors, styles->count * sizeof(struct css_selectors));
    styles->selectors[styles->count - 1] = toAdd;
}

struct css_persistent_styles CSS_makePersistentStyles() {
    struct css_persistent_styles styles;
    styles.selectors = NULL;
    styles.count = 0;

    return styles;
}

void CSS_freeSelector(struct css_selector_info selector) {
    if (selector.tagName) free(selector.tagName);
    if (selector.id) free(selector.id);
    for (int k = 0; k < selector.numClasses; k ++) {
        free(selector.classes[k]);
    }
    if (selector.classes) free(selector.classes);
}

void CSS_freePersistentStyles(struct css_persistent_styles *originalStyles) {
    for (int i = 0; i < originalStyles->count; i ++) {
        struct css_selectors curSelectors = originalStyles->selectors[i];
        free(curSelectors.styleContent);
        for (int j = 0; j < curSelectors.count; j ++) {
            struct css_selector_info selector = curSelectors.selectors[j];
            CSS_freeSelector(selector);
        }
        free(curSelectors.selectors);
    }
    free(originalStyles->selectors);
}

void CSS_applyStyleData(struct css_persistent_styles *originalStyles, char *content) {
    int currentState = CSS_PARSE_OUTSIDE_BRACKET_WHITESPACE;

    struct css_selectors currentSelector;
    currentSelector.selectors = NULL;
    currentSelector.count = 0;
    currentSelector.styleContent = (char *) calloc(strlen(content) + 1, sizeof(char));

    int maxDataUsage = 256;
    char *currentDataContent = (char *) calloc(maxDataUsage, sizeof(char));
    int currentDataUsage = 0;

    int len = strlen(content);
    int bracketNum = 0;
    for (int i = 0; i < len; i ++) {
        char curChar = content[i];

        switch (currentState) {
            case CSS_PARSE_OUTSIDE_BRACKET_WHITESPACE:
                if (curChar == '/' && content[i + 1] == '*') {
                    currentState = CSS_PARSE_COMMENT_OUTSIDE_BRACKET;
                    i ++;
                } else if (!isWhiteSpace(curChar)) {
                    currentState = CSS_PARSE_SELECTOR;
                    i --;
                }
                break;
            case CSS_PARSE_SELECTOR:
                if (curChar == '{') {
                    CSS_addSelectorTo(&currentSelector, CSS_parseSelector(trimString(currentDataContent)));
                    clrStr(currentDataContent);
                    currentDataUsage = 0;
                    currentState = CSS_PARSE_INSIDE_BRACKET_CONTENT;
                    bracketNum ++;
                } else if (curChar == ',') {
                    CSS_addSelectorTo(&currentSelector, CSS_parseSelector(trimString(currentDataContent)));
                    clrStr(currentDataContent);
                    currentDataUsage = 0;
                } else {
                    currentDataUsage ++;
                    if (currentDataUsage > maxDataUsage - 2) {
                        currentDataContent = (char *) realloc(currentDataContent, maxDataUsage * 2);
                        if (currentDataContent == NULL) {
                            return;
                        }
                        memset(currentDataContent + maxDataUsage, 0, maxDataUsage);
                        maxDataUsage *= 2;
                    }
                    currentDataContent[currentDataUsage - 1] = curChar;
                }
                break;
            case CSS_PARSE_INSIDE_BRACKET_CONTENT:
                // TODO: quotes
                if (curChar == '}') {
                    bracketNum --;
                    if (!bracketNum) {
                        currentSelector.styleContent[strlen(currentSelector.styleContent)] = 0; // just to make sure
                        CSS_addStyleTo(originalStyles, currentSelector);
                        currentState = CSS_PARSE_OUTSIDE_BRACKET_WHITESPACE;
                        currentSelector.selectors = NULL;
                        currentSelector.count = 0;
                        currentSelector.styleContent = (char *) calloc(strlen(content) + 1, sizeof(char));
                    }
                } else if (curChar == '{') {
                    bracketNum ++;
                } else {
                    currentSelector.styleContent[strlen(currentSelector.styleContent)] = curChar;
                }
                break;
            case CSS_PARSE_COMMENT_OUTSIDE_BRACKET:
                if (curChar == '*' && content[i + 1] == '/') {
                    currentState = CSS_PARSE_OUTSIDE_BRACKET_WHITESPACE;
                    i ++;
                }
                break;
        }
    }
}

#endif
