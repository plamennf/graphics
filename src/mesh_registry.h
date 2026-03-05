#pragma once

struct Mesh;

struct Mesh_Registry {
    String_Hash_Table <Mesh *> mesh_lookup;
    Array <const char *> all_names_in_order_of_loading;
    
    ~Mesh_Registry();
    
    Mesh *find_or_load(const char *name);

    void recursive_init_all();
};
