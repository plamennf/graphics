#include "pch.h"

int string_length(const char *s) {
    if (!s) return 0;

    int length = 0;
    while (*s++) {
        length++;
    }
    return length;
}

char *copy_string(const char *s) {
    if (!s) return 0;

    int len = string_length(s);
    char *result = new char[len + 1];
    memcpy(result, s, len + 1);
    return result;
}

bool strings_match(const char *a, const char *b) {
    if (a == b) return true;
    if (!a || !b) return false;

    while (*a && *b) {
        if (*a != *b) {
            return false;
        }

        a++;
        b++;
    }

    return *a == 0 && *b == 0;
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

const char *find_character_from_right(const char *s, char c) {
    if (!s) return NULL;

    const char *end = s + string_length(s) - 1;
    while (*end != *s) {
        if (*end == c) return end;
        end--;
    }

    return NULL;
}

const char *find_character_from_left(const char *s, char c) {
    if (!s) return NULL;

    while (*s) {
        if (*s == c) return s;
        s++;
    }

    return NULL;
}
