#pragma once

template<typename T>
struct v3 {
    union {
        struct { T x, y, z; };
        T values[3];
    };

    v3() : x(0), y(0), z(0) {};
    v3(T v) : x(v), y(v), z(v) {};
    v3(T x, T y, T z) : x(x), y(y), z(z) {};
};

typedef v3<float> v3f;
typedef v3<double> v3d;
typedef v3<signed long> v3i;



/* -------------------------------------------- V3 Unary Operators -------------------------------------------- */

template<typename T>
v3<T> operator-(v3<T> const &v) { return v3<T>(-v.x, -v.y, -v.z); }

template<typename T>
v3<T> operator+(v3<T> const &v) { return v; }



/* ------------------------------------------- V3 Binary Operators ------------------------------------------- */

template<typename T>
v3<T> operator+(v3<T> const &lhs, v3<T> const &rhs) { return v3<T>(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z); }

template<typename T>
v3<T> operator-(v3<T> const &lhs, v3<T> const &rhs) { return v3<T>(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z); }

template<typename T>
v3<T> operator*(v3<T> const &lhs, v3<T> const &rhs) { return v3<T>(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z); }

template<typename T>
v3<T> operator/(v3<T> const &lhs, v3<T> const &rhs) { return v3<T>(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z); }


template<typename T>
v3<T> operator+(v3<T> const &lhs, T const &rhs) { return v3<T>(lhs.x + rhs, lhs.y + rhs, lhs.z + rhs); }

template<typename T>
v3<T> operator-(v3<T> const &lhs, T const &rhs) { return v3<T>(lhs.x - rhs, lhs.y - rhs, lhs.z - rhs); }

template<typename T>
v3<T> operator*(v3<T> const &lhs, T const &rhs) { return v3<T>(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs); }

template<typename T>
v3<T> operator/(v3<T> const &lhs, T const &rhs) { return v3<T>(lhs.x / rhs, lhs.y / rhs, lhs.z / rhs); }


template<typename T>
v3<T> operator+(T const &lhs, v3<T> const &rhs) { return v3<T>(lhs + rhs.x, lhs + rhs.y, lhs + rhs.z); }

template<typename T>
v3<T> operator-(T const &lhs, v3<T> const &rhs) { return v3<T>(lhs - rhs.x, lhs - rhs.y, lhs - rhs.z); }

template<typename T>
v3<T> operator*(T const &lhs, v3<T> const &rhs) { return v3<T>(lhs * rhs.x, lhs * rhs.y, lhs * rhs.z); }

template<typename T>
v3<T> operator/(T const &lhs, v3<T> const &rhs) { return v3<T>(lhs / rhs.x, lhs / rhs.y, lhs / rhs.z); }



/* ------------------------------------------------ V3 Algebra ------------------------------------------------ */

template<typename T>
T v3_dot_v3(v3<T> const &lhs, v3<T> const &rhs) { return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z; }

template<typename T>
T v3_length(v3<T> const &v) { return static_cast<T>(sqrt(v.x * v.x + v.y * v.y + v.z * v.z)); }

template<typename T>
T v3_length2(v3<T> const &v) { return v.x * v.x + v.y * v.y + v.z * v.z; }

template<typename T>
v3<T> v3_normalize(v3<T> const &v) { T denom = 1 / v3_length(v); return v3<T>(v.x * denom, v.y * denom, v.z * denom); }

template<typename T>
v3<T> v3_cross_v3(v3<T> const &lhs, v3<T> const &rhs) { return v3<T>(lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x); }

template<typename T>
v3<T> v3_lerp(v3<T> const &lhs, v3<T> const &rhs, T t) { 
    T one_minus_t = 1 - t;
    return v3<T>(one_minus_t * lhs.x + t * rhs.x,
                 one_minus_t * lhs.y + t * rhs.y,
                 one_minus_t * lhs.z + t * rhs.z);
}

template<typename T>
v3<T> v3_reflect(v3<T> const &direction, v3<T> const &normal) {
    return v3_normalize(direction - 2 * v3_dot_v3(direction, normal) * normal);
}
