#pragma once

template<typename T>
struct v2 {
    T x, y;

    v2() : x(0), y(0) {};
    v2(T v) : x(v), y(v) {};
    v2(T x, T y) : x(x), y(y) {};
};

typedef v2<float> v2f;
typedef v2<double> v2d;
typedef v2<signed int> v2i;



/* -------------------------------------------- V2 Unary Operators -------------------------------------------- */

template<typename T>
v2<T> operator-(v2<T> const &v) { return v2<T>(-v.x, -v.y); }

template<typename T>
v2<T> operator+(v2<T> const &v) { return v; }



/* ------------------------------------------- V2 Binary operators ------------------------------------------- */

template<typename T>
v2<T> operator+(v2<T> const &lhs, v2<T> const &rhs) { return v2<T>(lhs.x + rhs.x, lhs.y + rhs.y); }

template<typename T>
v2<T> operator-(v2<T> const &lhs, v2<T> const &rhs) { return v2<T>(lhs.x - rhs.x, lhs.y - rhs.y); }

template<typename T>
v2<T> operator*(v2<T> const &lhs, v2<T> const &rhs) { return v2<T>(lhs.x * rhs.x, lhs.y * rhs.y); }

template<typename T>
v2<T> operator/(v2<T> const &lhs, v2<T> const &rhs) { return v2<T>(lhs.x / rhs.x, lhs.y / rhs.y); }


template<typename T>
v2<T> operator+(v2<T> const &lhs, T const &rhs) { return v2<T>(lhs.x + rhs, lhs.y + rhs); }

template<typename T>
v2<T> operator-(v2<T> const &lhs, T const &rhs) { return v2<T>(lhs.x - rhs, lhs.y - rhs); }

template<typename T>
v2<T> operator*(v2<T> const &lhs, T const &rhs) { return v2<T>(lhs.x * rhs, lhs.y * rhs); }

template<typename T>
v2<T> operator/(v2<T> const &lhs, T const &rhs) { return v2<T>(lhs.x / rhs, lhs.y / rhs); }


template<typename T>
v2<T> operator+(T const &lhs, v2<T> const &rhs) { return v2<T>(lhs + rhs.x, lhs + rhs.y); }

template<typename T>
v2<T> operator-(T const &lhs, v2<T> const &rhs) { return v2<T>(lhs - rhs.x, lhs - rhs.y); }

template<typename T>
v2<T> operator*(T const &lhs, v2<T> const &rhs) { return v2<T>(lhs * rhs.x, lhs * rhs.y); }

template<typename T>
v2<T> operator/(T const &lhs, v2<T> const &rhs) { return v2<T>(lhs / rhs.x, lhs / rhs.y); }



/* ------------------------------------------------ V2 Algebra ------------------------------------------------ */

template<typename T>
T v2_dot_v2(v2<T> const &lhs, v2<T> const &rhs) { return lhs.x * rhs.x + lhs.y * rhs.y; }

template<typename T>
T v2_length(v2<T> const &v) { return static_cast<T>(sqrt(v.x * v.x + v.y * v.y)); }

template<typename T>
T v2_length2(v2<T> const &v) { return v.x * v.x + v.y * v.y; }

template<typename T>
v2<T> v2_normalize(v2<T> const &v) { T denom = 1 / v2_length(v); return v2<T>(v.x * denom, v2.y * denom); }
