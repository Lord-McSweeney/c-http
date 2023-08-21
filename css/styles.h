#include "../xml/attributes.h"

#include "parse.h"

#ifndef _CSS_STYLES
    #define _CSS_STYLES 1

int CSS_isDefaultBlock(struct xml_node *node) {
    char *lower = xml_toLowerCase(node->name);
    int res = !strcmp(lower, "div") || !strcmp(lower, "p") || !strcmp(lower, "hr") || !strcmp(lower, "header") || !strcmp(lower, "aside") || !strcmp(lower, "h1") || !strcmp(lower, "h2") || !strcmp(lower, "h3") || !strcmp(lower, "h4") || !strcmp(lower, "h5") || !strcmp(lower, "h6") || !strcmp(lower, "blockquote") || !strcmp(lower, "dt") || !strcmp(lower, "tspan") || !strcmp(lower, "li") || !strcmp(lower, "dd") || !strcmp(lower, "tr") || !strcmp(lower, "button");
    free(lower);
    return res;
}

int CSS_isDefaultInline(const char *nodeName) {
    char *lower = xml_toLowerCase(nodeName);
    int res = !strcmp(lower, "a") || !strcmp(lower, "span") || !strcmp(lower, "font") || !strcmp(lower, "b") || !strcmp(lower, "i") || !strcmp(lower, "u") || !strcmp(lower, "img") || !strcmp(lower, "strong") || !strcmp(lower, "em") || !strcmp(lower, "sup") || !strcmp(lower, "td") || !strcmp(lower, "input") || !strcmp(lower, "kbd");
    free(lower);
    return res;
}

int CSS_isDefaultHidden(const char *nodeName) {
    char *lower = xml_toLowerCase(nodeName);
    int res = !strcmp(lower, "script") || !strcmp(lower, "style") || !strcmp(lower, "head") || !strcmp(lower, "meta") || !strcmp(lower, "link") || !strcmp(lower, "title") || !strcmp(lower, "svg");
    free(lower);
    return res;
}

enum css_display {
    DISPLAY_INLINE,
    DISPLAY_BLOCK,
    DISPLAY_NONE,
    DISPLAY_FLEX, // not yet implemented
    DISPLAY_DEFAULT,
    DISPLAY_UNKNOWN, // default
};

enum css_color {
    CSS_COLOR_RED,
    CSS_COLOR_BLUE,
    CSS_COLOR_GREEN,
    CSS_COLOR_BLACK, // default
    CSS_COLOR_UNKNOWN,
};

enum css_position {
    POSITION_ABSOLUTE,
    POSITION_RELATIVE,
    POSITION_STATIC, // default
    POSITION_FIXED,
    POSITION_STICKY,
    POSITION_UNKNOWN,
};

struct css_styling {
    int bold;
    int italic;
    int underline;
    int strikethrough;
    int tabbed;
    enum css_position position;
    enum css_color color;
    enum css_display display;
};

int CSS_isStyleBlock(struct css_styling style) {
    return style.display == DISPLAY_BLOCK;
}

int CSS_isStyleInline(struct css_styling style) {
    return style.display == DISPLAY_INLINE;
}

int CSS_isStyleHidden(struct css_styling style) {
    return style.display == DISPLAY_NONE;
}

enum css_display CSS_styleNameToDisplay(char *name) {
    if (name == NULL) return DISPLAY_DEFAULT;
    if (!strcmp(name, "inline")) return DISPLAY_INLINE;
    if (!strcmp(name, "block")) return DISPLAY_BLOCK;
    if (!strcmp(name, "none")) return DISPLAY_NONE;
    if (!strcmp(name, "flex")) return DISPLAY_FLEX;
    return DISPLAY_DEFAULT;
}

struct css_styling CSS_getDefaultStylesFromElement(struct xml_node node, struct xml_attributes *attribs) {
    struct css_styling styling;
    styling.display = DISPLAY_UNKNOWN;
    styling.color = CSS_COLOR_BLACK;
    styling.position = POSITION_STATIC;
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
    if (!strcmp(lower, "u")) {
        styling.underline = 1;
    }
    if (!strcmp(lower, "s")) {
        styling.strikethrough = 1;
    }
    if (!strcmp(lower, "dd")) {
        styling.tabbed = 1;
    }


    // TODO: support more styles
    char *attrib = XML_getAttributeByName(attribs, "style");
    if (attrib != NULL) {
        struct css_styles styles = CSS_makeEmptyStyles();
        CSS_parseInlineStyles(&styles, attrib);

        char *displayStyle = CSS_getStyleByName(&styles, "display");

        enum css_display display = CSS_styleNameToDisplay(displayStyle);
        if (display != DISPLAY_DEFAULT) {
            styling.display = display;
        }


        char *textDecorationStyle = CSS_getStyleByName(&styles, "text-decoration");
        if (textDecorationStyle != NULL) {
            if (!strcmp(textDecorationStyle, "underline")) {
                styling.underline = 1;
            } else if (!strcmp(textDecorationStyle, "line-through")) {
                styling.strikethrough = 1;
            } else if (!strcmp(textDecorationStyle, "none")) {
                // FIXME: It's more complex than this, see https://developer.mozilla.org/en-US/docs/Web/CSS/text-decoration
                styling.strikethrough = 0;
                styling.underline = 0;
            } else {
                fprintf(stderr, "Warning: Unsupported text-decoration \"%s\"\n", textDecorationStyle);
            }
        }


        char *colorStyle = CSS_getStyleByName(&styles, "color");
        if (colorStyle != NULL) {
            if (!strcmp(colorStyle, "blue")) {
                styling.color = CSS_COLOR_BLUE;
            } else if (!strcmp(colorStyle, "lime") || !strcmp(colorStyle, "green")) {
                styling.color = CSS_COLOR_GREEN;
            } else if (!strcmp(colorStyle, "red")) {
                styling.color = CSS_COLOR_RED;
            }
        }


        char *positionStyle = CSS_getStyleByName(&styles, "position");
        if (positionStyle != NULL) {
            if (!strcmp(positionStyle, "absolute")) {
                styling.position = POSITION_ABSOLUTE;
            } else if (!strcmp(positionStyle, "relative")) {
                styling.position = POSITION_RELATIVE;
            } else if (!strcmp(positionStyle, "static")) {
                styling.position = POSITION_STATIC;
            } else if (!strcmp(positionStyle, "fixed")) {
                styling.position = POSITION_FIXED;
            } else if (!strcmp(positionStyle, "sticky")) {
                styling.position = POSITION_STICKY;
            } else {
                styling.position = POSITION_UNKNOWN;
            }
        }
    }

    free(lower);
    return styling;
}

#endif
