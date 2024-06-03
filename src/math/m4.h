#pragma once

template<typename T>
struct v3;

template<typename T>
struct v4;

template<typename T>
struct m4 {
    struct column {
        T _[4];

        column() {};
        column(T x, T y, T z, T w) : _{ x, y, z, w } {};
        
        column operator+(const column &other) const { return column(_[0] + other._[0], _[1] + other._[1], _[2] + other._[2], _[3] + other._[3]); }
        column operator-(const column &other) const { return column(_[0] - other._[0], _[1] - other._[1], _[2] - other._[2], _[3] - other._[3]); }
        column operator*(const column &other) const { return column(_[0] * other._[0], _[1] * other._[1], _[2] * other._[2], _[3] * other._[3]); }

        column operator*(T v) const { return column(_[0] * v, _[1] * v, _[2] * v, _[3] * v); }
        
        const T operator[](s64 index) const { return _[index]; }
        T &operator[](s64 index) { return _[index]; }
    };

    column _[4];

    m4() : _{ { 0, 0, 0, 0 },
              { 0, 0, 0, 0 },
              { 0, 0, 0, 0 },
              { 0, 0, 0, 0 } } {};

    m4(T v) : _{ { v, 0, 0, 0 },
                 { 0, v, 0, 0 },
                 { 0, 0, v, 0 },
                 { 0, 0, 0, v } } {};

    m4(const column &c0, const column &c1, const column &c2, const column &c3) :
        _{ c0, c1, c2, c3 } {};
    
    m4(T m00, T m01, T m02, T m03,
       T m10, T m11, T m12, T m13,
       T m20, T m21, T m22, T m23,
       T m30, T m31, T m32, T m33) : _{ { m00, m01, m02, m03 },
                                        { m10, m11, m12, m13 },
                                        { m20, m21, m22, m23 },
                                        { m30, m31, m32, m33 } } {};    

    const column operator[](s64 index) const { return _[index]; }
    column &operator[](s64 index) { return _[index]; }
};

typedef m4<float> m4f;
typedef m4<double> m4d;
typedef m4<signed long> m4i;



/* ------------------------------------------- M4 Binary Operators ------------------------------------------- */

template<typename T>
m4<T> operator*(const m4<T> &lhs, const m4<T> &rhs) {
    m4<T> result;

    result[0][0] = lhs[0][0] * rhs[0][0] + lhs[1][0] * rhs[0][1] + lhs[2][0] * rhs[0][2] + lhs[3][0] * rhs[0][3];
    result[0][1] = lhs[0][1] * rhs[0][0] + lhs[1][1] * rhs[0][1] + lhs[2][1] * rhs[0][2] + lhs[3][1] * rhs[0][3];
    result[0][2] = lhs[0][2] * rhs[0][0] + lhs[1][2] * rhs[0][1] + lhs[2][2] * rhs[0][2] + lhs[3][2] * rhs[0][3];
    result[0][3] = lhs[0][3] * rhs[0][0] + lhs[1][3] * rhs[0][1] + lhs[2][3] * rhs[0][2] + lhs[3][3] * rhs[0][3];

    result[1][0] = lhs[0][0] * rhs[1][0] + lhs[1][0] * rhs[1][1] + lhs[2][0] * rhs[1][2] + lhs[3][0] * rhs[1][3];
    result[1][1] = lhs[0][1] * rhs[1][0] + lhs[1][1] * rhs[1][1] + lhs[2][1] * rhs[1][2] + lhs[3][1] * rhs[1][3];
    result[1][2] = lhs[0][2] * rhs[1][0] + lhs[1][2] * rhs[1][1] + lhs[2][2] * rhs[1][2] + lhs[3][2] * rhs[1][3];
    result[1][3] = lhs[0][3] * rhs[1][0] + lhs[1][3] * rhs[1][1] + lhs[2][3] * rhs[1][2] + lhs[3][3] * rhs[1][3];

    result[2][0] = lhs[0][0] * rhs[2][0] + lhs[1][0] * rhs[2][1] + lhs[2][0] * rhs[2][2] + lhs[3][0] * rhs[2][3];
    result[2][1] = lhs[0][1] * rhs[2][0] + lhs[1][1] * rhs[2][1] + lhs[2][1] * rhs[2][2] + lhs[3][1] * rhs[2][3];
    result[2][2] = lhs[0][2] * rhs[2][0] + lhs[1][2] * rhs[2][1] + lhs[2][2] * rhs[2][2] + lhs[3][2] * rhs[2][3];
    result[2][3] = lhs[0][3] * rhs[2][0] + lhs[1][3] * rhs[2][1] + lhs[2][3] * rhs[2][2] + lhs[3][3] * rhs[2][3];

    result[3][0] = lhs[0][0] * rhs[3][0] + lhs[1][0] * rhs[3][1] + lhs[2][0] * rhs[3][2] + lhs[3][0] * rhs[3][3];
    result[3][1] = lhs[0][1] * rhs[3][0] + lhs[1][1] * rhs[3][1] + lhs[2][1] * rhs[3][2] + lhs[3][1] * rhs[3][3];
    result[3][2] = lhs[0][2] * rhs[3][0] + lhs[1][2] * rhs[3][1] + lhs[2][2] * rhs[3][2] + lhs[3][2] * rhs[3][3];
    result[3][3] = lhs[0][3] * rhs[3][0] + lhs[1][3] * rhs[3][1] + lhs[2][3] * rhs[3][2] + lhs[3][3] * rhs[3][3];
    
    return result;
}

