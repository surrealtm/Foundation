#pragma once

#define PI 3.14159265359
#define FPI 3.14159265359f

#define TAU 6.283185307179586
#define FTAU 6.283185307179586f

template<typename T>
T turns_to_radians(T turns) { return turns * static_cast<T>(TAU); }

template<typename T>
T radians_to_turns(T radians) { return radians / static_cast<T>(TAU); }

#include "v2.h"
#include "v3.h"
#include "v4.h"
#include "qt.h"

#include "intersect.h"
