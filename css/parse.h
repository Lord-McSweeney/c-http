#include "../utils/string.h"

#ifndef _CSS_PARSE
    #define _CSS_PARSE 1

struct css_style {
    char *name;
    char *value;
    int important;
};

struct css_styles {
    struct css_style *styles;
    int count;
};

char *CSS_getStyleByName(struct css_styles *styles, const char *name) {
    char *n = toLowerCase(name);
    for (int i = 0; i < styles->count; i ++) {
        if (!strcmp(styles->styles[i].name, n)) {
            free(n);
            return styles->styles[i].value;
        }
    }

    free(n);
    return NULL;
}

int CSS_getStyleIndexByName(struct css_styles *styles, const char *name) {
    char *n = toLowerCase(name);
    for (int i = 0; i < styles->count; i ++) {
        if (!strcmp(styles->styles[i].name, n)) {
            free(n);
            return i;
        }
    }

    free(n);
    return -1;
}

void CSS_appendStyle(struct css_styles *styles, struct css_style style) {
   int idx = CSS_getStyleIndexByName(styles, style.name);
    if (idx != -1) {
       struct css_style *styleAtIdx = &styles->styles[idx];
       int does_override = !styleAtIdx->important || style.important;

       if (does_override) {
           styleAtIdx->value = style.value;
           return;
       }
    }

    styles->count ++;
    styles->styles = (struct css_style *) realloc(styles->styles, sizeof(struct css_style) * styles->count);
    styles->styles[styles->count - 1] = style;
}

struct css_style CSS_makeStyle(const char *name, const char *value) {
    struct css_style style;
    style.name = makeStrCpy(name);
    style.value = makeStrCpy(value);
    return style;
}

struct css_styles *CSS_makeEmptyStyles() {
    struct css_styles *styles = (struct css_styles *) calloc(1, sizeof(struct css_styles));
    styles->count = 0;
    styles->styles = NULL;
    return styles;
}

enum _css_internal_style_parser_state {
    CSS_PARSE_STYLE_NAME,
    CSS_PARSE_STYLE_VALUE,
    CSS_PARSE_STYLE_IMPORTANT,
    CSS_PARSE_SINGLE_QUOTED_VALUE,
    CSS_PARSE_DOUBLE_QUOTED_VALUE,
    CSS_PARSE_UNKNOWN,
};

void freeCSSStyles(struct css_styles *styles) {
    for (int i = 0; i < styles->count; i ++) {
        free(styles->styles[i].name);
        free(styles->styles[i].value);
    }
    free(styles->styles);
    free(styles);
}

// This will add/overwrite the styles to the styles pointer passed to the function.
// The function is infallible. Large CSS (names/values >16384 bytes) will simply be ignored.
void CSS_parseInlineStyles(struct css_styles *styles, char *inputString) {
    int currentState = CSS_PARSE_UNKNOWN;

    struct css_style currentStyle;
    currentStyle.name = NULL;
    currentStyle.value = NULL;
    currentStyle.important = 0;

    char *currentDataContent = (char *) calloc(16384, sizeof(char));
    int currentDataUsage = 0;

    int len = strlen(inputString);

    for (int i = 0; i < len; i ++) {
        char curChar = inputString[i];
        switch(currentState) {
            // TODO
            case CSS_PARSE_STYLE_VALUE:
                if (curChar == ';') {
                    currentStyle.value = trimString(currentDataContent);
                    currentDataUsage = 0;
                    clrStr(currentDataContent);

                    CSS_appendStyle(styles, currentStyle);
                    currentStyle.name = NULL;
                    currentStyle.value = NULL;
                    currentStyle.important = 0;

                    currentState = CSS_PARSE_UNKNOWN;
                } else if (curChar == '!') {
                    currentStyle.value = trimString(currentDataContent);
                    currentDataUsage = 0;
                    clrStr(currentDataContent);

                    currentState = CSS_PARSE_STYLE_IMPORTANT;
                } /* else if (curChar == '"') { // These are TODO but leaving them uncommented breaks the if-elseif, since then quotes don't get parsed as style
                    // TODO
                    startedParsingValue = 1;
                } else if (curChar == '\'') {
                    // TODO
                    startedParsingValue = 1;
                }*/ else if (currentDataUsage <= 16382) {
                    currentDataContent[currentDataUsage] = curChar;
                    currentDataUsage ++;
                }
                break;
            case CSS_PARSE_STYLE_NAME:
                if (curChar == ':') {
                    currentStyle.name = trimString(currentDataContent);
                    currentDataUsage = 0;
                    clrStr(currentDataContent);

                    currentState = CSS_PARSE_STYLE_VALUE;
                } else if (curChar == ';') {
                    currentState = CSS_PARSE_UNKNOWN;
                } else if (currentDataUsage <= 16382) {
                    currentDataContent[currentDataUsage] = curChar;
                    currentDataUsage ++;
                }
                break;
            case CSS_PARSE_STYLE_IMPORTANT:
                if (curChar == ';') {
                    currentStyle.important = !strcmp(trimString(currentDataContent), "important");
                    currentDataUsage = 0;
                    clrStr(currentDataContent);

                    CSS_appendStyle(styles, currentStyle);
                    currentStyle.name = NULL;
                    currentStyle.value = NULL;
                    currentStyle.important = 0;

                    currentState = CSS_PARSE_UNKNOWN;
                } else if (currentDataUsage <= 16382) {
                    currentDataContent[currentDataUsage] = curChar;
                    currentDataUsage ++;
                }
                break;
            case CSS_PARSE_UNKNOWN:
                if (!isWhiteSpace(curChar)) {
                    currentState = CSS_PARSE_STYLE_NAME;
                    i --;
                }
                break;
        }
    }

    switch(currentState) {
        case CSS_PARSE_STYLE_IMPORTANT:
            currentStyle.important = !strcmp(trimString(currentDataContent), "important");

            CSS_appendStyle(styles, currentStyle);
            break;
        case CSS_PARSE_STYLE_VALUE:
            currentStyle.value = trimString(currentDataContent);

            CSS_appendStyle(styles, currentStyle);
            break;
    }

    free(currentDataContent);
}

#endif
