// Download a URL.

#include "../http/http.h"
#include "../http/response.h"
#include "../utils/string.h"

// Returns: String to return
// Params: error code
typedef char *(*onerror)(void *, int);

// Returns: String to return (NULL to continue)
// Params: (relative) redirect URL, number of redirects
typedef char *(*onredirect)(void *, char *, int);

// Params: absolute redirect URL, number of redirects
typedef void (*onredirectsuccess)(void *, char *, int);

// Returns: String to return
// Params: (relative) redirect URL, number of redirects
typedef char *(*onredirecterror)(void *, char *, int);

struct http_response downloadPage(
    void *ptr,
    char *userAgent,
    char *url,
    dataReceiveHandler handler,
    dataReceiveHandler finishHandler,
    int redirect_depth,
    onerror onerrorhandler,
    onredirect onredirecthandler,
    onredirectsuccess onredirectsuccesshandler,
    onredirecterror onredirecterrorhandler
) {
    struct http_response parsedResponse = http_makeHTTPRequest(url, userAgent, handler, finishHandler, ptr);
    
    if (parsedResponse.error) {
        char *handlerResult = onerrorhandler(ptr, parsedResponse.error);
        return HTTP_responseFromString(handlerResult, 0);
    }

    if (parsedResponse.do_redirect) {
        char *result = onredirecthandler(ptr, parsedResponse.redirect, redirect_depth);
        if (result) {
            return HTTP_responseFromString(result, 0);
        }

        struct http_url *curURL = http_url_from_string(url);
        if (curURL == NULL) {
             // should be impossible, since we just made an HTTP request successfully with this URL
             int _ = (*(char *)(0x0)); // segfault so we can figure out what's going on
        }

        char *absoluteURL = http_resolveRelativeURL(curURL, url, parsedResponse.redirect);

        struct http_url *absURL = http_url_from_string(absoluteURL);
        if (absURL == NULL) {
            char *handlerResult = onredirecterrorhandler(ptr, parsedResponse.redirect, redirect_depth);
            return HTTP_responseFromString(handlerResult, 0);
        }

        onredirectsuccesshandler(ptr, http_urlToString(absURL), redirect_depth);

        return downloadPage(ptr, userAgent, absoluteURL, handler, finishHandler, redirect_depth + 1, onerrorhandler, onredirecthandler, onredirectsuccesshandler, onredirecterrorhandler);
    }

    return parsedResponse;
}
