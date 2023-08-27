#include "../utils/string.h"

#ifndef _CSS_SELECTORS
    #define _CSS_SELECTORS 1

// Note: separators/combinators are not yet supported.
enum css_combinator {
    COMBINATOR_CHILD,
    COMBINATOR_UNKNOWN,
    COMBINATOR_NONE // The first individual selector has COMBINATOR_NONE since there's nothing before it
};

struct css_select {
    char *elementName;
    char **classes;
    char **ids;
};

struct css_sselector {
    struct css_select individual;
    enum css_combinator combinator; // The combinator right before the current selector
    int count;
};

struct css_selector_parsed {
    struct css_sselector *selectors; // Individual selectors separated by combinators
    int count;
};


struct css_selectors {
    char **selectors; // Selectors separated by commas
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
};

void CSS_addSelectorTo(struct css_selectors *selectors, char *toAdd) {
    selectors->count ++;
    selectors->selectors = (char **) realloc(selectors->selectors, selectors->count * sizeof(char *));
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

void CSS_freePersistentStyles(struct css_persistent_styles *originalStyles) {
    for (int i = 0; i < originalStyles->count; i ++) {
        struct css_selectors curSelectors = originalStyles->selectors[i];
        free(curSelectors.styleContent);
        for (int j = 0; j < curSelectors.count; j ++) {
            char *selector = curSelectors.selectors[j];
            free(selector);
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
    for (int i = 0; i < len; i ++) {
        char curChar = content[i];

        switch (currentState) {
            case CSS_PARSE_OUTSIDE_BRACKET_WHITESPACE:
                if (!isWhiteSpace(curChar)) {
                    currentState = CSS_PARSE_SELECTOR;
                    i --;
                }
                break;
            case CSS_PARSE_SELECTOR:
                if (curChar == '{') {
                    CSS_addSelectorTo(&currentSelector, trimString(currentDataContent));
                    clrStr(currentDataContent);
                    currentDataUsage = 0;
                    currentState = CSS_PARSE_INSIDE_BRACKET_CONTENT;
                } else if (curChar == ',') {
                    CSS_addSelectorTo(&currentSelector, trimString(currentDataContent));
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
                    currentSelector.styleContent[strlen(currentSelector.styleContent)] = 0; // just to make sure
                    CSS_addStyleTo(originalStyles, currentSelector);
                    currentState = CSS_PARSE_OUTSIDE_BRACKET_WHITESPACE;
                    currentSelector.selectors = NULL;
                    currentSelector.count = 0;
                    currentSelector.styleContent = (char *) calloc(strlen(content) + 1, sizeof(char));
                } else {
                    currentSelector.styleContent[strlen(currentSelector.styleContent)] = curChar;
                }
                break;
        }
    }
}

#endif
