#include "synth.h"
#include "memutils.h"
#include "random.h"
#include "math/maths.h"

//
// https://issuu.com/petergoldsborough/docs/thesis
//



/* ------------------------------------------------ Oscillator ------------------------------------------------ */

Synth_Oscillator sine_oscillator(f32 frequency, f32 amplitude, f32 phase_offset) {
    return Synth_Oscillator{ OSCILLATOR_Sine, frequency, amplitude, 0, phase_offset };
}

Synth_Oscillator square_oscillator(f32 frequency, f32 amplitude, u32 partial_count, f32 phase_offset) {
    return Synth_Oscillator{ OSCILLATOR_Square, frequency, amplitude, partial_count, phase_offset };
}

Synth_Oscillator sawtooth_oscillator(f32 frequency, f32 amplitude, u32 partial_count, f32 phase_offset) {
    return Synth_Oscillator{ OSCILLATOR_Sawtooth, frequency, amplitude, partial_count, phase_offset };
}

Synth_Oscillator triangle_oscillator(f32 frequency, f32 amplitude, u32 partial_count, f32 phase_offset) {
    return Synth_Oscillator{ OSCILLATOR_Triangle, frequency, amplitude, partial_count, phase_offset };
}

f32 Synth_Oscillator::tick(f32 time) {
    f32 result = 0.f;
    const f32 angular_frequency = this->frequency * FTAU;
    
    switch(this->kind) {
    case OSCILLATOR_Sine:
        result = sinf(angular_frequency * time + this->phase_offset * FTAU);
        break;

    case OSCILLATOR_Square: {
        for(u32 n = 1; n <= this->partial_count * 2; n += 2) {
            result += (1.f / n) * sinf(angular_frequency * n * time + this->phase_offset * FTAU);
        }
    } break;

    case OSCILLATOR_Sawtooth: {
        for(u32 n = 1; n < this->partial_count; ++n) {
            result += (1.f / n) * sinf(angular_frequency * n * time + this->phase_offset * FTAU);
        }
        result *= 1.7f / FPI; // Normalization to bring the result into the [-1;1] range
    } break;

    case OSCILLATOR_Triangle: {
        f32 amp = -1.f;
        for(u32 n = 1; n <= this->partial_count * 2; n += 2) {
            amp = (amp > 0.f ? -1.f : 1.f) / (n * n);
            result += amp * sinf(angular_frequency * n * time + this->phase_offset * FTAU);
        }
        result *= 8.f / (FPI * FPI); // Normalization to bring the result into the [-1;1] range
    } break;
    }

    result *= this->amplitude;

    return result;
}



/* -------------------------------------------------- Noise -------------------------------------------------- */

f32 Synth_Noise::tick(f32 time) {
    f32 result;

    switch(this->kind) {
    case NOISE_White: result = this->rand.random_f32(-1.f, 1.f); break;
    case NOISE_Pink:  result = this->rand.random_f32_inverse_distribution(); break;
    }

    result *= this->amplitude;

    return result;
}

Synth_Noise noise(Synth_Noise_Kind kind, f32 amplitude) {
    Synth_Noise noise;
    noise.kind      = kind;
    noise.amplitude = amplitude;
    noise.rand      = Random_Generator();
    return noise;
}



/* ------------------------------------------------ Modulator ------------------------------------------------ */

f32 Synth_Envelope_Modulator::tick(f32 time) {
    f32 result = this->input->tick(time);

    f32 amp = 1.f;
    
    if(time <= this->attack_time) {
        // Attack phase
        amp = powf(time / this->attack_time, this->attack_curve);
    } else if(time <= this->attack_time + this->decay_time) {
        // Decay phase
        amp = 1.f - (time - this->attack_time) / this->decay_time * (1.f - this->sustain_level);
    } else if(time <= this->attack_time + this->decay_time + this->sustain_time) {
        // Sustain phase
        amp = this->sustain_level;
    } else {
        // Release phase
        f32 time_in_release = time - (this->attack_time + this->decay_time + this->sustain_time);
        amp = this->sustain_level - min(time_in_release, this->release_time) / this->release_time * this->sustain_level;
    }
    
    return result * amp;
}

f32 Synth_Envelope_Modulator::calculate_loop_time() {
    return this->attack_time + this->decay_time + this->sustain_time + this->release_time;
}

Synth_Envelope_Modulator envelope_modulator(Synthesizer_Module *input, f32 attack_time, f32 attack_curve, f32 decay_time, f32 sustain_level, f32 sustain_time, f32 release_time) {
    Synth_Envelope_Modulator modulator;
    modulator.input         = input;
    modulator.attack_time   = attack_time;
    modulator.attack_curve  = attack_curve;
    modulator.decay_time    = decay_time;
    modulator.sustain_level = sustain_level;
    modulator.sustain_time  = sustain_time;
    modulator.release_time  = release_time;
    return modulator;
}



f32 Synth_Low_Frequency_Modulator::tick(f32 time) {
    const f32 angular_frequency = this->frequency * FTAU;
    f32 amplifier = (sinf(angular_frequency * time + this->phase_offset * FTAU) * 0.5f + 0.5f) * (high - low) + low;
    return amplifier * this->input->tick(time);
}

Synth_Low_Frequency_Modulator low_frequency_modulator(Synthesizer_Module *input, f32 frequency, f32 low, f32 high, f32 phase_offset) {
    Synth_Low_Frequency_Modulator modulator;
    modulator.input        = input;
    modulator.frequency    = frequency;
    modulator.low          = low;
    modulator.high         = high;
    modulator.phase_offset = phase_offset;
    return modulator;
}



/* --------------------------------------------------- Loop --------------------------------------------------- */

f32 Synth_Loop::tick(f32 time) {
    return this->input->tick(fmodf(time, this->loop));
}

Synth_Loop loop(Synthesizer_Module *input, f32 time) {
    Synth_Loop loop;
    loop.input = input;
    loop.loop  = time;
    return loop;
}



/* --------------------------------------------------- Mixer --------------------------------------------------- */

f32 Synth_Mixer::tick(f32 time) {
    return this->lhs->tick(time) + this->rhs->tick(time);
}

Synth_Mixer mix(Synthesizer_Module *lhs, Synthesizer_Module *rhs) {
    Synth_Mixer mixer;
    mixer.lhs = lhs;
    mixer.rhs = rhs;
    return mixer;
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

f32 *update_synth(Synthesizer *synth, u64 requested_frames) {
    if(!synth->module) return null;

    u64 frames_to_generate = min(requested_frames, synth->buffer_size_in_frames);
    
    for(u32 i = 0; i < frames_to_generate; ++i) {
        f32 time = (f32) synth->total_frames_generated / (f32) synth->sample_rate;
        
        f32 frame = synth->module->tick(time);

        for(u8 j = 0; j < synth->channels; ++j) {
            u64 offset = (u64) (synth->available_frames + i) * synth->channels + j;
            synth->buffer[offset] = frame;
        }

        ++synth->total_frames_generated;
    }

    synth->available_frames  = frames_to_generate;
    synth->available_samples = synth->available_frames * synth->channels;

    return synth->buffer;
}
