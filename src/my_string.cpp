#include "pch.h"

static char temp_c_string_buffer[8092];

String::String() :
    length(0), data(NULL) {}

String::String(const char *s) {
    length = string_length(s);
    data   = (char *)s;
}

String::String(const char *s, int length)
    : length(length), data((char *)s) {}

const char *temp_c_string(String s) {
    memcpy(temp_c_string_buffer, s.data, s.length);
    temp_c_string_buffer[s.length] = 0;
    return (const char *)temp_c_string_buffer;
}

String substring(String s, int start, int count) {
    if (start < 0) return {};
    if (start >= s.length) return {};

    if (count == -1) {
        count = s.length - start;
    }
    
    if (start + count > s.length) count = s.length - start;

    String result;

    result.length = count;
    result.data   = s.data + start;

    return result;
}

void advance(String *s) {
    if (!s) return;
    if (s->is_empty()) return;

    s->data   += 1;
    s->length -= 1;
}

bool starts_with(String s, String prefix) {
    if (prefix.length > s.length) return false;
    return memcmp(s.data, prefix.data, prefix.length) == 0;
}

bool ends_with(String s, String suffix) {
    if (suffix.length > s.length) return false;
    return memcmp(s.data + s.length - suffix.length, suffix.data, suffix.length) == 0;
}

String eat_spaces(String s) {
    if (s.is_empty()) return {};

    String result = s;
    while (is_space(result[0])) {
        advance(&result);
    }

    return result;
}

String eat_trailing_spaces(String s) {
    if (s.is_empty()) return {};

    String result = s;
    while (is_space(result[result.length - 1])) {
        result.length -= 1;
    }

    return result;
}

String consume_next_line(String *s) {
    if (!s) return {};
    if (s->is_empty()) return {};

    String result = *s;
    while (s->get(0) != '\n' && s->get(0) != '\r') {
        advance(s);
    }

    result.length = result.length - s->length;
    
    if (s->length) {
        advance(s);

        if (s->length) {
            if (s->get(0) == '\r') {
                if (s->get(0) == '\n') {
                    advance(s);
                }
            }
        }
    }

    return result;
}