template<typename T>
m4<T> operator*(const m4<T> &lhs, const T &rhs) {
    m4<T> result;

    for(s64 i = 0; i < 4; ++i) {
        for(s64 j = 0; j < 4; ++j) {
            result[i][j] = lhs[i][j] * rhs;
        }
    }
    
    return result;
}

template<typename T>
v3<T> operator*(const m4<T> &lhs, const v3<T> &rhs) {
    v3<T> result;
    result.x = lhs[0][0] * rhs.x + lhs[1][0] * rhs.y + lhs[2][0] * rhs.z + lhs[3][0] * 1;
    result.y = lhs[0][1] * rhs.x + lhs[1][1] * rhs.y + lhs[2][1] * rhs.z + lhs[3][1] * 1;
    result.z = lhs[0][2] * rhs.x + lhs[1][2] * rhs.y + lhs[2][2] * rhs.z + lhs[3][2] * 1;
    return result;
}

template<typename T>
v4<T> operator*(const m4<T> &lhs, const v4<T> &rhs) {
    v4<T> result;
    result.x = lhs[0][0] * rhs.x + lhs[1][0] * rhs.y + lhs[2][0] * rhs.z + lhs[3][0] * rhs.w;
    result.y = lhs[0][1] * rhs.x + lhs[1][1] * rhs.y + lhs[2][1] * rhs.z + lhs[3][1] * rhs.w;
    result.z = lhs[0][2] * rhs.x + lhs[1][2] * rhs.y + lhs[2][2] * rhs.z + lhs[3][2] * rhs.w;
    result.w = lhs[0][3] * rhs.x + lhs[1][3] * rhs.y + lhs[2][3] * rhs.z + lhs[3][3] * rhs.w;
    return result;
}



/* ------------------------------------------------ M4 Algebra ------------------------------------------------ */

template<typename T>
m4<T> m4_inverse(const m4<T> &input) {
    T coef00 = input[2][2] * input[3][3] - input[3][2] * input[2][3];
    T coef02 = input[1][2] * input[3][3] - input[3][2] * input[1][3];
    T coef03 = input[1][2] * input[2][3] - input[2][2] * input[1][3];

    T coef04 = input[2][1] * input[3][3] - input[3][1] * input[2][3];
    T coef06 = input[1][1] * input[3][3] - input[3][1] * input[1][3];
    T coef07 = input[1][1] * input[2][3] - input[2][1] * input[1][3];

    T coef08 = input[2][1] * input[3][2] - input[3][1] * input[2][2];
    T coef10 = input[1][1] * input[3][2] - input[3][1] * input[1][2];
    T coef11 = input[1][1] * input[2][2] - input[2][1] * input[1][2];

    T coef12 = input[2][0] * input[3][3] - input[3][0] * input[2][3];
    T coef14 = input[1][0] * input[3][3] - input[3][0] * input[1][3];
    T coef15 = input[1][0] * input[2][3] - input[2][0] * input[1][3];

    T coef16 = input[2][0] * input[3][2] - input[3][0] * input[2][2];
    T coef18 = input[1][0] * input[3][2] - input[3][0] * input[1][2];
    T coef19 = input[1][0] * input[2][2] - input[2][0] * input[1][2];

    T coef20 = input[2][0] * input[3][1] - input[3][0] * input[2][1];
    T coef22 = input[1][0] * input[3][1] - input[3][0] * input[1][1];
    T coef23 = input[1][0] * input[2][1] - input[2][0] * input[1][1];

    typename m4<T>::column fac0 = m4<T>::column(coef00, coef00, coef02, coef03);
    typename m4<T>::column fac1 = m4<T>::column(coef04, coef04, coef06, coef07);
    typename m4<T>::column fac2 = m4<T>::column(coef08, coef08, coef10, coef11);
    typename m4<T>::column fac3 = m4<T>::column(coef12, coef12, coef14, coef15);
    typename m4<T>::column fac4 = m4<T>::column(coef16, coef16, coef18, coef19);
    typename m4<T>::column fac5 = m4<T>::column(coef20, coef20, coef22, coef23);
    
    typename m4<T>::column vec0 = m4<T>::column(input[1][0], input[0][0], input[0][0], input[0][0]);
    typename m4<T>::column vec1 = m4<T>::column(input[1][1], input[0][1], input[0][1], input[0][1]);
    typename m4<T>::column vec2 = m4<T>::column(input[1][2], input[0][2], input[0][2], input[0][2]);
    typename m4<T>::column vec3 = m4<T>::column(input[1][3], input[0][3], input[0][3], input[0][3]);

    typename m4<T>::column inv0 = (((vec1 * fac0) - (vec2 * fac1)) + (vec3 * fac2));
    typename m4<T>::column inv1 = (((vec0 * fac0) - (vec2 * fac3)) + (vec3 * fac4));
    typename m4<T>::column inv2 = (((vec0 * fac1) - (vec1 * fac3)) + (vec3 * fac5));
    typename m4<T>::column inv3 = (((vec0 * fac2) - (vec1 * fac4)) + (vec2 * fac5));

    m4<T> output = m4<T> {
            {  inv0[0], -inv0[1],  inv0[2], -inv0[3] },
            { -inv1[0],  inv1[1], -inv1[2],  inv1[3] },
            {  inv2[0], -inv2[1],  inv2[2], -inv2[3] },
            { -inv3[0],  inv3[1], -inv3[2],  inv3[3] },
        };

    typename m4<T>::column row0 = m4<T>::column(output[0][0], output[1][0], output[2][0], output[3][0]);
    typename m4<T>::column dot0 = m4<T>::column(input[0][0] * row0[0], input[0][1] * row0[1], input[0][2] * row0[2], input[0][3] * row0[3]);
    T dot1 = dot0[0] + dot0[1] + dot0[2] + dot0[3];
    
    T one_over_determinant = 1 / dot1;
    
    return output * one_over_determinant;   
}

