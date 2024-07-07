#include "window.h"
#include "software_renderer.h"
#include "os_specific.h"
#include "synth.h"
#include "audio.h"

struct Histogram {
    f32 *samples;
    u64 buffer_size_in_samples;
    u64 latest_sample_index;
    u64 present_sample_index;
};

static
void create_histogram(Histogram *histogram, u64 samples) {
    histogram->buffer_size_in_samples = samples;
    histogram->samples                = (f32 *) Default_Allocator->allocate(histogram->buffer_size_in_samples * sizeof(f32));
    histogram->latest_sample_index    = 0;
    histogram->present_sample_index   = 0;
}

static
void destroy_histogram(Histogram *histogram) {
    Default_Allocator->deallocate(histogram->samples);
    histogram->samples = null;
    histogram->buffer_size_in_samples = 0;
}

static
void add_histogram_data(Histogram *histogram, f32 *frames, u64 frame_count, u64 frame_stride, u64 frame_offset) {
    // Delete old samples that no longer have space in the histogram.
    u64 unused_samples = histogram->buffer_size_in_samples - histogram->latest_sample_index;
    if(frame_count > unused_samples) {
        s64 samples_to_remove = frame_count - unused_samples;
        memmove(&histogram->samples[0], &histogram->samples[samples_to_remove], (histogram->buffer_size_in_samples - samples_to_remove) * sizeof(f32));
        histogram->latest_sample_index  -= samples_to_remove;
        histogram->present_sample_index -= samples_to_remove;
    }

    // Move the new samples into the histogram.
    for(u64 i = 0; i < frame_count; ++i) {
        histogram->samples[histogram->latest_sample_index] = frames[i * frame_stride + frame_offset];
        ++histogram->latest_sample_index;
    }
}

static
void progress_histogram(Histogram *histogram, u64 samples) {
    histogram->present_sample_index = min(histogram->present_sample_index + samples, histogram->latest_sample_index);
}

static
f32 query_histogram(Histogram *histogram, u64 sample_index) {
    assert(sample_index >= 0 && sample_index <= histogram->present_sample_index);
    return histogram->samples[histogram->present_sample_index - sample_index];
}

static
u64 get_valid_samples_in_histogram(Histogram *histogram) {
    return histogram->present_sample_index + 1;
}

static
void draw_histogram(Window *window, Histogram *histogram, s32 histogram_index, s32 histogram_count) {
    s32 channel_height = 101;

    s32 channel_x0 = 10, channel_x1 = window->w - 10;
    s32 channel_y0 = window->h / 2 - (histogram_index * histogram_count - histogram_count/ 2) * channel_height, channel_y1 = channel_y0 + channel_height;
    draw_quad(channel_x0, channel_y0, channel_x1, channel_y1, Color(50, 50, 50, 200));
    
    s32 pixels = channel_x1 - channel_x0;
    u64 valid_samples = get_valid_samples_in_histogram(histogram);
    f32 samples_per_pixel = (f32) AUDIO_SAMPLE_RATE / (f32) pixels;

    f32 sample_pointer = (f32) 0;
    for(s32 x = 0; x < pixels; ++x) {
        u64 first_sample = (u64) roundf(sample_pointer), one_plus_last_sample = min(valid_samples, (u64) roundf(sample_pointer + samples_per_pixel));

        if(first_sample >= one_plus_last_sample) continue;

        f32 sample_min = 0.f, sample_max = 0.f;
        for(u64 j = first_sample; j < one_plus_last_sample; ++j) {
            sample_min = min(sample_min, query_histogram(histogram, j));
            sample_max = max(sample_max, query_histogram(histogram, j));
        }

        sample_min = max(-1.f, sample_min);
        sample_max = min( 1.f, sample_max);

        s32 sample_x0 = (channel_x1 - x - 1);
        s32 sample_x1 = (sample_x0 + 1);
        s32 sample_y0 = (s32) ((channel_y0 + channel_y1) / 2.f - sample_max * 0.5f * channel_height);
        s32 sample_y1 = (s32) ((channel_y0 + channel_y1) / 2.f - sample_min * 0.5f * channel_height);
        
        draw_quad(sample_x0, sample_y0, sample_x1, sample_y1, Color(255, 255, 255, 255));
        
        sample_pointer += samples_per_pixel;
    }

    draw_outlined_quad(channel_x0, channel_y0, channel_x1, channel_y1, 2, Color(200, 200, 200, 200));
}

