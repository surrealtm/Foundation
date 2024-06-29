#pragma once

#include "foundation.h"

// https://github.com/SRombauts/SimplexNoise/blob/master/src/SimplexNoise.cpp
struct Simplex_Noise_2D {
    u32 seed    = 843728974;
    u32 octaves = 4;
    
    f64 scale       = 1.0;
    f64 frequency   = 0.18;
    f64 amplitude   = 0.5;
    f64 lacunarity  = 2.0;
    f64 persistance = 0.5;

    u8 hash(u8 i);
    u8 hash(s32 x, s32 y);
    s32 floor(f64 t);
    f64 grad(u32 hash, f64 x, f64 y);
    f64 contrib(s32 gi, f64 x, f64 y);

    f64 noise(f64 x, f64 y);
    f64 fractal_noise(f64 x, f64 y);
};
