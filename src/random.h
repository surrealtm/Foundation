#pragma once

#include "foundation.h"

struct Random_Generator {
    u64 x, y, z, c;

    Random_Generator() { this->seed(); };

    void seed();
    void seed(u64 x);
    void seed(u64 x, u64 y, u64 z, u64 c);

    u64 random_u64();
    u64 random_u64(u64 low, u64 high);

    f32 random_f32_zero_to_one();
    f32 random_f32(f32 low, f32 high);
    f32 random_f32_normal_distribution(f32 mean, f32 stddev);
    f32 random_f32_linear_distribution(f32 low, f32 high);
    f32 random_f32_exponential_distribution(f32 lambda);
    f32 random_f32_inverse_distribution();
    
    f64 random_f64_zero_to_one();
    f64 random_f64(f64 low, f64 high);
    f64 random_f64_normal_distribution(f64 mean, f64 stddev);
    f64 random_f64_linear_distribution(f64 low, f64 high);
    f64 random_f64_exponential_distribution(f64 lambda);
};

extern Random_Generator default_random_generator;