template<typename T>
m4<T> m4_translate(const m4<T> &matrix, T x, T y, T z) {
    m4<T> result;
    result[0] = matrix[0];
    result[1] = matrix[1];
    result[2] = matrix[2];
    result[3][0] = matrix[0][0] * x + matrix[1][0] * y + matrix[2][0] * z + matrix[3][0];
    result[3][1] = matrix[0][1] * x + matrix[1][1] * y + matrix[2][1] * z + matrix[3][1];
    result[3][2] = matrix[0][2] * x + matrix[1][2] * y + matrix[2][2] * z + matrix[3][2];
    result[3][3] = matrix[0][3] * x + matrix[1][3] * y + matrix[2][3] * z + matrix[3][3];
    return result;
}

template<typename T>
m4<T> m4_scale(const m4<T> &matrix, T x, T y, T z) {
    m4<T> result;

    result[0] = matrix[0] * x;
    result[1] = matrix[1] * y;
    result[2] = matrix[2] * z;    
    result[3] = matrix[3];

    return result;
}

template<typename T>
m4<T> m4_rotate(const m4<T> &matrix, T angle, T x, T y, T z) {
    T radians = (angle * static_cast<T>(6.283185307179586)); // Turns to radians
    T _sin = static_cast<T>(sin(radians));
    T _cos = static_cast<T>(cos(radians));
    T _cos_inv = 1 - _cos;

    m4<T> result;

    result[0][0] = x * x * _cos_inv + _cos;
    result[0][1] = x * y * _cos_inv + z * _sin;
    result[0][2] = x * z * _cos_inv - y * _sin;
    result[0][3] = 0;

    result[1][0] = y * x * _cos_inv - z * _sin;
    result[1][1] = y * y * _cos_inv + _cos;
    result[1][2] = y * z * _cos_inv + x * _sin;
    result[1][3] = 0;

    result[2][0] = z * x * _cos_inv + y * _sin;
    result[2][1] = z * y * _cos_inv - x * _sin;
    result[2][2] = z * z * _cos_inv + _cos;
    result[2][3] = 0;

    result[3][0] = 0;
    result[3][1] = 0;
    result[3][2] = 0;
    result[3][3] = 1;
    
    return matrix * result;    
}

template<typename T>
m4<T> m4_rotate_XYZ(const m4<T> &matrix, T x, T y, T z) {
    m4<T> result;
    result = m4_rotate(matrix, x, static_cast<T>(1), static_cast<T>(0), static_cast<T>(0));
    result = m4_rotate(result, y, static_cast<T>(0), static_cast<T>(1), static_cast<T>(0));
    result = m4_rotate(result, z, static_cast<T>(0), static_cast<T>(0), static_cast<T>(1));
    return result;
}
