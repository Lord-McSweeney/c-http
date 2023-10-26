#include "../utils/string.h"

struct http_cookie {
    char *name;
    char *value;

    char *hostname;
};

struct http_cookie_store {
    struct http_cookie *cookies;
    int count;
};

int HTTP_getCookieIndex(struct http_cookie_store *store, char *name, char *hostname) {
    if (store == NULL) return -1;

    int len = store->count;
    for (int i = 0; i < len; i ++) {
        if (!strcmp(store->cookies[i].hostname, hostname) && !strcmp(store->cookies[i].name, name)) {
            return i;
        }
    }

    return -1;
}

struct http_cookie_store *HTTP_makeCookieStore() {
    struct http_cookie_store *cookie_store = (struct http_cookie_store *) calloc(1, sizeof(struct http_cookie_store));
    cookie_store->count = 0;
    cookie_store->cookies = NULL;

    return cookie_store;
}

void HTTP_addCookieToStore(struct http_cookie_store *store, struct http_cookie cookie) {
    if (store == NULL) return;

    int idx = HTTP_getCookieIndex(store, cookie.name, cookie.hostname);
    if (idx != -1) {
        store->cookies[idx] = cookie;
        return;
    }

    store->count ++;
    store->cookies = (struct http_cookie *) realloc(store->cookies, sizeof(struct http_cookie) * store->count);
    store->cookies[store->count - 1] = cookie;
}

struct http_cookie HTTP_makeCookie(char *name, char *value, char *hostname) {
    struct http_cookie cookie;
    cookie.name = name;
    cookie.value = value;
    cookie.hostname = hostname;

    return cookie;
}

struct http_cookie HTTP_parseCookieString(char *string, char *hostname) {
    int doneParsingName = 0;
    int len = strlen(string);
    char *name = (char *) calloc(len, sizeof(char));
    char *value = (char *) calloc(len, sizeof(char));
    int idx = 0;
    for (int i = 0; i < len; i ++) {
        char curChar = string[i];
        if (!doneParsingName) {
            if (curChar == '=') {
                idx = 0;
                doneParsingName = 1;
                continue;
            } else if (curChar == ';') {
                break;
            } else {
                name[idx] = curChar;
                idx ++;
            }
        } else {
            if (curChar == ';') {
                break;
            } else {
                value[idx] = curChar;
                idx ++;
            }
        }
    }

    char *nameCpy = makeStrCpy(name);
    free(name);

    char *valueCpy = makeStrCpy(value);
    free(value);

    return HTTP_makeCookie(nameCpy, valueCpy, makeStrCpy(hostname));
}

char *HTTP_cookieStoreToString(struct http_cookie_store *store, char *hostname) {
    if (store == NULL) return makeStrCpy("");

    char *result = (char *) calloc(1, sizeof(char));
    int count = store->count;
    for (int i = 0; i < count; i ++) {
        struct http_cookie cookie = store->cookies[i];

        // Should this be case-insensitive?
        if (!strcmp(cookie.hostname, hostname)) {
            result = (char *) realloc(result, strlen(result) + strlen(cookie.name) + strlen(cookie.value) + 8);
            strcat(result, cookie.name);
            strcat(result, "=");
            strcat(result, cookie.value);
            if (i != count - 1) {
                strcat(result, "; ");
            }
        }
    }
    char *res = makeStrCpy(result);
    free(result);
    return res;
}

char *HTTP_cookieStoreToStringPretty(struct http_cookie_store *store, char *hostname) {
    if (store == NULL || store->count == 0) return makeStrCpy("None");

    char *result = (char *) calloc(1, sizeof(char));
    int count = store->count;
    for (int i = 0; i < count; i ++) {
        struct http_cookie cookie = store->cookies[i];

        // Should this be case-insensitive?
        if (!strcmp(cookie.hostname, hostname)) {
            result = (char *) realloc(result, strlen(result) + strlen(cookie.name) + strlen(cookie.value) + 8);
            strcat(result, cookie.name);
            strcat(result, "=");
            strcat(result, cookie.value);
            if (i != count - 1) {
                strcat(result, "\n\n");
            }
        }
    }
    char *res = makeStrCpy(result);
    free(result);
    return res;
}
