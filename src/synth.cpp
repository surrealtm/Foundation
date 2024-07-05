#include "synth.h"
#include "memutils.h"
#include "math/maths.h"

//
// https://issuu.com/petergoldsborough/docs/thesis
//



/* ------------------------------------------------ Oscillator ------------------------------------------------ */

Oscillator sine_oscillator(f32 frequency, f32 amplitude) {
    return Oscillator{ OSCILLATOR_Sine, frequency, amplitude, 0 };
}

Oscillator square_oscillator(f32 frequency, f32 amplitude, u32 partial_count) {
    return Oscillator{ OSCILLATOR_Square, frequency, amplitude, partial_count };
}

Oscillator sawtooth_oscillator(f32 frequency, f32 amplitude, u32 partial_count) {
    return Oscillator{ OSCILLATOR_Sawtooth, frequency, amplitude, partial_count };
}

Oscillator triangle_oscillator(f32 frequency, f32 amplitude, u32 partial_count) {
    return Oscillator{ OSCILLATOR_Triangle, frequency, amplitude, partial_count };
}

f32 Oscillator::generate(f32 time) {
    f32 result = 0.f;
    const f32 angular_frequency = this->frequency * FTAU;
    
    switch(this->kind) {
    case OSCILLATOR_Sine:
        result = sinf(angular_frequency * time);
        break;

    case OSCILLATOR_Square: {
        for(u32 n = 1; n <= this->partial_count * 2; n += 2) {
            result += (1.f / n) * sinf(angular_frequency * n * time);
        }
    } break;

    case OSCILLATOR_Sawtooth: {
        for(u32 n = 1; n < this->partial_count; ++n) {
            result += (1.f / n) * sinf(angular_frequency * n * time);
        }
    } break;

    case OSCILLATOR_Triangle: {
        f32 amp = -1.f;
        for(u32 n = 1; n <= this->partial_count * 2; n *= 2) {
            amp = (amp > 0.f ? -1.f : 1.f) / (n * n);
            result += amp * sinf(angular_frequency * n * time);
        }
    } break;
    }

    result *= this->amplitude;

    return result;
}



/* ----------------------------------------------- Synthesizer ----------------------------------------------- */

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

    auto osc = sawtooth_oscillator(400, .5f);
    
    for(u32 i = 0; i < frames_to_generate; ++i) {
        f32 time = (f32) synth->total_frames_generated / (f32) synth->sample_rate;
        
        f32 frame = osc.generate(time);

        for(u8 j = 0; j < synth->channels; ++j) {
            u64 offset = (u64) (synth->available_frames + i) * synth->channels + j;
            synth->buffer[offset] = frame;
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
