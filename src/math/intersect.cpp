#include "intersect.h"

b8 ray_plane_intersection(const v3<f32> &point, const v3<f32> &normal, const v3<f32> &origin, const v3<f32> &direction, f32 &distance) {
    f32 denom = v3_dot_v3(normal, direction);
    if(denom < F32_EPSILON) {
        v3<f32> p0l0 = point - origin;
        distance = v3_dot_v3(p0l0, normal) / denom; 
        return distance >= 0;
    }

    return false;
}

b8 ray_double_sided_plane_intersection(const v3<f32> &point, const v3<f32> &normal, const v3<f32> &origin, const v3<f32> &direction, f32 &distance) {
    f32 denom = fabs(v3_dot_v3(normal, direction));

    if(denom < F32_EPSILON) {
        v3<f32> p0l0 = point - origin;
        distance = v3_dot_v3(p0l0, normal) / denom; 
        return distance >= 0;
    }

    return false;
}
