#include "../xml/html.h"
#include "../xml/attributes.h"

int CSS_isDefaultBlock(struct xml_node *node) {
    char *lower = xml_toLowerCase(node->name);
    int res = !strcmp(lower, "div") || !strcmp(lower, "p") || !strcmp(lower, "hr") || !strcmp(lower, "header") || !strcmp(lower, "aside") || !strcmp(lower, "h1") || !strcmp(lower, "h2") || !strcmp(lower, "h3") || !strcmp(lower, "h4") || !strcmp(lower, "h5") || !strcmp(lower, "h6") || !strcmp(lower, "blockquote") || !strcmp(lower, "dt") || !strcmp(lower, "tspan") || !strcmp(lower, "desc") || !strcmp(lower, "li") || !strcmp(lower, "dd") || !strcmp(lower, "tr");
    free(lower);
    return res;
}

int CSS_isDefaultInline(const char *nodeName) {
    char *lower = xml_toLowerCase(nodeName);
    int res = !strcmp(lower, "a") || !strcmp(lower, "span") || !strcmp(lower, "font") || !strcmp(lower, "b") || !strcmp(lower, "i") || !strcmp(lower, "img") || !strcmp(lower, "strong") || !strcmp(lower, "em") || !strcmp(lower, "sup") || !strcmp(lower, "td");
    free(lower);
    return res;
}

int CSS_isDefaultHidden(const char *nodeName) {
    char *lower = xml_toLowerCase(nodeName);
    int res = !strcmp(lower, "script") || !strcmp(lower, "style");
    free(lower);
    return res;
}

enum css_display {
    DISPLAY_INLINE,
    DISPLAY_BLOCK,
    DISPLAY_NONE,
    DISPLAY_FLEX, // not yet implemented
    DISPLAY_UNKNOWN, // default
};

enum css_color {
    CSS_COLOR_RED,
    CSS_COLOR_BLUE,
    CSS_COLOR_BLACK,
    CSS_COLOR_UNKNOWN, // default
};

struct css_style {
    int bold;
    int italic;
    int underline;
    int strikethrough;
    int tabbed;
    enum css_color color;
    enum css_display display;
};

int CSS_isStyleBlock(struct css_style style) {
    return style.display == DISPLAY_BLOCK;
}

int CSS_isStyleInline(struct css_style style) {
    return style.display == DISPLAY_INLINE;
}

int CSS_isStyleHidden(struct css_style style) {
    return style.display == DISPLAY_NONE;
}

struct css_style CSS_getDefaultStylesFromElement(struct xml_node node, struct xml_attributes *attribs) {
    struct css_style styling;
    styling.display = DISPLAY_UNKNOWN;
    styling.color = COLOR_BLACK;
    styling.bold = 0;
    styling.italic = 0;
    styling.underline = 0;
    styling.strikethrough = 0;
    styling.tabbed = 0;

    // Default user-agent styles
    if (CSS_isDefaultBlock(&node)) {
        styling.display = DISPLAY_BLOCK;
    } else if (CSS_isDefaultInline(node.name)) {
        styling.display = DISPLAY_INLINE;
    } else if (CSS_isDefaultHidden(node.name)) {
        styling.display = DISPLAY_NONE;
    }

    char *lower = xml_toLowerCase(node.name);

    if (!strcmp(lower, "a")) {
        styling.color = CSS_COLOR_BLUE;
        styling.underline = 1;
    }
    if (!strcmp(lower, "b") || !strcmp(lower, "strong")) {
        styling.bold = 1;
    }
    if (!strcmp(lower, "i") || !strcmp(lower, "em")) {
        styling.italic = 1;
    }
    if (!strcmp(lower, "s")) {
        styling.strikethrough = 1;
    }
    if (!strcmp(lower, "dd")) {
        styling.tabbed = 1;
    }


    // CSS styling comes next: TODO
    // ...
    char *attrib = XML_getAttributeByName(attribs, "style");
    fprintf(stderr, "style attribute: |%s|\n", attrib);
    if (attrib != NULL && (!strcmp(attrib, "display: none") || !strcmp(attrib, "display: none;") || !strcmp(attrib, "display: none:") || !strcmp(attrib, "display:none"))) {
        styling.display = DISPLAY_NONE;
    }

    free(lower);
    return styling;
}
