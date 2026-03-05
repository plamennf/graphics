#pragma once

#include <string.h>

struct String {
    int length;
    char *data;

    String();
    String(const char *s);
    String(const char *s, int length);

    inline char &operator[](int index) {
        Assert(index >= 0);
        Assert(index < length);
        return data[index];
    }

    inline char const &operator[](int index) const {
        Assert(index >= 0);
        Assert(index < length);
        return data[index];
    }

    inline char &get(int index) {
        Assert(index >= 0);
        Assert(index < length);
        return data[index];
    }

    inline char const &get(int index) const {
        Assert(index >= 0);
        Assert(index < length);
        return data[index];
    }

    inline bool is_empty() const {
        return length <= 0 || data == NULL;
    }
};

inline bool operator==(String const &a, String const &b) {
    if (a.length != b.length) return false;
    return memcmp(a.data, b.data, a.length) == 0;
}

inline bool operator!=(String const &a, String const &b) {
    return !(a == b);
}

const char *temp_c_string(String s);

String substring(String s, int start, int count = -1);
void advance(String *s);

bool starts_with(String s, String prefix);
bool ends_with(String s, String suffix);

String eat_spaces(String s);
String eat_trailing_spaces(String s);

String consume_next_line(String *s);
