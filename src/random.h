#pragma once

#include "foundation.h"

void set_random_seed(u64 seed);

u32 get_random_u32();
u32 get_random_u32(u32 low, u32 high);

f64 get_random_f64_zero_to_one();
f64 get_random_f64_uniform(f64 low, f64 high);
f64 get_random_f64_normal(f64 mean, f64 stddev, f64 low, f64 high);

f32 get_random_f32_zero_to_one();
f32 get_random_f32_uniform(f32 low, f32 high);
f32 get_random_f32_normal(f32 mean, f32 stddev, f32 low, f32 high);
