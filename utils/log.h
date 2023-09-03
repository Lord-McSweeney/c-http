#ifndef _LOGGING
    #define _LOGGING 1


    #include <stdarg.h>
    #include <unistd.h>

    #include "string.h"

void log_err(const char *format, ...) {
    if (isatty(2)) return;

    va_list args;
    va_start(args, format);

    char *alloc = (char *) calloc(strlen(format) + 16, sizeof(char));
    strcpy(alloc, "Error: ");
    strcat(alloc, format);
    vfprintf(stderr, alloc, args);
    free(alloc);

    va_end(args);
}

void log_warn(const char *format, ...) {
    if (isatty(2)) return;

    va_list args;
    va_start(args, format);

    char *alloc = (char *) calloc(strlen(format) + 16, sizeof(char));
    strcpy(alloc, "Warning: ");
    strcat(alloc, format);
    vfprintf(stderr, alloc, args);
    free(alloc);

    va_end(args);
}

#endif
