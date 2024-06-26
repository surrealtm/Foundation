#include "noise.h"

#define F2 0.366025403
#define G2 0.211324865
#define H2 40.0

#define FASTFLOOR(x) ( ((x)>0) ? ((int)x) : (((int)x)-1) )

static u8 perm[512] = {151,160,137,91,90,15,
                       131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
                       190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
                       88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
                       77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
                       102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
                       135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
                       5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
                       223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
                       129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
                       251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
                       49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
                       138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
                       151,160,137,91,90,15,
                       131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
                       190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
                       88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
                       77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
                       102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
                       135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
                       5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
                       223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
                       129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
                       251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
                       49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
                       138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

static f64 grad2lut[8][2] = {
    { -1.0, -1.0 }, { 1.0,  0.0 }, { -1.0, 0.0 }, { 1.0,  1.0 },
    { -1.0,  1.0 }, { 0.0, -1.0 }, {  0.0, 1.0 }, { 1.0, -1.0 }
};
	

static inline
void grad2(s32 hash, f64 *gx, f64 *gy) {
    s32 h = hash & 7;
    *gx = grad2lut[h][0];
    *gy = grad2lut[h][1];
    return;
}

Noise_Result_2D Simplex_Noise_2D::noise(f64 x, f64 y) {
    f64 n0, n1, n2; // Noise contributions from the three corners
	
	// Skew the input space to determine which simplex cell we're in
	f64 s = (x + y) * F2; // Hairy factor for 2D
	f64 xs = x + s;
	f64 ys = y + s;
	s32 i = FASTFLOOR(xs);
	s32 j = FASTFLOOR(ys);
	
	f64 t = (f64)(i + j) * G2;
	f64 X0 = i - t; // Unskew the cell origin back to (x,y) space
	f64 Y0 = j - t;
	f64 x0 = x - X0; // The x,y distances from the cell origin
	f64 y0 = y - Y0;
	
	// For the 2D case, the simplex shape is an equilateral triangle.
	// Determine which simplex we are in.
	s32 i1, j1; // Offsets for second (middle) corner of simplex in (i,j) coords
	if(x0 > y0) {
        // lower triangle, XY order: (0,0)->(1,0)->(1,1)
        i1 = 1; 
        j1 = 0;
    } else {
        // upper triangle, YX order: (0,0)->(0,1)->(1,1)
        i1=0; 
        j1=1;
    }
	
	// A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and
	// a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where
	// c = (3-sqrt(3))/6
	
	f64 x1 = x0 - i1 + G2; // Offsets for middle corner in (x,y) unskewed coords
	f64 y1 = y0 - j1 + G2;
	f64 x2 = x0 - 1.0 + 2.0 * G2; // Offsets for last corner in (x,y) unskewed coords
	f64 y2 = y0 - 1.0 + 2.0 * G2;
	
	// Wrap the s32eger indices at 256, to avoid indexing details::perm[] out of bounds
	s32 ii = i & 0xff;
	s32 jj = j & 0xff;
	
	f64 gx0, gy0, gx1, gy1, gx2, gy2; /* Gradients at simplex corners */

    /* Calculate the contribution from the three corners */
	f64 t0 = 0.5 - x0 * x0 - y0 * y0;
	f64 t20, t40;
	if(t0 < 0.0) {
        t40 = t20 = t0 = n0 = gx0 = gy0 = 0.0; /* No influence */
	} else {
		grad2(perm[ii + perm[jj]], &gx0, &gy0);
		t20 = t0 * t0;
		t40 = t20 * t20;
		n0 = t40 * (gx0 * x0 + gy0 * y0);
	}
	
	f64 t1 = 0.5 - x1 * x1 - y1 * y1;
	f64 t21, t41;
	if(t1 < 0.0) {
        t21 = t41 = t1 = n1 = gx1 = gy1 = 0.0; /* No influence */
	} else {
		grad2(perm[ii + i1 + perm[jj + j1]], &gx1, &gy1);
		t21 = t1 * t1;
		t41 = t21 * t21;
		n1 = t41 * (gx1 * x1 + gy1 * y1);
	}
	
	f64 t2 = 0.5 - x2 * x2 - y2 * y2;
	f64 t22, t42;
	if(t2 < 0.0) {
        t42 = t22 = t2 = n2 = gx2 = gy2 = 0.0; /* No influence */
	} else {
		grad2(perm[ii + 1 + perm[jj + 1]], &gx2, &gy2);
		t22 = t2 * t2;
		t42 = t22 * t22;
		n2 = t42 * (gx2 * x2 + gy2 * y2);
	}
	
	/* Compute derivative, if requested by supplying non-null pos32ers
	 * for the last two arguments */
	/*  A straight, unoptimised calculation would be like:
	 *    *dnoise_dx = -8.0f * t20 * t0 * x0 * (gx0 * x0 + gy0 * y0) + t40 * gx0;
	 *    *dnoise_dy = -8.0f * t20 * t0 * y0 * (gx0 * x0 + gy0 * y0) + t40 * gy0;
	 *    *dnoise_dx += -8.0f * t21 * t1 * x1 * (gx1 * x1 + gy1 * y1) + t41 * gx1;
	 *    *dnoise_dy += -8.0f * t21 * t1 * y1 * (gx1 * x1 + gy1 * y1) + t41 * gy1;
	 *    *dnoise_dx += -8.0f * t22 * t2 * x2 * (gx2 * x2 + gy2 * y2) + t42 * gx2;
	 *    *dnoise_dy += -8.0f * t22 * t2 * y2 * (gx2 * x2 + gy2 * y2) + t42 * gy2;
	 */
	f64 temp0 = t20 * t0 * (gx0 * x0 + gy0 * y0);
	f64 dnoise_dx = temp0 * x0;
	f64 dnoise_dy = temp0 * y0;
	f64 temp1 = t21 * t1 * (gx1 * x1 + gy1 * y1);
	dnoise_dx += temp1 * x1;
	dnoise_dy += temp1 * y1;
	f64 temp2 = t22 * t2 * (gx2 * x2 + gy2 * y2);
	dnoise_dx += temp2 * x2;
	dnoise_dy += temp2 * y2;
	dnoise_dx *= -8.0;
	dnoise_dy *= -8.0;
	dnoise_dx += t40 * gx0 + t41 * gx1 + t42 * gx2;
	dnoise_dy += t40 * gy0 + t41 * gy1 + t42 * gy2;
	
	// Add contributions from each corner to get the final noise value.
	// The result is scaled to return values in the interval [-1,1].
    Noise_Result_2D result;
    result.value = H2 * (n0 + n1 + n2);
    result.dx    = dnoise_dx;
    result.dy    = dnoise_dy;
    return result;
}

Noise_Result_2D Simplex_Noise_2D::fractal_noise(f64 x, f64 y) {
    Noise_Result_2D sum = { 0 };
    f64 freq = this->frequency;
    f64 amp  = this->amplitude;

    for(u32 i = 0; i < this->octaves; ++i) {
        Noise_Result_2D it = this->noise(x * freq, y * freq);
        sum.value += it.value * amp;
        sum.dx    += it.dx * amp;
        sum.dy    += it.dy * amp;
        freq *= this->lacunarity;
        amp  *= this->gain;
    }

    return sum;
}
