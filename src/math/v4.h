#pragma once

template<typename T>
struct v4 {
	T x, y, z, w;

	v4<T>() : x(0), y(0), z(0), w(0) {};
	v4<T>(T v) : x(v), y(v), z(v), w(v) {};
	v4<T>(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {};
};

typedef v4<float> v4f;
typedef v4<double> v4d;
typedef v4<signed long> v4i;



/* -------------------------------------------- V4 Unary Operators -------------------------------------------- */

template<typename T>
v4<T> operator-(v4<T> const &v) { return v4<T>(-v.x, -v.y, -v.z, -v.w); }

template<typename T>
v4<T> operator+(v4<T> const &v) { return v; }



/* ------------------------------------------- V4 Binary Operators ------------------------------------------- */

template<typename T>
v4<T> operator+(v4<T> const &lhs, v4<T> const &rhs) { return v4<T>(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w); }

template<typename T>
v4<T> operator-(v4<T> const &lhs, v4<T> const &rhs) { return v4<T>(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w); }

template<typename T>
v4<T> operator*(v4<T> const &lhs, v4<T> const &rhs) { return v4<T>(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w); }

template<typename T>
v4<T> operator/(v4<T> const &lhs, v4<T> const &rhs) { return v4<T>(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w); }


template<typename T>
v4<T> operator+(v4<T> const &lhs, T const &rhs) { return v4<T>(lhs.x + rhs, lhs.y + rhs, lhs.z + rhs, lhs.w + rhs); }

template<typename T>
v4<T> operator-(v4<T> const &lhs, T const &rhs) { return v4<T>(lhs.x - rhs, lhs.y - rhs, lhs.z - rhs, lhs.w - rhs); }

template<typename T>
v4<T> operator*(v4<T> const &lhs, T const &rhs) { return v4<T>(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs); }

template<typename T>
v4<T> operator/(v4<T> const &lhs, T const &rhs) { return v4<T>(lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs); }


template<typename T>
v4<T> operator+(T const &lhs, v4<T> const &rhs) { return v4<T>(lhs + rhs.x, lhs + rhs.y, lhs + rhs.z, lhs + rhs.w); }

template<typename T>
v4<T> operator-(T const &lhs, v4<T> const &rhs) { return v4<T>(lhs - rhs.x, lhs - rhs.y, lhs - rhs.z, lhs - rhs.w); }

template<typename T>
v4<T> operator*(T const &lhs, v4<T> const &rhs) { return v4<T>(lhs * rhs.x, lhs * rhs.y, lhs * rhs.z, lhs * rhs.w); }

template<typename T>
v4<T> operator/(T const &lhs, v4<T> const &rhs) { return v4<T>(lhs / rhs.x, lhs / rhs.y, lhs / rhs.z, lhs / rhs.w); }



/* ------------------------------------------------ V4 Algebra ------------------------------------------------ */

template<typename T>
T v4_dot_v4(v4<T> const &lhs, v4<T> const &rhs) { return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w; }

template<typename T>
T v4_length(v4<T> const &v) { return static_cast<T>(sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w)); }

template<typename T>
T v4_length2(v4<T> const &v) { return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w; }

template<typename T>
v4<T> v4_normalize(v4<T> const &v) { T denom = 1 / v4_length(v); return v4<T>(v.x * denom, v.y * denom, v.z * denom, v.w * denom); }
