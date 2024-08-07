#pragma once

#define PI 3.14159265359
#define FPI 3.14159265359f

#define TAU 6.283185307179586
#define FTAU 6.283185307179586f

#define F32_EPSILON 1e-5f
#define F64_EPSILON 1e-8

template<typename T>
T turns_to_radians(T turns) { return turns * static_cast<T>(TAU); }

template<typename T>
T radians_to_turns(T radians) { return radians / static_cast<T>(TAU); }

template<typename T>
T turns_to_degrees(T turns) { return turns * static_cast<T>(360.0); }

template<typename T>
T degrees_to_turns(T degrees) { return degrees / static_cast<T>(360.0); }

template<typename T>
T degrees_to_radians(T degrees) { return degrees / static_cast<T>(180) * static_cast<T>(PI); }

template<typename T>
T radians_to_degrees(T radians) { return radians / static_cast<T>(PI) * static_cast<T>(180); }

static inline
b8 fuzzy_equals(f32 lhs, f32 rhs) { return (lhs - rhs) <= F32_EPSILON && (lhs - rhs) >= -F32_EPSILON; }

static inline
b8 fuzzy_equals(f64 lhs, f64 rhs) { return (lhs - rhs) <= F64_EPSILON && (lhs - rhs) >= -F64_EPSILON; }


#include "v2.h"
#include "v3.h"
#include "v4.h"
#include "qt.h"
#include "m4.h"

#include "intersect.h"
#include "algebra.h"


template<typename T>
static inline
b8 v2_fuzzy_equals(v2<T> const &lhs, v2<T> const &rhs) { 
    return fuzzy_equals(lhs.x, rhs.x) && fuzzy_equals(lhs.y, rhs.y);
}

template<typename T>
static inline
b8 v3_fuzzy_equals(v3<T> const &lhs, v3<T> const &rhs) { 
    return fuzzy_equals(lhs.x, rhs.x) && fuzzy_equals(lhs.y, rhs.y) && fuzzy_equals(lhs.z, rhs.z);
}

template<typename T>
static inline
b8 v4_fuzzy_equals(v4<T> const &lhs, v4<T> const &rhs) { 
    return fuzzy_equals(lhs.x, rhs.x) && fuzzy_equals(lhs.y, rhs.y) && fuzzy_equals(lhs.z, rhs.z) && fuzzy_equals(lhs.w, rhs.w);
}

template<typename T>
static inline
b8 qt_fuzzy_equals(qt<T> const &lhs, qt<T> const &rhs) { 
    return fuzzy_equals(lhs.x, rhs.x) && fuzzy_equals(lhs.y, rhs.y) && fuzzy_equals(lhs.z, rhs.z) && fuzzy_equals(lhs.w, rhs.w);
}
