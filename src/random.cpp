#include "random.h"
#include "math/maths.h"

Random_Generator default_random_generator;


void Random_Generator::seed() {
    this->seed(1234567890987654321ULL, 362436362436362436ULL, 1066149217761810ULL, 123456123456123456ULL);
}

void Random_Generator::seed(u64 x) {
    this->seed(x, 362436362436362436ULL, 1066149217761810ULL, 123456123456123456ULL);
}

void Random_Generator::seed(u64 x, u64 y, u64 z, u64 c) {
    this->x = x;
    this->y = y;
    this->z = z;
    this->c = c;
}

u64 Random_Generator::random_u64() {
    //
    // https://www.thecodingforums.com/threads/64-bit-kiss-rngs.673657/
    // https://github.com/cgwrench/librandom/blob/master/src/kiss.c
    //

    /* Congruential generator */
    this->x = 6906969069ULL * this->x + 1234567;
    
    /* 3-shift shift-register generator */
    this->y ^= (this->y << 13);
    this->y ^= (this->y >> 17);
    this->y ^= (this->y << 43);

    /* Multiply-with-carry generator */
    u64 t = (this->z << 58) + this->c;
    this->c  = (this->z >>  6);
    this->z += t;
    this->c += (this->z < t);

    return this->x + this->y + this->z;
}

u64 Random_Generator::random_u64(u64 low, u64 high) {
    return this->random_u64() % (high - low) + low;
}

f32 Random_Generator::random_f32_zero_to_one() {
    return (f32) this->random_u64() / (f32) MAX_U64;
}

f32 Random_Generator::random_f32(f32 low, f32 high) {
    return this->random_f32_zero_to_one() * (high - low) + low;
}

f32 Random_Generator::random_f32_normal_distribution(f32 mean, f32 stddev) {
    f32 theta  = this->random_f32_zero_to_one() * FTAU;
    f32 rho    = sqrtf(logf(1.f - this->random_f32(0.00001f, 1.0f)) * -2.0f);
    f32 result = mean + stddev * rho * cosf(theta);
    return result;
}

f32 Random_Generator::random_f32_linear_distribution(f32 low, f32 high) {
    f32 x = this->random_f32_zero_to_one();
    return sqrtf(2.f * x) * (high - low) + low;
}

f32 Random_Generator::random_f32_exponential_distribution(f32 lambda) {
    f32 x = this->random_f32_zero_to_one();
    return -(logf(x) / lambda);
}

f64 Random_Generator::random_f64_zero_to_one() {
    return (f64) this->random_u64() / (f64) MAX_U64;
}

f64 Random_Generator::random_f64(f64 low, f64 high) {
    return this->random_f64_zero_to_one() * (high - low) + low;
}

f64 Random_Generator::random_f64_normal_distribution(f64 mean, f64 stddev) {
    f64 theta  = this->random_f64_zero_to_one() * TAU;
    f64 rho    = sqrt(log(1. - this->random_f64(0.00001, 1.0)) * -2.0);
    f64 result = mean + stddev * rho * cos(theta);
    return result;
}

f64 Random_Generator::random_f64_linear_distribution(f64 low, f64 high) {
    f64 x = this->random_f64_zero_to_one();
    return sqrt(2. * x) * (high - low) + low;
}

f64 Random_Generator::random_f64_exponential_distribution(f64 lambda) {
    f64 x = this->random_f64_zero_to_one();
    return -(log(x) / lambda);
}
