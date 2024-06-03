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


//
// I do not want to include all that template definition mess in this header file, so instead I'm just
// providing the explicit instantiations here, since all these procedures should only ever be used with
// floating point numbers anyway. I hate this so much...
//

template f32 calculate_triangle_area(const v3f&, const v3f&, const v3f&);
template f64 calculate_triangle_area(const v3d&, const v3d&, const v3d&);

template b8 ray_plane_intersection(const v3f&, const v3f&, const v3f&, const v3f&, f32*);
template b8 ray_plane_intersection(const v3d&, const v3d&, const v3d&, const v3d&, f64*);

template b8 ray_double_sided_plane_intersection(const v3f&, const v3f&, const v3f&, const v3f&, f32*);
template b8 ray_double_sided_plane_intersection(const v3d&, const v3d&, const v3d&, const v3d&, f64*);

template f32 point_plane_distance_signed(const v3f&, const v3f&, const v3f&);
template f64 point_plane_distance_signed(const v3d&, const v3d&, const v3d&);

template v3f project_point_onto_triangle(const v3f&, const v3f&, const v3f&, const v3f&);
template v3d project_point_onto_triangle(const v3d&, const v3d&, const v3d&, const v3d&);

template Triangle_Intersection_Result<f32> ray_triangle_intersection(const v3f&, const v3f&, const v3f&, const v3f&, const v3f&);
template Triangle_Intersection_Result<f64> ray_triangle_intersection(const v3d&, const v3d&, const v3d&, const v3d&, const v3d&);

template Triangle_Intersection_Result<f32> ray_double_sided_triangle_intersection(const v3f&, const v3f&, const v3f&, const v3f&, const v3f&);
template Triangle_Intersection_Result<f64> ray_double_sided_triangle_intersection(const v3d&, const v3d&, const v3d&, const v3d&, const v3d&);

template b8 ray_triangle_intersection(const v3f&, const v3f&, const v3f&, const v3f&, const v3f&, f32*);
template b8 ray_triangle_intersection(const v3d&, const v3d&, const v3d&, const v3d&, const v3d&, f64*);

template b8 ray_double_sided_triangle_intersection(const v3f&, const v3f&, const v3f&, const v3f&, const v3f&, f32*);
template b8 ray_double_sided_triangle_intersection(const v3d&, const v3d&, const v3d&, const v3d&, const v3d&, f64*);

template b8 point_inside_triangle(const v2f&, const v2f&, const v2f&, const v2f&);
template b8 point_inside_triangle(const v2d&, const v2d&, const v2d&, const v2d&);

template void calculate_barycentric_coefficients(const v3f&, const v3f&, const v3f&, const v3f&, f32*, f32*, f32*);
template void calculate_barycentric_coefficients(const v3d&, const v3d&, const v3d&, const v3d&, f64*, f64*, f64*);
