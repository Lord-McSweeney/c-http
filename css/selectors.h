#include "../utils/string.h"
#include "parse-selectors.h"
#include "parse.h"
#include "../xml/attributes.h"
#include "../xml/html.h"

struct css_selector_info {
    char *tagName; // NULL to match all tags

    char **classes; // NULL to match all classes
    int numClasses;

    char *id; // NULL to match all ids

    int matches_nothing; // two ids in a single selector results in the selector matching nothing
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

    int alreadyMatchedId = 0;
    int currentState = PARSE_SELECTOR_UNKNOWN;
    int len = strlen(string);
    int currentIndex = 1;
    char *currentContent = (char *) calloc(512, sizeof(char));
    int currentContentUsage = 0;
    for (int i = 0; i < len; i ++) {
        char curChar = string[i];
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
                        fprintf(stderr, "Warning: Content usage exceeded maximum content usage while recording a class!");
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
                        fprintf(stderr, "Warning: Content usage exceeded maximum content usage while recording a class!");
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
                        fprintf(stderr, "Warning: Content usage exceeded maximum content usage while recording ID!");
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

int CSS_selectorMatchesElement(struct css_selector_info selector, struct xml_node *node, struct xml_attributes *attribs) {
    if (selector.matches_nothing) return 0;

    if (selector.tagName) {
        if (strcmp(selector.tagName, node->name)) {
            return 0;
        }
    }

    if (selector.id) {
        char *id = XML_getAttributeByName(attribs, "id");
        if (!id) return 0;

        if (strcmp(selector.id, id)) {
            return 0;
        }
    }

    if (selector.numClasses) {
        char *classList = XML_getAttributeByName(attribs, "class");
        if (!classList) return 0;

        struct split_string classes = splitStringWithWhitespaceDelimiter(classList);
        
        for (int k = 0; k < selector.numClasses; k ++) {
            char *selectorClass = selector.classes[k];
            int matched = 0;
            for (int i = 0; i < classes.count; i ++) {
                if (!strcmp(classes.result[i], selectorClass)) {
                    matched = 1;
                }
            }

            if (!matched) {
                return 0;
            }
        }
    }

    return 1;
}

int isSelectorSimple(char *selector) {
    // technically this also includes everything from isSelectorUnsupported, but that stuff was already checked
    return !stringContainsAny(selector, ".#()\"");
}

int isSelectorUnsupported(char *selector) {
    return stringContainsAny(selector, " |:+~>^$[]*=");
}

int doesSelectorMatchNode(char *selector, struct xml_node *node, struct xml_attributes *attribs) {
    if (!strcmp(selector, "*")) {
        return 1;
    }

    if (isSelectorUnsupported(selector)) {
        // Unsupported: [attribute[op]value] matching, :[something] selectors, and combinators/separators
        return 0;
    }

    if (isSelectorSimple(selector)) {   
        // Just a tag name, phew
        return !strcmp(node->name, selector);
    }

    return CSS_selectorMatchesElement(CSS_parseSelector(selector), node, attribs);
}

void CSS_getCSSTextForNode(struct css_styles *styles, struct xml_node *node, struct xml_attributes *attribs, struct css_persistent_styles *persistentStyles) {
    // CSS has a specificity level. So far, this is unimplemented- later styles will always override newer ones (but the current implementation does handle !important).
    for (int i = 0; i < persistentStyles->count; i ++) {
        struct css_selectors curSelectors = persistentStyles->selectors[i];
        for (int j = 0; j < curSelectors.count; j ++) {
            char *selector = curSelectors.selectors[j];
            if (doesSelectorMatchNode(selector, node, attribs)) {
                CSS_parseInlineStyles(styles, curSelectors.styleContent);
            }
        }
    }
}
