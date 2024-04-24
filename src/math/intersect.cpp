#include "intersect.h"


f32 calculate_triangle_area(const v3<f32> &p0, const v3<f32> &p1, const v3<f32> &p2) {
    v3<f32> normal = v3_cross_v3(p1 - p0, p2 - p0);
    return v3_length(normal) / 2;
}


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



Triangle_Intersection_Result ray_triangle_intersection(const v3<f32> &ray_origin, const v3<f32> &ray_direction, const v3<f32> &p0, const v3<f32> &p1, const v3<f32> &p2) {
    //
    // Copied from:
    // https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/code/raytri_tam.pdf
    //
    Triangle_Intersection_Result result;

    v3<f32> edge1 = p1 - p0;
    v3<f32> edge2 = p2 - p0;

    v3<f32> pvector = v3_cross_v3(ray_direction, edge2);
    f32 determinant = v3_dot_v3(edge1, pvector);

    if(determinant < F32_EPSILON) {
        // Ray is parallel to the triangle plane.
        result.intersection = false;
        return result;
    }

    v3<f32> tvector = ray_origin - p0;
    f32 u = v3_dot_v3(tvector, pvector);
    if(u < 0.0f || u > determinant) {
        result.intersection = false;
        return result;
    }

    v3<f32> qvector = v3_cross_v3(tvector, edge1);
    f32 v = v3_dot_v3(ray_direction, qvector);
    if(v < 0.0f || u + v > determinant) {
        result.intersection = false;
        return result;
    }

    f32 inverse_determinant = 1.f / determinant;
    result.intersection     = true;
    result.distance         = v3_dot_v3(edge2, qvector) * inverse_determinant;
    result.u                = u * inverse_determinant; // Normalize to [0:1]
    result.v                = v * inverse_determinant; // Normalize to [0:1]
    return result;
}

Triangle_Intersection_Result ray_double_sided_triangle_intersection(const v3<f32> &ray_origin, const v3<f32> &ray_direction, const v3<f32> &p0, const v3<f32> &p1, const v3<f32> &p2) {
    //
    // Copied from:
    // https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/code/raytri_tam.pdf
    //
    Triangle_Intersection_Result result;
    
    v3<f32> edge1 = p1 - p0;
    v3<f32> edge2 = p2 - p0;

    v3<f32> pvector = v3_cross_v3(ray_direction, edge2);
    f32 determinant = v3_dot_v3(edge1, pvector);

    if(determinant > -F32_EPSILON && determinant < F32_EPSILON) {
        // Ray is parallel to the triangle plane.
        result.intersection = false;
        return result;
    }

    f32 inverse_determinant = 1.f / determinant;
    
    v3<f32> tvector = ray_origin - p0;
    f32 u = v3_dot_v3(tvector, pvector) * inverse_determinant;
    if(u < 0.0f || u > 1.0f) {
        result.intersection = false;
        return result;
    }

    v3<f32> qvector = v3_cross_v3(tvector, edge1);
    f32 v = v3_dot_v3(ray_direction, qvector) * inverse_determinant;
    if(v < 0.0f || u + v > 1.0f) {
        result.intersection = false;
        return result;
    }

    result.intersection     = true;
    result.distance         = v3_dot_v3(edge2, qvector) * inverse_determinant;
    result.u                = u;
    result.v                = v;
    return result;
}


void calculate_barycentric_coefficients(const v3<f32> &p0, const v3<f32> &p1, const v3<f32> &p2, const v3<f32> &point, f32 *u, f32 *v, f32 *w) {
    //
    // Copied from:
    // https://users.csc.calpoly.edu/~zwood/teaching/csc471/2017F/barycentric.pdf
    //
    v3<f32> n = v3_cross_v3(p1 - p0, p2 - p0);
    f32 inverse_n2 = 1.f / v3_dot_v3(n, n);
    
    v3<f32> t0 = v3_cross_v3(p2 - p1, point - p1);
    v3<f32> t1 = v3_cross_v3(p0 - p2, point - p2);
    v3<f32> t2 = v3_cross_v3(p1 - p0, point - p0);

    *u = v3_dot_v3(n, t0) * inverse_n2;
    *v = v3_dot_v3(n, t1) * inverse_n2;
    *w = v3_dot_v3(n, t2) * inverse_n2;
}
