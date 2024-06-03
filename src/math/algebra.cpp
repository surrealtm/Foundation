#include "algebra.h"
#include "v4.h"

m4f make_orthographic_projection_matrix(float width, float height, float near, float far) {
    m4f result = m4f(1);
    result[0][0] =  1 / width;
    result[1][1] =  1 / height;
    result[2][2] = -1 / (far - near);
    result[3][2] = -(far + near) / (far - near);
    result[3][3] = 1;
    return result;
}

m4f make_perspective_projection_matrix(f32 fov, f32 ratio, f32 near, f32 far) {
    f32 frustum_length = far - near;
    f32 tan_half_angle = tanf(fov / 180.f * 3.14159f);

    m4f result = m4f(1);
    result[0][0] = 1 / (ratio * tan_half_angle);
    result[1][1] = 1 / tan_half_angle;
    result[2][2] = -(far + near) / frustum_length;
    result[2][3] = -1;
    result[3][2] = -(2 * near * far) / frustum_length;
    return result;
}

m4f make_view_matrix(const v3f &position, const v3f &rotation) {
    m4f result = m4f(1);
    result = m4_rotate_XYZ(result, rotation.x, rotation.y, rotation.z);
    result = m4_translate(result, -position.x, -position.y, -position.z);
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
