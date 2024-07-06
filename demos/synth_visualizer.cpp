#include "window.h"
#include "software_renderer.h"
#include "os_specific.h"
#include "synth.h"
#include "audio.h"

#define CONSTANT_TIME_STRETCH true // The entire band width represents one second
#define DRAW_AVERAGE          false // Draw average or min/max values for the sampled area

static
void draw_channel(Window *window, Synthesizer *synth, u8 channel_index) {
    s32 channel_height = 101;

    s32 x0 = 10, x1 = window->w - 10;
    s32 y0 = window->h / 2 - (channel_index * synth->channels - synth->channels / 2) * channel_height, y1 = y0 + channel_height;
            
    draw_quad(x0, y0, x1, y1, Color(50, 50, 50, 200));

    s32 w = min((x1 - x0), (s32) synth->available_frames);
    s32 h = y1 - y0;

#if CONSTANT_TIME_STRETCH
    f32 frames_per_pixel = (f32) AUDIO_SAMPLE_RATE / AUDIO_UPDATES_PER_SECOND / (f32) w;
#else
    f32 frames_per_pixel = (f32) synth->available_frames / (f32) w;
#endif

    f32 frame = 0.f;
    for(s32 x = 0; x < w; ++x) {
        u64 first_frame = (u64) roundf(frame), one_plus_last_frame = min(synth->available_frames, (u64) roundf(frame + frames_per_pixel));

        if(first_frame >= one_plus_last_frame) continue;

        f32 sample_y0, sample_y1;
        f32 sample_x0 = (f32) (x0 + x), sample_x1 = (f32) (x0 + x + 1);
        
#if DRAW_AVERAGE
        f32 avg = 0.f;
        for(u64 i = first_frame; i < one_plus_last_frame; ++i) {
            avg += synth->buffer[i * synth->channels + channel_index];
        }

        avg /= (f32) (one_plus_last_frame - first_frame);

        sample_y0 = (y0 + y1) / 2.f;
        sample_y1 = sample_y0 - avg * 0.5f * h;
#else
        f32 sample_min = 0.f, sample_max = 0.f;
        for(u64 i = first_frame; i < one_plus_last_frame; ++i) {
            sample_min = min(sample_min, synth->buffer[i * synth->channels + channel_index]);
            sample_max = max(sample_max, synth->buffer[i * synth->channels + channel_index]);
        }

        sample_y0 = (y0 + y1) / 2.f - sample_min * 0.5f * h;
        sample_y1 = (y0 + y1) / 2.f - sample_max * 0.5f * h;
#endif

        draw_quad((s32) sample_x0, (s32) sample_y0, (s32) sample_x1, (s32) sample_y1, Color(255, 255, 255, 255));
        
        frame += frames_per_pixel;
    }
}

int main() {
    Window window;
    create_window(&window, "Hello Windows"_s);
    show_window(&window);
    os_enable_high_resolution_clock();

    create_software_renderer(&window);
    
    Frame_Buffer frame_buffer;
    create_frame_buffer(&frame_buffer, window.w, window.h, 4);

    Synthesizer synth;
    create_synth(&synth, AUDIO_CHANNELS, AUDIO_SAMPLE_RATE);

    //Oscillator module = sine_oscillator(400);
    Noise module;
    synth.module = &module;
    
    Audio_Player player;
    Error_Code error = create_audio_player(&player);
    if(error != Success) printf("Error Initialization Player: %.*s\n", (u32) error_string(error).count, error_string(error).data);

    player.volumes[AUDIO_VOLUME_Master] = .1f;
    
    Audio_Buffer buffer;
    create_streaming_audio_buffer(&buffer, AUDIO_BUFFER_FORMAT_Float32, synth.channels, synth.sample_rate, synth.buffer_size_in_frames, "Streaming Buffer"_s);

    Audio_Source *source = acquire_audio_source(&player, AUDIO_VOLUME_Master);
    play_audio_buffer(source, &buffer);

    u32 frames_to_generate = AUDIO_SAMPLES_PER_UPDATE;
    
    while(!window.should_close) {
        Hardware_Time frame_start = os_get_hardware_time();
        
        update_window(&window);

        // Update the synth and audio player
        {
            update_synth(&synth, frames_to_generate);
            update_streaming_audio_buffer(&buffer, (u8 *) synth.buffer, synth.available_frames);
            update_audio_player(&player);

            frames_to_generate = (u32) (AUDIO_SAMPLES_PER_UPDATE - (buffer.frame_count - source->frame_offset_in_buffer));
            consume_frames(&synth, source->frame_offset_in_buffer);
            source->frame_offset_in_buffer = 0;
            source->state = AUDIO_SOURCE_Playing;
        }
        
        // Draw the synth
        {
            bind_frame_buffer(&frame_buffer);
            clear_frame(Color(50, 100, 200, 255));
            for(u8 i = 0; i < synth.channels; ++i) draw_channel(&window, &synth, i);
            swap_buffers(&frame_buffer);
        }

        Hardware_Time frame_end = os_get_hardware_time();
        window_ensure_frame_time(frame_start, frame_end, 30);
    }
    
    destroy_audio_buffer(&buffer);
    destroy_audio_player(&player);
    destroy_synth(&synth);
    destroy_frame_buffer(&frame_buffer);
    destroy_software_renderer();
    destroy_window(&window);
    return 0;
}
