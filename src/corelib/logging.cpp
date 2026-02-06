#include "corelib.h"

#include <stdio.h>
#include <stdarg.h>

static FILE *log_file;

void init_logging(const char *log_filepath) {
    log_file = fopen(log_filepath, "wb");
}

void shutdown_logging() {
    if (log_file) {
        fclose(log_file);
    }
}

void logprintf(const char *fmt, ...) {
    char buf[4096];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    fprintf(stdout, "%s", buf);
    fflush(stdout);
    fprintf(log_file, "%s", buf);
    fflush(log_file);
}
