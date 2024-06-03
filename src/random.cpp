#include "random.h"

#define __random_clamp(value, low, high) ((value < low) ? (low) : ((value) > (high) ? (high) : (value)))

u64 current_random_state = 0xcafef00dd15ea5e5;
const u64 random_multiplier = 6364136223846793005;

void set_random_seed(u64 seed) {
    current_random_state = seed;
}


u32 get_random_u32() {
    u64 x = current_random_state;
    u64 count = (x >> 61);

	current_random_state = x * random_multiplier;
	x ^= x >> 22;
	return (u32) (x >> (22 + count));

}

u32 get_random_u32(u32 low, u32 high) {
    return (get_random_u32() % (high - low)) + low;
}


f64 get_random_f64_zero_to_one() {
    return get_random_u32() / 4294967295.0;
}

f64 get_random_f64_uniform(f64 low, f64 high) {
    return get_random_f64_zero_to_one() * (high - low) + low;
}

f64 get_random_f64_normal(f64 mean, f64 stddev, f64 low, f64 high) {
    f64 theta  = get_random_f64_zero_to_one() * 6.28318;
    f64 rho    = sqrt(log(1 - get_random_f64_uniform(0.00001, 1.0)) * -2.0);
    f64 result = mean + stddev * rho * cos(theta);
    result     = __random_clamp(result, low, high);
    return result;
}


f32 get_random_f32_zero_to_one() {
    return get_random_u32() / 4294967295.0f;
}

f32 get_random_f32_uniform(f32 low, f32 high) {
    return get_random_f32_zero_to_one() * (high - low) + low;
}

f32 get_random_f32_normal(f32 mean, f32 stddev, f32 low, f32 high) {
    f32 theta  = get_random_f32_zero_to_one() * 6.28318f;
    f32 rho    = sqrtf(logf(1 - get_random_f32_uniform(0.00001f, 1.0f)) * -2.0f);
    f32 result = mean + stddev * rho * cosf(theta);
    result     = __random_clamp(result, low, high);
    return result;
}
