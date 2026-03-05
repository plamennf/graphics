#pragma once

struct Rectangle2i {
    int x;
    int y;
    int width;
    int height;
};

inline Rectangle2i aspect_ratio_fit(int window_width, int window_height, int render_width, int render_height) {
    Rectangle2i result = {};
    if (!window_width || !window_height || !render_width || !render_height) return result;

    float optimal_window_width = (float)window_height * ((float)render_width / (float)render_height);
    float optimal_window_height = (float)window_width * ((float)render_height / (float)render_width);

    if ((float)window_width > optimal_window_width) {
        result.y = 0;
        result.height = (int)window_height;

        result.width = (int)optimal_window_width;
        result.x = (window_width - result.width) / 2;
    } else {
        result.x = 0;
        result.width = (int)window_width;

        result.height = (int)optimal_window_height;
        result.y = (window_height - result.height) / 2;
    }

    return result;
}

struct Rectangle2 {
    float x;
    float y;
    float width;
    float height;
};

inline bool is_touching_left(Rectangle2 a, Rectangle2 b, Vector2 vel) {
    float al = a.x;
    float ar = al + a.width;
    float ab = a.y;
    float at = a.y + a.height;
    
    float bl = b.x;
    float br = bl + b.width;
    float bb = b.y;
    float bt = b.y + b.height;

    return ar + vel.x > bl &&
        al < bl &&
        ab < bt &&
        at > bb;
}

inline bool is_touching_right(Rectangle2 a, Rectangle2 b, Vector2 vel) {
    float al = a.x;
    float ar = al + a.width;
    float ab = a.y;
    float at = a.y + a.height;

    float bl = b.x;
    float br = bl + b.width;
    float bb = b.y;
    float bt = b.y + b.height;

    return al + vel.x < br &&
        ar > br &&
        ab < bt &&
        at > bb;
}

inline bool is_touching_top(Rectangle2 a, Rectangle2 b, Vector2 vel) {
    float al = a.x;
    float ar = al + a.width;
    float ab = a.y;
    float at = a.y + a.height;

    float bl = b.x;
    float br = bl + b.width;
    float bb = b.y;
    float bt = b.y + b.height;

    return ab + vel.y < bt &&
        at > bt &&
        ar > bl &&
        al < br;
}

inline bool is_touching_bottom(Rectangle2 a, Rectangle2 b, Vector2 vel) {
    float al = a.x;
    float ar = al + a.width;
    float ab = a.y;
    float at = a.y + a.height;

    float bl = b.x;
    float br = bl + b.width;
    float bb = b.y;
    float bt = b.y + b.height;

    return at + vel.y > bb &&
        ab < bb &&
        ar > bl &&
        al < br;
}

inline bool are_intersecting(Rectangle2 a, Rectangle2 b) {
#if 0
    // @TODO: Speed.
    bool result = (is_touching_left(a, b, v2(0, 0)) ||
                   is_touching_right(a, b, v2(0, 0)) ||
                   is_touching_top(a, b, v2(0, 0)) ||
                   is_touching_bottom(a, b, v2(0, 0)));
    return result;
#else
    int d0 = (b.x + b.width)  < a.x;
    int d1 = (a.x + a.width)  < b.x;
    int d2 = (b.y + b.height) < a.y;
    int d3 = (a.y + a.height) < b.y;
    return !(d0 | d1 | d2 | d3);
#endif
}

inline bool are_rect_and_circle_colliding(Rectangle2 rect, Vector2 position, float radius) {
    float closest_x = position.x;
    float closest_y = position.y;
    clamp(&closest_x, rect.x, rect.x + rect.width);
    clamp(&closest_y, rect.y, rect.y + rect.height);

    float dx = position.x - closest_x;
    float dy = position.y - closest_y;

    return (dx * dx + dy * dy) <= (radius * radius);
}
