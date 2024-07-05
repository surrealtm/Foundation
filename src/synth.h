#pragma once

#include "foundation.h"

struct Synthesizer {
    u8 channels;
    u32 sample_rate;
    
    f32 *buffer;
    u64 buffer_size_in_bytes;
    u64 buffer_size_in_frames;

    u32 available_frames;
    u32 available_samples;
    
    u64 total_frames_generated;
};

void create_synth(Synthesizer *synth, u8 channels, u32 sample_rate);
void destroy_synth(Synthesizer *synth);
void update_synth(Synthesizer *synth, u32 requested_frames);
