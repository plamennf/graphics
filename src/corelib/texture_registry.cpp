#include "corelib.h"
#include "texture_registry.h"

#include <stdio.h>

// TODO: Copy-paste: Move this into corelib
static bool file_exists(const char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

Texture *Texture_Registry::find_or_load(String _name) {
    const char *name = temp_c_string(_name);
    Texture **_texture = texture_lookup.find((char *)name);
    if (_texture) return *_texture;

    const char *extensions[] = {
        "png",
        "jpg",
        "bmp",
    };

    char full_path[256]; bool full_path_exists = false;
    for (int i = 0; i < ArrayCount(extensions); i++) {
        snprintf(full_path, sizeof(full_path), "data/textures/%s.%s", name, extensions[i]);
        if (file_exists(full_path)) {
            full_path_exists = true;
            break;
        }
    }

    if (!full_path_exists) {
        logprintf("No texture '%s' found in 'data/textures'\n", name);
        return NULL;
    }

    Texture *texture = new Texture();
    if (!load_texture(texture, full_path)) {
        delete texture;
        return NULL;
    }

    texture_lookup.add((char *)name, texture);

    return texture;
}

Texture_Registry::~Texture_Registry() {
    for (int i = 0; i < texture_lookup.allocated; i++) {
        if (!texture_lookup.occupancy_mask[i]) continue;

        Texture *texture = texture_lookup.buckets[i].value;
        release_texture(texture);
    }
}
