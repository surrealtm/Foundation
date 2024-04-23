#include "intersect.h"

b8 ray_plane_intersection(const v3<f32> &ray_origin, const v3<f32> &ray_direction, const v3<f32> &plane_point, const v3<f32> &plane_normal, f32 *distance) {
    f32 denom = v3_dot_v3(plane_normal, ray_direction);
    if(denom < -0.0f) {
        v3<f32> p0l0 = plane_point - ray_origin;
        *distance = v3_dot_v3(p0l0, plane_normal) / denom; 
        return *distance >= 0;
    }

    return false;
}

b8 ray_double_sided_plane_intersection(const v3<f32> &ray_origin, const v3<f32> &ray_direction, const v3<f32> &plane_point, const v3<f32> &plane_normal, f32 *distance) {
    f32 denom = v3_dot_v3(plane_normal, ray_direction);

    if(fabsf(denom) > 0.0f) {
        v3<f32> p0l0 = plane_point - ray_origin;
        *distance = v3_dot_v3(p0l0, plane_normal) / denom; 
        return true;
    }

    return false;
}

f32 point_plane_distance_signed(const v3<f32> &point, const v3<f32> &plane_point, const v3<f32> &plane_normal) {
    return v3_dot_v3(plane_normal, point - plane_point);
}

v3<f32> project_point_onto_triangle(const v3<f32> &p, const v3<f32> &a, const v3<f32> &b, const v3<f32> &c) {
    //
    // Copied from:
    // https://stackoverflow.com/questions/2924795/fastest-way-to-compute-point-to-triangle-distance-in-3d
    //

    v3<f32> ab = b - a;
    v3<f32> ac = c - a;

    v3<f32> ap = p - a;
    f32 d1 = v3_dot_v3(ab, ap);
    f32 d2 = v3_dot_v3(ac, ap);
    if(d1 <= 0.f && d2 <= 0.f) return a;

    v3<f32> bp = p - b;
    f32 d3 = v3_dot_v3(ab, bp);
    f32 d4 = v3_dot_v3(ac, bp);
    if(d3 >= 0.f && d4 <= d3) return b;

    v3<f32> cp = p - c;
    f32 d5 = v3_dot_v3(ab, cp);
    f32 d6 = v3_dot_v3(ac, cp);
    if(d6 >= 0.f && d5 <= d6) return c;

    f32 v2 = d1 * d4 - d3 * d2;
    if(v2 <= 0.f && d1 >= 0.f && d3 <= 0.f) {
        f32 v = d1 / (d1 - d3);
        return a + v * ab;
    }

    f32 v1 = d5 * d2 - d1 * d6;
    if(v1 <= 0.f && d2 >= 0.f && d6 <= 0.f) {
        f32 v = d2 / (d2 - d6);
        return a + v * ac;
    }

    f32 v0 = d3 * d6 - d5 * d4;
    if(v0 <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) {
        f32 v = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + v * (c - b);
    }

    f32 denom = 1.f / (v0 + v1 + v2);
    f32 v = v1 * denom;
    f32 w = v2 * denom;
    return a + v * ab + w * ac;
}
