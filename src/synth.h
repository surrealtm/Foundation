#pragma once

#include "foundation.h"
#include "random.h"

struct Synthesizer_Module {
    virtual f32 tick(f32 time) = 0;
};

enum Synth_Oscillator_Kind {
    OSCILLATOR_Sine,
    OSCILLATOR_Square,
    OSCILLATOR_Sawtooth,
    OSCILLATOR_Triangle,
};

struct Synth_Oscillator : Synthesizer_Module {
    Synth_Oscillator_Kind kind;
    f32 phase_offset = 0.f;
    f32 frequency;
    f32 amplitude;
    u32 partial_count;

    Synth_Oscillator(Synth_Oscillator_Kind kind, f32 frequency, f32 amplitude, u32 partial_count, f32 phase_offset) :
        kind(kind), frequency(frequency), amplitude(amplitude), partial_count(partial_count), phase_offset(phase_offset) {};
    
    f32 tick(f32 time);
};

enum Synth_Noise_Kind {
    NOISE_White,
    NOISE_Pink
};

struct Synth_Noise : Synthesizer_Module {
    Synth_Noise_Kind kind;
    f32 amplitude;
    
    Random_Generator rand;

    f32 tick(f32 time);
};

struct Synth_Envelope_Modulator : Synthesizer_Module {
    Synthesizer_Module *input;

    f32 attack_time;
    f32 attack_curve;
    f32 decay_time;
    f32 sustain_level;
    f32 sustain_time;
    f32 release_time;
    
    f32 tick(f32 time);
    f32 calculate_loop_time();
};

struct Synth_Low_Frequency_Modulator : Synthesizer_Module {
    Synthesizer_Module *input;
    f32 phase_offset;
    f32 frequency;
    f32 low, high;

    f32 tick(f32 time);
};

struct Synth_Loop : Synthesizer_Module {
    Synthesizer_Module *input;
    f32 loop; // In seconds
    
    f32 tick(f32 time);
};

struct Synth_Mixer : Synthesizer_Module {
    Synthesizer_Module *lhs, *rhs;

    f32 tick(f32);
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

Synth_Oscillator sine_oscillator(f32 frequency, f32 amplitude = 1.f, f32 phase_offset = 0.f);
Synth_Oscillator square_oscillator(f32 frequency, f32 amplitude = 1.f, u32 partial_count = 64, f32 phase_offset = 0.f);
Synth_Oscillator sawtooth_oscillator(f32 frequency, f32 amplitude = 1.f, u32 partial_count = 64, f32 phase_offset = 0.f);
Synth_Oscillator triangle_oscillator(f32 frequency, f32 amplitude = 1.f, u32 partial_count = 2, f32 phase_offset = 0.f);

Synth_Noise noise(Synth_Noise_Kind kind, f32 amplitude);

Synth_Envelope_Modulator envelope_modulator(Synthesizer_Module *input, f32 attack_time = .2f, f32 attack_curve = 1.f, f32 decay_time = .2f, f32 sustain_level = .7f, f32 sustain_time = .2f, f32 release_time = .3f);
Synth_Low_Frequency_Modulator low_frequency_modulator(Synthesizer_Module *input, f32 frequency, f32 low = 0.7f, f32 high = 1.f, f32 phase_offset = 0.f);

Synth_Loop loop(Synthesizer_Module *input, f32 time = 1.f);

Synth_Mixer mix(Synthesizer_Module *lhs, Synthesizer_Module *rhs);

void create_synth(Synthesizer *synth, u8 channels, u32 sample_rate);
void destroy_synth(Synthesizer *synth);
f32 *update_synth(Synthesizer *synth, u64 requested_frames);
