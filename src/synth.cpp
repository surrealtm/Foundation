#include "synth.h"
#include "memutils.h"
#include "math/maths.h"

//
// https://issuu.com/petergoldsborough/docs/thesis
//

void create_synth(Synthesizer *synth, u8 channels, u32 sample_rate) {
    synth->channels               = channels;
    synth->sample_rate            = sample_rate;
    synth->buffer_size_in_frames  = synth->sample_rate; // Make enough space for exactly one second.
    synth->buffer_size_in_bytes   = synth->buffer_size_in_frames * synth->channels * sizeof(f32);
    synth->available_frames       = 0;
    synth->total_frames_generated = 0;
    synth->buffer = (f32 *) Default_Allocator->allocate(synth->buffer_size_in_bytes);
}

void destroy_synth(Synthesizer *synth) {
    Default_Allocator->deallocate(synth->buffer);
    synth->channels               = 0;
    synth->sample_rate            = 0;
    synth->buffer                 = null;
    synth->buffer_size_in_bytes   = 0;
    synth->buffer_size_in_frames  = 0;
    synth->available_frames       = 0;
    synth->total_frames_generated = 0;
}

void update_synth(Synthesizer *synth, u32 requested_frames) {
    synth->available_frames  = min(requested_frames, (u32) synth->buffer_size_in_frames);
    synth->available_samples = synth->available_frames * synth->channels;

    for(u32 i = 0; i < synth->available_frames; ++i) {
        f32 time = (f32) synth->total_frames_generated / (f32) synth->sample_rate;
        
        for(u8 j = 0; j < synth->channels; ++j) {
            u64 offset = i * synth->channels + j;

            synth->buffer[offset] = sinf(time * FTAU);
        }

        ++synth->total_frames_generated;
    }
}
