#include "../utils/string.h"

#ifndef _BLOCK_INLINE
    #define _BLOCK_INLINE 1

int CSS_isDefaultBlock(const char *nodeName) {
    char *lower = toLowerCase(nodeName);
    int res = !strcmp(lower, "div") || !strcmp(lower, "p") || !strcmp(lower, "hr") || !strcmp(lower, "header") || !strcmp(lower, "aside") || !strcmp(lower, "h1") || !strcmp(lower, "h2") || !strcmp(lower, "h3") || !strcmp(lower, "h4") || !strcmp(lower, "h5") || !strcmp(lower, "h6") || !strcmp(lower, "blockquote") || !strcmp(lower, "dt") || !strcmp(lower, "tspan") || !strcmp(lower, "li") || !strcmp(lower, "dd") || !strcmp(lower, "tr") || !strcmp(lower, "button") || !strcmp(lower, "center") || !strcmp(lower, "ul") || !strcmp(lower, "ol") || !strcmp(lower, "pre");
    free(lower);
    return res;
}

int CSS_isDefaultInline(const char *nodeName) {
    char *lower = toLowerCase(nodeName);
    int res = !strcmp(lower, "a") || !strcmp(lower, "span") || !strcmp(lower, "font") || !strcmp(lower, "b") || !strcmp(lower, "i") || !strcmp(lower, "u") || !strcmp(lower, "img") || !strcmp(lower, "strong") || !strcmp(lower, "em") || !strcmp(lower, "sup") || !strcmp(lower, "td") || !strcmp(lower, "input") || !strcmp(lower, "kbd") || !strcmp(lower, "code") || !strcmp(lower, "time");
    free(lower);
    return res;
}

int CSS_isDefaultHidden(const char *nodeName) {
    char *lower = toLowerCase(nodeName);
    int res = !strcmp(lower, "script") || !strcmp(lower, "style") || !strcmp(lower, "head") || !strcmp(lower, "meta") || !strcmp(lower, "link") || !strcmp(lower, "title") || !strcmp(lower, "svg");
    free(lower);
    return res;
}

#endif
