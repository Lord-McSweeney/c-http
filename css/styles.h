#include "../utils/log.h"
#include "../xml/attributes.h"

#include "block-inline.h"
#include "parse.h"
#include "selectors.h"

#ifndef _CSS_STYLES
    #define _CSS_STYLES 1

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
    CSS_COLOR_WHEAT,
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

enum css_pointer_events {
    POINTER_EVENTS_NONE,
    POINTER_EVENTS_UNKNOWN,
    POINTER_EVENTS_DEFAULT, // default
    POINTER_EVENTS_ALL,
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
    enum css_pointer_events pointer_events;
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

struct css_styling CSS_getDefaultStyles() {
    struct css_styling styling;
    styling.display = DISPLAY_UNKNOWN;
    styling.color = CSS_COLOR_BLACK;
    styling.position = POSITION_STATIC;
    styling.bold = 0;
    styling.italic = 0;
    styling.underline = 0;
    styling.strikethrough = 0;
    styling.tabbed = 0;

    return styling;
}

struct css_styling CSS_getDefaultStylesFromElement(struct xml_node node, struct xml_attributes *attribs, struct css_persistent_styles *persistentStyles) {
    struct css_styling styling;
    styling.display = DISPLAY_UNKNOWN;
    styling.color = CSS_COLOR_BLACK;
    styling.position = POSITION_STATIC;
    styling.pointer_events = POINTER_EVENTS_DEFAULT;
    styling.bold = 0;
    styling.italic = 0;
    styling.underline = 0;
    styling.strikethrough = 0;
    styling.tabbed = 0;

    // Default user-agent styles
    if (CSS_isDefaultBlock(node.name)) {
        styling.display = DISPLAY_BLOCK;
    } else if (CSS_isDefaultInline(node.name)) {
        styling.display = DISPLAY_INLINE;
    } else if (CSS_isDefaultHidden(node.name)) {
        styling.display = DISPLAY_NONE;
    }

    char *lower = toLowerCase(node.name);

    // Should these be set from some sort of embedded stylesheet rather than being hard-coded in?
    if (!strcmp(lower, "a")) {
        char *href = XML_getAttributeByName(attribs, "href");
        if (href) {
            styling.color = CSS_COLOR_BLUE;
            styling.underline = 1;
        }
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
    struct css_styles *styles = CSS_makeEmptyStyles();

    CSS_getCSSTextForNode(styles, &node, attribs, persistentStyles);

    char *attrib = XML_getAttributeByName(attribs, "style");
    if (attrib != NULL) {
        CSS_parseInlineStyles(styles, attrib);
    }

    char *displayStyle = CSS_getStyleByName(styles, "display");

    enum css_display display = CSS_styleNameToDisplay(displayStyle);
    if (display != DISPLAY_DEFAULT) {
        styling.display = display;
    }


    char *textDecorationStyle = CSS_getStyleByName(styles, "text-decoration");
    if (textDecorationStyle != NULL) {
        if (!strcmp(textDecorationStyle, "underline")) {
            styling.underline = 1;
        } else if (!strcmp(textDecorationStyle, "line-through")) {
            styling.strikethrough = 1;
        } else if (!strcmp(textDecorationStyle, "none")) {
            styling.strikethrough = 0;
            styling.underline = 0;
        } else {
            log_warn("Unsupported text-decoration \"%s\"\n", textDecorationStyle);
        }
    }


    char *colorStyle = CSS_getStyleByName(styles, "color");
    if (colorStyle != NULL) {
        if (!strcmp(colorStyle, "blue")) {
            styling.color = CSS_COLOR_BLUE;
        } else if (!strcmp(colorStyle, "lime") || !strcmp(colorStyle, "green")) {
            styling.color = CSS_COLOR_GREEN;
        } else if (!strcmp(colorStyle, "red")) {
            styling.color = CSS_COLOR_RED;
        } else if (!strcmp(colorStyle, "wheat")) {
            styling.color = CSS_COLOR_WHEAT;
        }
    }


    char *positionStyle = CSS_getStyleByName(styles, "position");
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


    char *pointerEventsStyle = CSS_getStyleByName(styles, "pointer-events");
    if (pointerEventsStyle != NULL) {
        if (!strcmp(pointerEventsStyle, "none")) {
            styling.pointer_events = POINTER_EVENTS_NONE;
        } else if (!strcmp(pointerEventsStyle, "all")) {
            styling.pointer_events = POINTER_EVENTS_ALL;
        } else {
            styling.pointer_events = POINTER_EVENTS_UNKNOWN;
        }
    }

    freeCSSStyles(styles);

    free(lower);
    return styling;
}

#endif
