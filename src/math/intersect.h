#pragma once

#include "../foundation.h"
#include "v2.h"
#include "v3.h"

//
// I'm not a huge fan of templating all this (C++ templates are bad), but I also don't want to
// copy-pasta everything or write a code generator for it, so here we are...
//

template<typename T>
T calculate_triangle_area(const v3<T> &p0, const v3<T> &p1, const v3<T> &p2);

template<typename T>
b8 ray_plane_intersection(const v3<T> &ray_origin, const v3<T> &ray_direction, const v3<T> &plane_point, const v3<T> &plane_normal, T *distance);
template<typename T>
b8 ray_double_sided_plane_intersection(const v3<T> &ray_origin, const v3<T> &ray_direction, const v3<T> &plane_point, const v3<T> &plane_normal, T *distance);

template<typename T>
T point_plane_distance_signed(const v3<T> &point, const v3<T> &plane_point, const v3<T> &plane_normal);
template<typename T>
v3<T> project_point_onto_triangle(const v3<T> &p, const v3<T> &a, const v3<T> &b, const v3<T> &c);

template<typename T>
struct Triangle_Intersection_Result {
	b8 intersection; // Whether any intersection actually occured. If not, all other members are undefined.
	T distance; // The unnormalized distance from the ray origin to the intersection point, measured in ray_direction units!
	T u, v; // The normalized barycentric coordinate factors of p1 and p2 respectively, normalized to [0:1].
};

template<typename T>
Triangle_Intersection_Result<T> ray_triangle_intersection(const v3<T> &ray_origin, const v3<T> &ray_direction, const v3<T> &p0, const v3<T> &p1, const v3<T> &p2);
template<typename T>
Triangle_Intersection_Result<T> ray_double_sided_triangle_intersection(const v3<T> &ray_origin, const v3<T> &ray_direction, const v3<T> &p0, const v3<T> &p1, const v3<T> &p2);

template<typename T>
b8 ray_triangle_intersection(const v3<T> &ray_origin, const v3<T> &ray_direction, const v3<T> &p0, const v3<T> &p1, const v3<T> &p2, T *distance);
template<typename T>
b8 ray_double_sided_triangle_intersection(const v3<T> &ray_origin, const v3<T> &ray_direction, const v3<T> &p0, const v3<T> &p1, const v3<T> &p2, T *distance);

template<typename T>
b8 point_inside_triangle(const v2<T> &point, const v2<T> &p0, const v2<T> &p1, const v2<T> &p2);

template<typename T>
void calculate_barycentric_coefficients(const v3<T> &p0, const v3<T> &p1, const v3<T> &p2, const v3<T> &point, T *u, T *v, T *w);

// Because C++ is a terrible language, we need to supply the template definitions in the header file for
// instantiation to work correctly... This feels horrible but still better than just inlining the code I guess.
#include "intersect.inl"
