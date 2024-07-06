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
    f32 frequency;
    f32 amplitude;
    u32 partial_count;

    Synth_Oscillator(Synth_Oscillator_Kind kind, f32 frequency, f32 amplitude, u32 partial_count) :
        kind(kind), frequency(frequency), amplitude(amplitude), partial_count(partial_count) {};
    
    f32 tick(f32 time);
};

struct Synth_Noise : Synthesizer_Module {
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

struct Synth_Loop : Synthesizer_Module {
    Synthesizer_Module *input;
    f32 loop; // In seconds
    
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

Synth_Oscillator sine_oscillator(f32 frequency, f32 amplitude = 1.f);
Synth_Oscillator square_oscillator(f32 frequency, f32 amplitude = 1.f, u32 partial_count = 64);
Synth_Oscillator sawtooth_oscillator(f32 frequency, f32 amplitude = 1.f, u32 partial_count = 64);
Synth_Oscillator triangle_oscillator(f32 frequency, f32 amplitude = 1.f, u32 partial_count = 2);

Synth_Envelope_Modulator envelope_modulator(Synthesizer_Module *input, f32 attack_time = .2f, f32 attack_curve = 1.f, f32 decay_time = .2f, f32 sustain_level = .7f, f32 sustain_time = .2f, f32 release_time = .5f);

Synth_Loop loop(Synthesizer_Module *input, f32 time = 1.f);

void create_synth(Synthesizer *synth, u8 channels, u32 sample_rate);
void destroy_synth(Synthesizer *synth);
f32 *update_synth(Synthesizer *synth, u64 consumed_frames, u64 requested_frames);
