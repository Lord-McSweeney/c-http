#include "../utils/string.h"
#include "parse-selectors.h"
#include "../xml/attributes.h"
#include "../xml/html.h"

int isSelectorSimple(char *selector) {
    return !stringContains(selector, '.') &&
           !stringContains(selector, '>') &&
           !stringContains(selector, '#') &&
           !stringContains(selector, ' ') &&
           !stringContains(selector, '|') &&
           !stringContains(selector, '~') &&
           !stringContains(selector, '+') &&
           !stringContains(selector, ':') &&
           !stringContains(selector, '(') &&
           !stringContains(selector, ')') &&
           !stringContains(selector, '"') &&
           !stringContains(selector, '[') &&
           !stringContains(selector, ']') &&
           !stringContains(selector, '=');
}

int doesSelectorMatchNode(char *selector, struct xml_node *node, struct xml_attributes *attribs) {
    if (!strcmp(selector, "*")) {
        return 1;
    }

    if (isSelectorSimple(selector)) {   
        // Just a tag name, phew
        return !strcmp(node->name, selector);
    }
    if (strlen(selector) > 1 && selector[0] == '#' && isSelectorSimple(selector + 1)) {   
        // id
        char *id = XML_getAttributeByName(attribs, "id");
        if (id) {
            return !strcmp(id, selector + 1);
        } else {
            return 0;
        }
    }

    // Unimplemented selector
    return 0;
}

char *CSS_getCSSTextForNode(struct xml_node *node, struct xml_attributes *attribs, struct css_persistent_styles *styles) {
    // CSS has an importance level (e.g. !important). So far, this is unimplemented- later styles will override newer ones.
    for (int i = styles->count - 1; i >= 0; i --) {
        struct css_selectors curSelectors = styles->selectors[i];
        for (int j = 0; j < curSelectors.count; j ++) {
            char *selector = curSelectors.selectors[j];
            if (doesSelectorMatchNode(selector, node, attribs)) {
                return curSelectors.styleContent;
            }
        }
    }
    return makeStrCpy("");
}
