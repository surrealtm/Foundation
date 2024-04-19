#include "intersect.h"

b8 ray_plane_intersection(const v3<f32> &ray_origin, const v3<f32> &ray_direction, const v3<f32> &plane_point, const v3<f32> &plane_normal, f32 *distance) {
    f32 denom = v3_dot_v3(plane_normal, ray_direction);
    if(denom < F32_EPSILON) {
        v3<f32> p0l0 = plane_point - ray_origin;
        *distance = v3_dot_v3(p0l0, plane_normal) / denom; 
        return *distance >= 0;
    }

    return false;
}

b8 ray_double_sided_plane_intersection(const v3<f32> &ray_origin, const v3<f32> &ray_direction, const v3<f32> &plane_point, const v3<f32> &plane_normal, f32 *distance) {
    f32 denom = fabsf(v3_dot_v3(plane_normal, ray_direction));

    if(denom > F32_EPSILON) {
        v3<f32> p0l0 = plane_point - ray_origin;
        *distance = v3_dot_v3(p0l0, plane_normal) / denom; 
        return *distance >= 0;
    }

    return false;
}

f32 point_plane_distance_signed(const v3<f32> &point, const v3<f32> &plane_point, const v3<f32> &plane_normal) {
    return v3_dot_v3(plane_normal, plane_point - point);
}
