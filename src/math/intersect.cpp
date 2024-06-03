#include "intersect.h"

template<typename T>
constexpr T intersect_epsilon();

template<>
f32 intersect_epsilon() { return F32_EPSILON; }

template<>
f64 intersect_epsilon() { return F64_EPSILON; }



template<typename T>
T calculate_triangle_area(const v3<T> &p0, const v3<T> &p1, const v3<T> &p2) {
    v3<T> normal = v3_cross_v3(p1 - p0, p2 - p0);
    return v3_length(normal) / 2;
}



template<typename T>
b8 ray_plane_intersection(const v3<T> &ray_origin, const v3<T> &ray_direction, const v3<T> &plane_point, const v3<T> &plane_normal, T *distance) {
    T denom = v3_dot_v3(plane_normal, ray_direction);
    if(denom < -intersect_epsilon<T>()) {
        v3<T> p0l0 = plane_point - ray_origin;
        *distance = v3_dot_v3(p0l0, plane_normal) / denom; 
        return *distance >= 0.f;
    }

    return false;
}

template<typename T>
b8 ray_double_sided_plane_intersection(const v3<T> &ray_origin, const v3<T> &ray_direction, const v3<T> &plane_point, const v3<T> &plane_normal, T *distance) {
    T denom = v3_dot_v3(plane_normal, ray_direction);

    if(fabs(denom) > intersect_epsilon<T>()) {
        v3<T> p0l0 = plane_point - ray_origin;
        *distance = v3_dot_v3(p0l0, plane_normal) / denom; 
        return true;
    }

    return false;
}



template<typename T>
T point_plane_distance_signed(const v3<T> &point, const v3<T> &plane_point, const v3<T> &plane_normal) {
    return v3_dot_v3(plane_normal, point - plane_point);
}

template<typename T>
v3<T> project_point_onto_triangle(const v3<T> &p, const v3<T> &a, const v3<T> &b, const v3<T> &c) {
    //
    // Copied from:
    // https://stackoverflow.com/questions/2924795/fastest-way-to-compute-point-to-triangle-distance-in-3d
    //

    v3<T> ab = b - a;
    v3<T> ac = c - a;

    v3<T> ap = p - a;
    T d1 = v3_dot_v3(ab, ap);
    T d2 = v3_dot_v3(ac, ap);
    if(d1 <= 0.f && d2 <= 0.f) return a;

    v3<T> bp = p - b;
    T d3 = v3_dot_v3(ab, bp);
    T d4 = v3_dot_v3(ac, bp);
    if(d3 >= 0.f && d4 <= d3) return b;

    v3<T> cp = p - c;
    T d5 = v3_dot_v3(ab, cp);
    T d6 = v3_dot_v3(ac, cp);
    if(d6 >= 0.f && d5 <= d6) return c;

    T v2 = d1 * d4 - d3 * d2;
    if(v2 <= 0.f && d1 >= 0.f && d3 <= 0.f) {
        T v = d1 / (d1 - d3);
        return a + v * ab;
    }

    T v1 = d5 * d2 - d1 * d6;
    if(v1 <= 0.f && d2 >= 0.f && d6 <= 0.f) {
        T v = d2 / (d2 - d6);
        return a + v * ac;
    }

    T v0 = d3 * d6 - d5 * d4;
    if(v0 <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) {
        T v = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + v * (c - b);
    }

    T denom = 1.f / (v0 + v1 + v2);
    T v = v1 * denom;
    T w = v2 * denom;
    return a + v * ab + w * ac;
}



template<typename T>
Triangle_Intersection_Result<T> ray_triangle_intersection(const v3<T> &ray_origin, const v3<T> &ray_direction, const v3<T> &p0, const v3<T> &p1, const v3<T> &p2) {
    //
    // Copied from:
    // https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/code/raytri_tam.pdf
    //
    Triangle_Intersection_Result<T> result;

    v3<T> edge1 = p1 - p0;
    v3<T> edge2 = p2 - p0;

    v3<T> pvector = v3_cross_v3(ray_direction, edge2);
    T determinant = v3_dot_v3(edge1, pvector);

    if(determinant < intersect_epsilon<T>()) {
        // Ray is parallel to the triangle plane.
        result.intersection = false;
        return result;
    }

    v3<T> tvector = ray_origin - p0;
    T u = v3_dot_v3(tvector, pvector);
    if(u < 0.0f || u > determinant) {
        result.intersection = false;
        return result;
    }

    v3<T> qvector = v3_cross_v3(tvector, edge1);
    T v = v3_dot_v3(ray_direction, qvector);
    if(v < 0.0f || u + v > determinant) {
        result.intersection = false;
        return result;
    }

    T inverse_determinant = 1.f / determinant;
    result.intersection     = true;
    result.distance         = v3_dot_v3(edge2, qvector) * inverse_determinant;
    result.u                = u * inverse_determinant; // Normalize to [0:1]
    result.v                = v * inverse_determinant; // Normalize to [0:1]
    return result;
}

