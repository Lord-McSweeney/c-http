#include "../utils/string.h"
#include "parse-selectors.h"
#include "parse.h"
#include "../xml/attributes.h"
#include "../xml/html.h"

int CSS_selectorMatchesElement(struct css_selector_info selector, struct xml_node *node, struct xml_attributes *attribs) {
    if (selector.any) return 1;
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
                for (int i = 0; i < classes.count; i ++) {
                    free(classes.result[i]);
                }
                free(classes.result);
                return 0;
            }
        }

        for (int i = 0; i < classes.count; i ++) {
            free(classes.result[i]);
        }
        free(classes.result);
    }
    return 1;
}

int CSS_doesSelectorMatchList(struct css_selector_info selector, struct xml_list list) {
    int elemCount = list.count;
    for (int i = 0; i < elemCount; i ++) {
        struct xml_attrib_result res = XML_parseAttributes(list.nodes[i].attribute_content);
        if (res.error) continue;

        if (CSS_selectorMatchesElement(selector, &list.nodes[i], res.attribs)) {
            return 1;
        }
    }
    return 0;
}

void CSS_getCSSTextForNode(struct css_styles *styles, struct xml_node *node, struct xml_attributes *attribs, struct css_persistent_styles *persistentStyles) {
    // CSS has a specificity level. So far, this is unimplemented- later styles will always override newer ones (but the current implementation does handle !important).
    for (int i = 0; i < persistentStyles->count; i ++) {
        struct css_selectors curSelectors = persistentStyles->selectors[i];
        for (int j = 0; j < curSelectors.count; j ++) {
            struct css_selector_info selector = curSelectors.selectors[j];
            if (CSS_selectorMatchesElement(selector, node, attribs)) {
                CSS_parseInlineStyles(styles, curSelectors.styleContent);
            }
        }
    }
}
