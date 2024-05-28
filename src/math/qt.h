#pragma once

template<typename T>
struct qt {
    T x, y, z, w;

    qt() : x(0), y(0), z(0), w(1) {};
    qt(T v) : x(v), y(v), z(v), w(v) {};
    qt(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {};
};

typedef qt<float> qtf;
typedef qt<double> qtd;
typedef qt<signed int> qti;



/* -------------------------------------------- QT Unary Operators -------------------------------------------- */

template<typename T>
qt<T> operator-(qt<T> const &q) { return qt<T>(-q.x, -q.y, -q.z, -q.w); }

template<typename T>
qt<T> operator+(qt<T> const &q) { return q; }



/* ------------------------------------------- QT Binary Operators ------------------------------------------- */

template<typename T>
qt<T> operator+(qt<T> const &lhs, qt<T> const &rhs) { return qt<T>(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w); }

template<typename T>
qt<T> operator-(qt<T> const &lhs, qt<T> const &rhs) { return qt<T>(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w); }

template<typename T>
qt<T> operator*(qt<T> const &lhs, qt<T> const &rhs) { return qt<T>(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w); }

template<typename T>
qt<T> operator/(qt<T> const &lhs, qt<T> const &rhs) { return qt<T>(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w); }


template<typename T>
qt<T> operator*(qt<T> const &lhs, T const &rhs) { return qt<T>(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs); }

template<typename T>
qt<T> operator/(qt<T> const &lhs, T const &rhs) { return qt<T>(lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs); }



/* ------------------------------------------------ QT Algebra ------------------------------------------------ */

template<typename T>
T qt_dot(qt<T> const &lhs, qt<T> const &rhs) { return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w; }

template<typename T>
qt<T> qt_length(qt<T> const &q) { return static_cast<T>(sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w)); }

template<typename T>
qt<T> qt_length2(qt<T> const &q) { return q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w; }

template<typename T>
qt<T> qt_normalize(qt<T> const &q) { T denom = 1 / qt_length(q); return qt<T>(q.x * denom, q.y * denom, q.z * denom, q.w * denom); }



/* ----------------------------------------------- QT Rotation ----------------------------------------------- */

template<typename T>
static inline
T qt_mix(T const &lhs, T const &rhs, T const &t) {
    return lhs * t + rhs * (static_cast<T>(1) - t);
}

template<typename T>
qt<T> qt_conjugate(qt<T> const &q) { return qt<T>(-q.x, -q.y, -q.z, q.w); }

template<typename T>
qt<T> qt_inverse(qt<T> const &q) { T denom = 1 / qt_length2(q); return qt<T>(-q.x * denom, -q.y * denom, -q.z * denom, q.w * denom); }

template<typename T>
v3<T> qt_rotate(qt<T> const &q, v3<T> const &v) {
    T w_squared = q.w * q.w;
    T u_dot_u = q.x * q.x + q.y * q.y + q.z * q.z;
    T u_dot_v = q.x * v.x + q.y * v.y + q.z * v.z;

    v3<T> u_cross_v = v3<T>(q.y * v.z - q.z * v.y,
                            q.z * v.x - q.x * v.z,
                            q.x * v.y - q.y * v.x);

    v3<T> rotated = v3<T>(2 * u_dot_v * q.x + (w_squared - u_dot_u) * v.x + 2 * q.w * u_cross_v.x,
                          2 * u_dot_v * q.y + (w_squared - u_dot_u) * v.y + 2 * q.w * u_cross_v.y,
                          2 * u_dot_v * q.z + (w_squared - u_dot_u) * v.z + 2 * q.w * u_cross_v.z);

    return rotated;
}

template<typename T>
qt<T> qt_slerp(qt<T> const &lhs, qt<T> const &rhs, T t) {
    T theta = qt_dot(lhs, rhs);
    qt<T> _rhs;

    if(theta < static_cast<T>(0)) {
        theta = -theta;
        _rhs  = -rhs;
    } else {
        _rhs = rhs;
    }

    qt<T> result;
    if(theta < static_cast<T>(1)) {
        T angle = static_cast<T>(acos(theta));
        result = (lhs * static_cast<T>(sin((1 - t) * angle)) + _rhs * static_cast<T>(sin(t * angle))) / static_cast<T>(sin(angle));
    } else {
        result = qt<T>(qt_mix(lhs.x, _rhs.x, t), qt_mix(lhs.y, _rhs.y, t), qt_mix(lhs.z, _rhs.z, t), qt_mix(lhs.w, _rhs.w, t));        
    }

    return result;
}

template<typename T>
qt<T> qt_slerp_long(qt<T> const &lhs, qt<T> const &rhs, T t) {
    T theta = qt_dot(lhs, rhs);
    qt<T> _rhs;

    if(theta > static_cast<T>(0)) {
        theta = -theta;
        _rhs  = -rhs;
    } else {
        _rhs = rhs;
    }

    qt<T> result;
    if(theta < static_cast<T>(1)) {
        T angle = static_cast<T>(acos(theta));
        result = lhs * static_cast<T>(sin((1 - t) * angle)) + _rhs * static_cast<T>(sin(t * angle));
    } else {
        result = qt<T>(qt_mix(lhs.x, _rhs.x, t), qt_mix(lhs.y, _rhs.y, t), qt_mix(lhs.z, _rhs.z, t), qt_mix(lhs.w, _rhs.w, t));        
    }

    return result;
}



/* ---------------------------------------------- QT Conversion ---------------------------------------------- */

template<typename T>
qt<T> qt_from_angle_axis(v3<T> const &axis, T angle) {
    T half_angle_radians = turns_to_radians(angle / 2);
    T theta = static_cast<T>(sin(half_angle_radians));
    qt<T> result = qt<T>(theta * axis.x, theta * axis.y, theta * axis.z, static_cast<T>(cos(half_angle_radians)));
    return qt_normalize(result);
}

template<typename T>
qt<T> qt_from_euler_turns(v3<T> const &euler_turns) {
    T rx = (euler_turns.x / 2) * static_cast<T>(6.283185307179586); // Turns to radians
    T ry = (euler_turns.y / 2) * static_cast<T>(6.283185307179586); // Turns to radians
    T rz = (euler_turns.z / 2) * static_cast<T>(6.283185307179586); // Turns to radians

    T cx = static_cast<T>(cos(rx));
    T cy = static_cast<T>(cos(ry));
    T cz = static_cast<T>(cos(rz));

    T sx = static_cast<T>(sin(rx));
    T sy = static_cast<T>(sin(ry));
    T sz = static_cast<T>(sin(rz));

    qt<T> result = qt<T>(sx * cy * cz - cx * sy * sz,
                         cx * sy * cz + sx * cy * sz,
                         cx * cy * sz - sx * sy * cz,
                         cx * cy * cz - sx * sy * sz);

    return result;
}