template<typename T>
Triangle_Intersection_Result<T> ray_double_sided_triangle_intersection(const v3<T> &ray_origin, const v3<T> &ray_direction, const v3<T> &p0, const v3<T> &p1, const v3<T> &p2) {
    //
    // Copied from:
    // https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/code/raytri_tam.pdf
    //
    Triangle_Intersection_Result<T> result;
    
    v3<T> edge1 = p1 - p0;
    v3<T> edge2 = p2 - p0;

    v3<T> pvector = v3_cross_v3(ray_direction, edge2);
    T determinant = v3_dot_v3(edge1, pvector);

    if(determinant > -intersect_epsilon<T>() && determinant < intersect_epsilon<T>()) {
        // Ray is parallel to the triangle plane.
        result.intersection = false;
        return result;
    }

    T inverse_determinant = 1.f / determinant;
    
    v3<T> tvector = ray_origin - p0;
    T u = v3_dot_v3(tvector, pvector) * inverse_determinant;
    if(u < 0.0f || u > 1.0f) {
        result.intersection = false;
        return result;
    }

    v3<T> qvector = v3_cross_v3(tvector, edge1);
    T v = v3_dot_v3(ray_direction, qvector) * inverse_determinant;
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



template<typename T>
b8 ray_triangle_intersection(const v3<T> &ray_origin, const v3<T> &ray_direction, const v3<T> &p0, const v3<T> &p1, const v3<T> &p2, T *distance) {
    Triangle_Intersection_Result<T> result = ray_triangle_intersection(ray_origin, ray_direction, p0, p1, p2);
    *distance = result.distance;
    return result.intersection;
}

template<typename T>
b8 ray_double_sided_triangle_intersection(const v3<T> &ray_origin, const v3<T> &ray_direction, const v3<T> &p0, const v3<T> &p1, const v3<T> &p2, T *distance) {
    Triangle_Intersection_Result<T> result = ray_double_sided_triangle_intersection(ray_origin, ray_direction, p0, p1, p2);
    *distance = result.distance;
    return result.intersection;
}



template<typename T>
b8 point_inside_triangle(const v2<T> &point, const v2<T> &p0, const v2<T> &p1, const v2<T> &p2) {
    //
    // https://stackoverflow.com/questions/2049582/how-to-determine-if-a-point-is-in-a-2d-triangle
    //
#define sign(p0, p1, p2) ((p0.x - p2.x) * (p1.y - p2.y) - (p1.x - p2.x) * (p0.y - p2.y))
    
    T d0 = sign(point, p0, p1);
    T d1 = sign(point, p1, p2);
    T d2 = sign(point, p2, p0);

#undef sign

    // Handle cases in which the point lies on the edge of a triangle, in which case we may
    // get very small positive or negative numbers.
    b8 negative = (d0 < -intersect_epsilon<T>()) || (d1 < -intersect_epsilon<T>()) || (d2 < -intersect_epsilon<T>());
    b8 positive = (d0 >  intersect_epsilon<T>()) || (d1 >  intersect_epsilon<T>()) || (d2 >  intersect_epsilon<T>());

    return !(negative && positive);
}



template<typename T>
void calculate_barycentric_coefficients(const v3<T> &p0, const v3<T> &p1, const v3<T> &p2, const v3<T> &point, T *u, T *v, T *w) {
    //
    // Copied from:
    // https://users.csc.calpoly.edu/~zwood/teaching/csc471/2017F/barycentric.pdf
    //
    v3<T> n = v3_cross_v3(p1 - p0, p2 - p0);
    T inverse_determinant = 1.f / v3_dot_v3(n, n);
    
    v3<T> t0 = v3_cross_v3(p2 - p1, point - p1);
    v3<T> t1 = v3_cross_v3(p0 - p2, point - p2);
    v3<T> t2 = v3_cross_v3(p1 - p0, point - p0);

    *u = clamp(v3_dot_v3(n, t0) * inverse_determinant, 0, 1);
    *v = clamp(v3_dot_v3(n, t1) * inverse_determinant, 0, 1);
    *w = clamp(1.f - *u - *v, 0, 1);
}
