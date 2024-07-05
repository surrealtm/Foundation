#pragma once

#include "foundation.h"

enum Oscillator_Kind {
    OSCILLATOR_Sine,
    OSCILLATOR_Square,
    OSCILLATOR_Sawtooth,
    OSCILLATOR_Triangle,
};

struct Oscillator {
    Oscillator_Kind kind;
    f32 frequency;
    f32 amplitude;

    // Used for all but the sine oscillator. Good values:
    //   Square:   64
    //   Sawtooth: 32
    //   Triangle:  2
    u32 partial_count;
    
    f32 generate(f32 time);
};

struct Synthesizer {
    u8 channels;
    u32 sample_rate;

    f32 *buffer;
    u64 buffer_size_in_bytes;
    u64 buffer_size_in_frames;

    u64 available_frames;
    u64 available_samples;
    
    u64 total_frames_generated;
};

Oscillator sine_oscillator(f32 frequency, f32 amplitude = 1.f);
Oscillator square_oscillator(f32 frequency, f32 amplitude = 1.f, u32 partial_count = 64);
Oscillator sawtooth_oscillator(f32 frequency, f32 amplitude = 1.f, u32 partial_count = 64);
Oscillator triangle_oscillator(f32 frequency, f32 amplitude = 1.f, u32 partial_count = 2);

void create_synth(Synthesizer *synth, u8 channels, u32 sample_rate);
void destroy_synth(Synthesizer *synth);
void update_synth(Synthesizer *synth, u64 requested_frames);
void consume_frames(Synthesizer *synth, u64 frames_consumed);
