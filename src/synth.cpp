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

void update_synth(Synthesizer *synth, u64 requested_frames) {
    u64 frames_to_generate = min(requested_frames, (synth->buffer_size_in_frames - synth->available_frames));

    for(u32 i = 0; i < frames_to_generate; ++i) {
        f32 time = (f32) synth->total_frames_generated / (f32) synth->sample_rate;
        
        for(u8 j = 0; j < synth->channels; ++j) {
            u64 offset = (u64) (synth->available_frames + i) * synth->channels + j;

            f32 HZ = 240 + j * 400;

            synth->buffer[offset] = sinf(HZ * time * FTAU);
        }

        ++synth->total_frames_generated;
    }

    synth->available_frames += frames_to_generate;
    synth->available_samples = synth->available_frames * synth->channels;
}

void consume_frames(Synthesizer *synth, u64 frames_consumed) {
    memmove(synth->buffer, &synth->buffer[frames_consumed * synth->channels], (synth->available_frames - frames_consumed) * synth->channels * sizeof(f32));
    synth->available_frames -= frames_consumed;
}
