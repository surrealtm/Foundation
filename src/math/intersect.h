#pragma once
#include "foundation.h"
#include "v3.h"

b8 ray_plane_intersection(const v3<f32> &ray_origin, const v3<f32> &ray_direction, const v3<f32> &plane_point, const v3<f32> &plane_normal, f32 *distance);
b8 ray_double_sided_plane_intersection(const v3<f32> &ray_origin, const v3<f32> &ray_direction, const v3<f32> &plane_point, const v3<f32> &plane_normal, f32 *distance);

f32 point_plane_distance_signed(const v3<f32> &point, const v3<f32> &plane_point, const v3<f32> &plane_normal);
