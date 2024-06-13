#pragma once

#include "../foundation.h"
#include "v2.h"
#include "v3.h"
#include "qt.h"
#include "m4.h"

m4f make_orthographic_projection_matrix(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far);
m4f make_orthographic_projection_matrix(f32 width, f32 height, f32 near, f32 far);
m4f make_orthographic_projection_matrix(f32 width, f32 height, f32 length);
m4f make_perspective_projection_matrix_vertical_fov(f32 fov, f32 ratio, f32 near, f32 far);
m4f make_perspective_projection_matrix_horizontal_fov(f32 fov, f32 ratio, f32 near, f32 far);
m4f make_view_matrix(const v3f &position, const v3f &rotation);
m4f make_lookat_matrix(const v3f &eye, const v3f &center);
m4f make_transformation_matrix(const v3f &position, const v3f &rotation, const v3f &scale);
m4f make_transformation_matrix(const v3f &position, const qtf &rotation, const v3f &scale);
m4f make_rotation_matrix_from_quat(const qtf &rotation);
v3f world_space_to_screen_space(const m4f &projection, const m4f &view, const v3f &position, const v2f &screen_size);
void make_orthonormal_basis(const v3f &forward, v3f *up, v3f *right);
