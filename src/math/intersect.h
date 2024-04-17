#pragma once
#include "foundation.h"
#include "v3.h"

b8 ray_plane_intersection(const v3<f32> &point, const v3<f32> &normal, const v3<f32> &origin, const v3<f32> &direction, f32 &distance);
b8 ray_double_sided_plane_intersection(const v3<f32> &point, const v3<f32> &normal, const v3<f32> &origin, const v3<f32> &direction, f32 &distance);
