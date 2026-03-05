#pragma once

struct Texture;

struct Texture_Registry {
    String_Hash_Table <Texture *> texture_lookup;

    ~Texture_Registry();
    
    Texture *find_or_load(String name);
};
