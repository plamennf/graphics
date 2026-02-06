#include "pch.h"

#include <stdio.h>

int main(int argc, char *argv[]) {
    init_temporary_storage(Kilobytes(40));
    
    init_logging();
    defer { shutdown_logging(); };

    char *s = TAllocArray(char, 6);
    s[0] = 'H';
    s[1] = 'e';
    s[2] = 'l';
    s[3] = 'l';
    s[4] = 'o';
    s[5] = '\0';
    
    logprintf("%s, World!\n", s);
    
    return 0;
}
