#pragma once

#include "foundation.h"
#include "random.h"

struct Synthesizer_Module {
    virtual f32 tick(f32 time) = 0;
};

enum Oscillator_Kind {
    OSCILLATOR_Sine,
    OSCILLATOR_Square,
    OSCILLATOR_Sawtooth,
    OSCILLATOR_Triangle,
};

struct Oscillator : Synthesizer_Module {
    Oscillator_Kind kind;
    f32 frequency;
    f32 amplitude;
    u32 partial_count;

    Oscillator(Oscillator_Kind kind, f32 frequency, f32 amplitude, u32 partial_count) :
        kind(kind), frequency(frequency), amplitude(amplitude), partial_count(partial_count) {};
    
    f32 tick(f32 time);
};

struct Noise : Synthesizer_Module {
    Random_Generator rand;

    f32 tick(f32 time);
};

struct Envelope_Modulator : Synthesizer_Module {
    Synthesizer_Module *input;

    f32 attack_time;
    f32 attack_curve;
    f32 decay_time;
    f32 sustain_level;
    f32 sustain_time;
    f32 release_time;
    
    f32 tick(f32 time);
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

    Synthesizer_Module *module;
};

Oscillator sine_oscillator(f32 frequency, f32 amplitude = 1.f);
Oscillator square_oscillator(f32 frequency, f32 amplitude = 1.f, u32 partial_count = 64);
Oscillator sawtooth_oscillator(f32 frequency, f32 amplitude = 1.f, u32 partial_count = 64);
Oscillator triangle_oscillator(f32 frequency, f32 amplitude = 1.f, u32 partial_count = 2);

Envelope_Modulator envelope_modulator(Synthesizer_Module *input, f32 attack_time = 2.f, f32 attack_curve = 1.f, f32 decay_time = .2f, f32 sustain_level = .7f, f32 release_time = .5f);

void create_synth(Synthesizer *synth, u8 channels, u32 sample_rate);
void destroy_synth(Synthesizer *synth);
void update_synth(Synthesizer *synth, u64 requested_frames);
void consume_frames(Synthesizer *synth, u64 frames_consumed);
