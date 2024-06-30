#pragma once

#include "foundation.h"

struct Noise_Result_2D {
    f64 value;
    f64 dx;
    f64 dy;
};

struct Simplex_Noise_2D {
    u32 seed    = 843728974;
    u32 octaves = 4;
    
    f64 scale      = 1.0;
    f64 frequency  = 0.18;
    f64 amplitude  = 0.5;
    f64 lacunarity = 2.0;
    f64 gain       = 0.5;

    Noise_Result_2D noise(f64 x, f64 y);
    Noise_Result_2D fractal_noise(f64 x, f64 y);
};
