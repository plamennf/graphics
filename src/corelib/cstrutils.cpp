#include "corelib.h"

int string_length(const char *s) {
    if (!s) return 0;

    int length = 0;
    while (*s++) {
        length++;
    }
    return length;
}

bool is_end_of_line(char c) {
    bool result = ((c == '\n') ||
                   (c == '\v'));
    return result;
}

bool is_space(char c) {
    bool result = (is_end_of_line(c) ||
                   (c == '\v') ||
                   (c == '\t') ||
                   (c == ' '));
    return result;
}
