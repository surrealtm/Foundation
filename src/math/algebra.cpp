#include "algebra.h"
#include "v4.h"

//
// OpenGL uses NDC Depth Range [-1;1], whereas D3D11 uses Depth Range [0;1].
// This means that the projection matrices need to differ to manage depth values properly.
// Both use a Right Handed Coordinate System.
//
#define ALGEBRA_OPENGL false
#define ALGEBRA_D3D11  true

m4f make_orthographic_projection_matrix(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far) {
#if ALGEBRA_OPENGL
    m4f result = m4f(1);
    result[0][0] =  2.f / (right - left);
    result[1][1] =  2.f / (top   - bottom);
    result[2][2] = -2.f / (far   - near);
    result[3][0] = -(right + left)   / (right - left);
    result[3][1] = -(top   + bottom) / (top   - bottom);
    result[3][2] = -(far   + near)   / (far   - near);
    return result;
#elif ALGEBRA_D3D11
    m4f result = m4f(1);
    result[0][0] =  2.f / (right - left);
    result[1][1] =  2.f / (top - bottom);
    result[2][2] = -1.f / (far - near);
    result[3][0] = -(right + left)   / (right - left);
    result[3][1] = -(top   + bottom) / (top - bottom);
    result[3][2] = -(near)           / (far - near);
    return result;
#endif
}

m4f make_orthographic_projection_matrix(f32 width, f32 height, f32 near, f32 far) {
    return make_orthographic_projection_matrix(-width / 2.f, width / 2.f, -height / 2.f, height / 2.f, near, far);
}

m4f make_orthographic_projection_matrix(f32 width, f32 height, f32 length) {
#if ALGEBRA_OPENGL
    m4f result = m4f(1);
    result[0][0] =  2.f / (right - left);
    result[1][1] =  2.f / (top   - bottom);
    result[2][2] = -2.f / (far   - near);
    return result;
#elif ALGEBRA_D3D11
    m4f result = m4f(1);
    result[0][0] =  2.f / (width);
    result[1][1] =  2.f / (height);
    result[2][2] = -1.f / (length);
    return result;
#endif
}
    
m4f make_perspective_projection_matrix_vertical_fov(f32 fov, f32 ratio, f32 near, f32 far) {
#if ALGEBRA_OPENGL
    f32 tangent = tanf(fov / 2.f * 3.14159f / 180.f);
    
    m4f result = m4f(0);
    result[0][0] = 1.f / (ratio * tangent);
    result[1][1] = 1.f / tangent;
    result[2][2] = -(far + near) / (far - near);
    result[2][3] = -1.f;
    result[3][2] = -(2.f * near * far) / (far - near);
    return result;
#elif ALGEBRA_D3D11
    f32 tangent = tanf(fov / 2.f * 3.14159f / 180.f);
    
    m4f result = m4f(0);
    result[0][0] = 1.f / (ratio * tangent);
    result[1][1] = 1.f / tangent;
    result[2][2] = -far / (far - near);
    result[2][3] = -1.f;
    result[3][2] = -(near * far) / (far - near);
    return result;    
#endif
}

m4f make_perspective_projection_matrix_horizontal_fov(f32 fov, f32 ratio, f32 near, f32 far) {
    f32 vertical_fov = 2.f * atanf(tanf(fov / 2.f * 3.14159f / 180.f) / ratio) / 3.14159f * 180.f;
    return make_perspective_projection_matrix_vertical_fov(vertical_fov, ratio, near, far);
}

m4f make_view_matrix(const v3f &position, const v3f &rotation) {
    m4f result = m4f(1);
    result = m4_rotate_XYZ(result, rotation.x, rotation.y, rotation.z);
    result = m4_translate(result, -position.x, -position.y, -position.z);
    return result;
}

m4f make_lookat_matrix(const v3f &eye, const v3f &center) {
    v3f forward = v3_normalize(eye - center), up, right;
    make_orthonormal_basis(forward, &up, &right);
    
    m4f result;
    result[0][0] = right.x;
    result[1][0] = right.y;
    result[2][0] = right.z;
    result[3][0] = -v3_dot_v3(right, eye);

    result[0][1] = up.x;
    result[1][1] = up.y;
    result[2][1] = up.z;
    result[3][1] = -v3_dot_v3(up, eye);

    result[0][2] = forward.x;
    result[1][2] = forward.y;
    result[2][2] = forward.z;
    result[3][2] = -v3_dot_v3(forward, eye);

    result[0][3] = 0;
    result[1][3] = 0;
    result[2][3] = 0;
    result[3][3] = 1;

    return result;
}

m4f make_transformation_matrix(const v3f &position, const v3f &rotation, const v3f &scale) {
    m4f result = m4f(1);
    result = m4_translate(result, position.x, position.y, position.z);
    result = m4_rotate_XYZ(result, rotation.x, rotation.y, rotation.z);
    result = m4_scale(result, scale.x, scale.y, scale.z);
    return result;
}

m4f make_transformation_matrix(const v3f &position, const qtf &rotation, const v3f &scale) {
    m4f translated = m4_translate(m4f(1), position.x, position.y, position.z);
    m4f rotated    = make_rotation_matrix_from_quat(rotation);
    m4f scaled     = m4_scale(m4f(1), scale.x, scale.y, scale.z);
    return (translated * rotated) * scaled;
}

m4f make_rotation_matrix_from_quat(const qtf &rotation) {
    f32 qxx = rotation.x * rotation.x;
    f32 qxy = rotation.x * rotation.y;
    f32 qxz = rotation.x * rotation.z;
    f32 qxw = rotation.x * rotation.w;
    f32 qyy = rotation.y * rotation.y;
    f32 qyz = rotation.y * rotation.z;
    f32 qyw = rotation.y * rotation.w;
    f32 qzz = rotation.z * rotation.z;
    f32 qzw = rotation.z * rotation.w;
    
    m4f result = m4f(1);
    result[0][0] = 1 - 2 * (qyy + qzz);
    result[0][1] =     2 * (qxy + qzw);
    result[0][2] =     2 * (qxz - qyw);
    
    result[1][0] =     2 * (qxy - qzw);
    result[1][1] = 1 - 2 * (qxx + qzz);
    result[1][2] =     2 * (qyz + qxw);
    
    result[2][0] =     2 * (qxz + qyw);
    result[2][1] =     2 * (qyz - qxw);
    result[2][2] = 1 - 2 * (qxx + qyy);
    return result;
}

v3f world_space_to_screen_space(const m4f &projection, const m4f &view, const v3f &position, const v2f &screen_size) {
    v4f eye  = view * v4f(position.x, position.y, position.z, 1);
    v4f clip = projection * eye;
    clip.x /= clip.w;
    clip.y /= clip.w;
    clip.z /= clip.w;

    return v3f((clip.x * 0.5f + 0.5f) * screen_size.x, (0.5f - clip.y * 0.5f) * screen_size.y, clip.z);
}

void make_orthonormal_basis(const v3f &forward, v3f *up, v3f *right) {
    v3f default_up = v3f(0, 1, 0);
    if(v3_dot_v3(forward, default_up) >= 1 - F32_EPSILON || v3_dot_v3(forward, default_up) <= -1 + F32_EPSILON) {
        default_up = v3f(forward.z, forward.x, forward.y);
    }
    
    *right = v3_normalize(v3_cross_v3(default_up, forward));
    *up    = v3_cross_v3(forward, *right);
}