int main() {
    //
    // Display
    //
    Window window;
    create_window(&window, "Hello Windows"_s);
    show_window(&window);
    os_enable_high_resolution_clock();

    create_software_renderer(&window);
    
    Frame_Buffer frame_buffer;
    create_frame_buffer(&frame_buffer, window.w, window.h, 4);


    //
    // Synthesizer
    //
    Synthesizer synth;
    create_synth(&synth, AUDIO_CHANNELS, AUDIO_SAMPLE_RATE);
    
    Synth_Oscillator first_sine             = sine_oscillator(400);
    Synth_Noise noiser                      = noise(NOISE_Pink, .1f);
    Synth_Envelope_Modulator envelope       = envelope_modulator(&first_sine);
    Synth_Loop looper                       = loop(&envelope, envelope.calculate_loop_time() + .2f);
    Synth_Low_Frequency_Modulator first_lf  = low_frequency_modulator(&looper, 10);
    Synth_Mixer first_mixer                 = mix(&noiser, &first_lf);
    
    Synth_Oscillator second_sine            = sine_oscillator(600, .2f);
    Synth_Low_Frequency_Modulator second_lf = low_frequency_modulator(&second_sine, .25f, 0.f, 1.f);
    Synth_Oscillator third_sine             = triangle_oscillator(350, .2f);
    Synth_Low_Frequency_Modulator third_lf  = low_frequency_modulator(&third_sine, .25f, 0.f, 1.f, .5f);
    Synth_Mixer second_mixer                = mix(&second_lf, &third_lf);
    
    Synth_Mixer final_mixer                = mix(&first_mixer, &second_mixer);

    synth.module = &final_mixer;


    //
    // Histograms
    //
    Histogram histograms[AUDIO_CHANNELS];
    for(s32 i = 0; i < AUDIO_CHANNELS; ++i) create_histogram(&histograms[i], (u64) (AUDIO_SAMPLE_RATE * 1.5f));


    //
    // Audio output
    //
    Audio_Player player;
    Error_Code error = create_audio_player(&player);
    if(error != Success) printf("Error Initialization Player: %.*s\n", (u32) error_string(error).count, error_string(error).data);

    player.volumes[AUDIO_VOLUME_Master] = .1f;
    
    Audio_Stream *stream = create_audio_stream(&player, &synth, (Audio_Stream_Callback) update_synth, AUDIO_VOLUME_Master, ""_s);

    update_audio_player_with_silence(&player); // Avoid sound artifacts due to the long loading times which would require wayyy too many samples to be created.

    while(!window.should_close) {
        Hardware_Time frame_start = os_get_hardware_time();
        
        update_window(&window);

        // Update the synth and audio player
        {
            update_audio_player(&player);
        }
        
        // Update the histograms
        {
            for(s32 i = 0; i < AUDIO_CHANNELS; ++i) {
                progress_histogram(&histograms[i], (u64) (window.frame_time * AUDIO_SAMPLE_RATE));
                add_histogram_data(&histograms[i], (f32 *) stream->buffer.data, stream->frames_played_last_update, stream->buffer.channels, i);
            }
        }

        // Draw the histograms
        {
            bind_frame_buffer(&frame_buffer);
            clear_frame(Color(50, 100, 200, 255));

            for(s32 i = 0; i < AUDIO_CHANNELS; ++i) {
                draw_histogram(&window, &histograms[i], i, synth.channels);
            }

            swap_buffers(&frame_buffer);
        }

        if(1.f / window.frame_time < AUDIO_UPDATES_PER_SECOND) printf("Dropping frames (%d presented, %d needed).\n", (s32) (1.f / window.frame_time), AUDIO_UPDATES_PER_SECOND);
        
        Hardware_Time frame_end = os_get_hardware_time();
        window_ensure_frame_time(frame_start, frame_end, 61);
    }


    //
    // Cleanup
    //

    for(s64 i = 0; i < AUDIO_CHANNELS; ++i) destroy_histogram(&histograms[i]);
    
    destroy_audio_player(&player);
    destroy_synth(&synth);
    destroy_frame_buffer(&frame_buffer);
    destroy_software_renderer();
    destroy_window(&window);
    return 0;
}
