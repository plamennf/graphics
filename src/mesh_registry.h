#pragma once

struct Mesh;

struct Mesh_Registry {
    String_Hash_Table <Mesh *> mesh_lookup;

    ~Mesh_Registry();
    
    Mesh *find_or_load(String name);
};
