#include "noise.h"

static u8 simplex_noise_permutation_vector[256] = {
    151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69,
    142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219,
    203, 117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175,
    74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230,
    220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76,
    132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186,
    3, 64, 52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59,
    227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70,
    221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178,
    185, 112, 104, 218, 246, 97, 228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81,
    51, 145, 235, 249, 14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115,
    121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195,
    78, 66, 215, 61, 156, 180
};

u8 Simplex_Noise_2D::hash(u8 i) {
    return simplex_noise_permutation_vector[i];
}

u8 Simplex_Noise_2D::hash(s32 x, s32 y) {
    return this->hash((u8) (x + this->hash((u8) (y + this->hash((u8) this->seed)))));
}

s32 Simplex_Noise_2D::floor(f64 t) {
    s32 i = static_cast<s32>(t);
    return (t < i) ? (i - 1) : i;
}

f64 Simplex_Noise_2D::grad(u32 hash, f64 x, f64 y) {
    u32 h = hash & 0x3F;   // convert low 3 bits of hash code
    f64 u = h < 4 ? x : y; // into 8 simple gradient directions
    f64 v = h < 4 ? y : x;

    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0 * v : 2.0 * v); // compute dot product with (x,y)
}

f64 Simplex_Noise_2D::contrib(s32 gi, f64 x, f64 y) {
    f64 t = 0.5 - x * x - y * y;
    f64 n;
    
    if(t >= 0.0) {
        t = t * t;
        n = t * t * this->grad(gi, x, y);
    } else {
        n = 0.0;
    }
    
    return n;
}

f64 Simplex_Noise_2D::noise(f64 x, f64 y) {
    // Skewing/Unskewing factors for 2D
    const f64 F2 = 0.366025403f;  // F2 = (sqrt(3) - 1) / 2
    const f64 G2 = 0.211324865f;  // G2 = (3 - sqrt(3)) / 6   = F2 / (1 + 2 * K)
    const f64 H2 = 45.23065f; // to get the end result into [-1,1] intervall
    
    // determine which simplex cell were in
    f64 s       = (x + y) * F2;
    s32 cell[2] = { this->floor(x + s), this->floor(y + s) };
    
    // unskew the cell origin back to (x,y) space
    f64 t = static_cast<f64>(cell[0] + cell[1]) * G2;
    f64 v0[2] = { x - (cell[0] - t), y - (cell[1] - t) }; // the x,y distances from the cell origin

    s32 cell_offset[2] = { (v0[0] > v0[1]) ? 1 : 0, (v0[0] > v0[1]) ? 0 : 1 }; // offset for second corner of simplex in (i,j) coords

    f64 v1[2] = { v0[0] - cell_offset[0] + G2,  v0[1] - cell_offset[1] + G2 }; // offset for middle corner in (x,y)
    f64 v2[2] = { v0[0] - 1.0 + 2.0 * G2, v0[1] - 1.0 + 2.0 * G2 }; // offset for last corner in (x,y)

    s32 gi_x = this->hash(cell[0], cell[1]);
    s32 gi_y = this->hash(cell[0] + cell_offset[0], cell[1] + cell_offset[1]);
    s32 gi_z = this->hash(cell[0] + 1, cell[1] + 1);

    f64 n_x = this->contrib(gi_x, v0[0], v0[1]);
    f64 n_y = this->contrib(gi_y, v1[0], v1[1]);
    f64 n_z = this->contrib(gi_z, v2[0], v2[1]);

    return H2 * (n_x + n_y + n_z);  
}

f64 Simplex_Noise_2D::fractal_noise(f64 x, f64 y) {
    f64 output = 0.0;
    f64 denom  = 0.0;
    f64 freq   = this->frequency;
    f64 amp    = this->amplitude;
    
    for(u32 i = 0; i < this->octaves; ++i) {
        output += (amp * this->noise(x * freq / this->scale, y * freq / this->scale));
        denom  += amp;
        
        freq *= this->lacunarity;
        amp  *= this->persistance;
    }

    return (output / denom) * this->amplitude;
}
