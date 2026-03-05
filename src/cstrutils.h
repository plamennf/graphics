#pragma once

int string_length(const char *s);
char *copy_string(const char *s);
bool strings_match(const char *a, const char *b);

bool is_end_of_line(char c);
bool is_space(char c);

const char *find_character_from_right(const char *s, char c);
const char *find_character_from_left(const char *s, char c);
