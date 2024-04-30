#pragma once
#include "foundation.h"
#include "v2.h"
#include "v3.h"

f32 calculate_triangle_area(const v3<f32> &p0, const v3<f32> &p1, const v3<f32> &p2);

b8 ray_plane_intersection(const v3<f32> &ray_origin, const v3<f32> &ray_direction, const v3<f32> &plane_point, const v3<f32> &plane_normal, f32 *distance);
b8 ray_double_sided_plane_intersection(const v3<f32> &ray_origin, const v3<f32> &ray_direction, const v3<f32> &plane_point, const v3<f32> &plane_normal, f32 *distance);

f32 point_plane_distance_signed(const v3<f32> &point, const v3<f32> &plane_point, const v3<f32> &plane_normal);
v3<f32> project_point_onto_triangle(const v3<f32> &p, const v3<f32> &a, const v3<f32> &b, const v3<f32> &c);

struct Triangle_Intersection_Result {
	b8 intersection; // Whether any intersection actually occured. If not, all other members are undefined.
	f32 distance; // The unnormalized distance from the ray origin to the intersection point, measured in ray_direction units!
	f32 u, v; // The normalized barycentric coordinate factors of p1 and p2 respectively, normalized to [0:1].
};

Triangle_Intersection_Result ray_triangle_intersection(const v3<f32> &ray_origin, const v3<f32> &ray_direction, const v3<f32> &p0, const v3<f32> &p1, const v3<f32> &p2);
Triangle_Intersection_Result ray_double_sided_triangle_intersection(const v3<f32> &ray_origin, const v3<f32> &ray_direction, const v3<f32> &p0, const v3<f32> &p1, const v3<f32> &p2);

b8 ray_triangle_intersection(const v3<f32> &ray_origin, const v3<f32> &ray_direction, const v3<f32> &p0, const v3<f32> &p1, const v3<f32> &p2, f32 *distance);
b8 ray_double_sided_triangle_intersection(const v3<f32> &ray_origin, const v3<f32> &ray_direction, const v3<f32> &p0, const v3<f32> &p1, const v3<f32> &p2, f32 *distance);

b8 point_inside_triangle(const v2<f32> &point, const v2<f32> &p0, const v2<f32> &p1, const v2<f32> &p2);

void calculate_barycentric_coefficients(const v3<f32> &p0, const v3<f32> &p1, const v3<f32> &p2, const v3<f32> &point, f32 *u, f32 *v, f32 *w);
