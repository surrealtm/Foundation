#include "memutils.h"
#include "os_specific.h"

#include "math/v3.h"
#include "math/m4.h"
#include "math/algebra.h"

#include "window.h"
#include "font.h"

#define D3D11_DEMO    false
#define LINUX_DEMO    false
#define SOFTWARE_DEMO true

#if D3D11_DEMO
#include "d3d11_layer.h"

struct Constants {
    m4f projection;
    v3f color;
};

int main() {
    os_enable_high_resolution_clock();
    
    //install_allocator_console_logger(Default_Allocator, "Heap");
    
	Window window;
	create_window(&window, "Foundation"_s);
    set_window_icon_from_file(&window, "diffraction.ico"_s);
    show_window(&window);
    
    create_d3d11_context(&window);
    
    Frame_Buffer *default_frame_buffer = get_default_frame_buffer(&window);
    
    Frame_Buffer my_frame_buffer; // @Incomplete: Support resizing?
    create_frame_buffer(&my_frame_buffer, 8);
    create_frame_buffer_color_attachment(&my_frame_buffer, window.w, window.h, false);
    create_frame_buffer_depth_stencil_attachment(&my_frame_buffer, window.w, window.h);
    
    Pipeline_State pipeline_state = { false, false, false, false };
    create_pipeline_state(&pipeline_state);
    
    Vertex_Buffer_Array vertex_buffer;
    create_vertex_buffer_array(&vertex_buffer, VERTEX_BUFFER_Triangles);
    allocate_vertex_data(&vertex_buffer, 5 * 6 * 2, 2);
    allocate_vertex_data(&vertex_buffer, 5 * 6 * 2, 2);
    
    Constants constants;
    constants.projection = make_orthographic_projection_matrix((f32) window.w, (f32) window.h, -1, 1);
    constants.color = v3f(1, 1, 1);
    
    Shader_Constant_Buffer constants_buffer;
    create_shader_constant_buffer(&constants_buffer, sizeof(Constants), &constants);
    
    Shader_Input_Specification inputs[] = {
        { "POSITION", 2, 0 },
        { "UV", 2, 1 },
    };
    
    Shader shader;
    create_shader_from_file(&shader, "data\\shader\\rgba.hlsl"_s, inputs, ARRAY_COUNT(inputs));
    
    Texture texture;
    create_texture_from_file(&texture, "data\\textures\\rock.png"_s, TEXTURE_FILTER_Nearest | TEXTURE_WRAP_Edge);
    
    Font font;
    create_font_from_file(&font, "C:\\Windows\\Fonts\\times.ttf"_s, 50, FONT_FILTER_Lcd_With_Alpha, GLYPH_SET_Extended_Ascii);
    
    for(Font_Atlas *atlas = font.atlas; atlas != null; atlas = atlas->next) {
        Texture *texture = Default_Allocator->New<Texture>();
        create_texture_from_memory(texture, atlas->bitmap, atlas->w, atlas->h, atlas->channels, TEXTURE_FILTER_Nearest | TEXTURE_WRAP_Edge);
        
        atlas->user_handle = texture;
    }
    
    f32 total_time = 0.f;
    
	while(!window.should_close) {
        Hardware_Time frame_start = os_get_hardware_time();
        
        {
            update_window(&window);
            
            Text_Mesh text_mesh = build_text_mesh(&font, "AVWa!"_s, 100, 100, TEXT_ALIGNMENT_Left | TEXT_ALIGNMENT_Median, Default_Allocator);
            update_vertex_data(&vertex_buffer, 0, text_mesh.vertices, text_mesh.glyph_count * 6 * 2);
            update_vertex_data(&vertex_buffer, 1, text_mesh.uvs, text_mesh.glyph_count * 6 * 2);
            free_text_mesh(&text_mesh, Default_Allocator);
            
            constants.color.x = cosf(total_time) * 0.5f + 0.5f;
            constants.color.y = sinf(total_time) * 0.5f + 0.5f;
            update_shader_constant_buffer(&constants_buffer, &constants);
            
            bind_frame_buffer(&my_frame_buffer);
            clear_frame_buffer(&my_frame_buffer, .1f, .1f, .1f);
            
            bind_shader(&shader);
            bind_shader_constant_buffer(&constants_buffer, 0, SHADER_Vertex | SHADER_Pixel);
            bind_vertex_buffer_array(&vertex_buffer);
            bind_pipeline_state(&pipeline_state);
            bind_texture((Texture *) font.atlas->user_handle, 0);
            draw_vertex_buffer_array(&vertex_buffer);
            
            blit_frame_buffer(default_frame_buffer, &my_frame_buffer);
            
            swap_d3d11_buffers(&window);
        }
        
        total_time += window.frame_time;
        
        Hardware_Time frame_end = os_get_hardware_time();
        window_ensure_frame_time(frame_start, frame_end, 60);
    }
    
    for(Font_Atlas *atlas = font.atlas; atlas != null; atlas = atlas->next) {
        Texture *texture = (Texture *) atlas->user_handle;
        destroy_texture(texture);
        Default_Allocator->deallocate(texture);
    }
    
    destroy_font(&font);
    destroy_texture(&texture);
    destroy_pipeline_state(&pipeline_state);
    destroy_shader_constant_buffer(&constants_buffer);
    destroy_shader(&shader);
    destroy_vertex_buffer_array(&vertex_buffer);
    destroy_frame_buffer(&my_frame_buffer);
    
    destroy_d3d11_context(&window);
	destroy_window(&window);
    
#if FOUNDATION_DEVELOPER
    if(Default_Allocator->stats.allocations > Default_Allocator->stats.deallocations) {
        foundation_error("Detected Memory Leaks on the Heap Allocator!");
    }
#endif
    
	return 0;
}

#elif LINUX_DEMO

int main() {
    Window window;
    create_window(&window, "Hello Linux"_s);
    set_window_icon_from_file(&window, "data/textures/icon.bmp"_s);
    show_window(&window);
    
    confine_cursor(&window);
    
    while(!window.should_close) {
        update_window(&window);
        window_sleep(0.016f);
    }
    
    unconfine_cursor(&window);
    destroy_window(&window);
    return 0;
}

#elif SOFTWARE_DEMO
#include "software_renderer.h"
#include "synth.h"
#include "audio.h"

static
void draw_channel(Window *window, Synthesizer *synth, u8 channel_index) {
    const s32 channel_height = 101;

    const s32 x0 = 10, x1 = window->w - 10;
    const s32 y0 = window->h / 2 - (channel_index * synth->channels - synth->channels / 2) * channel_height, y1 = y0 + channel_height;
            
    draw_quad(x0, y0, x1, y1, Color(50, 50, 50, 200));

    const s32 w = min((x1 - x0), (s32) synth->available_frames);
    const s32 h = y1 - y0;

#define CONSTANT_TIME_STRETCH true // The entire band width represents one second

#if CONSTANT_TIME_STRETCH
    const f32 frames_per_pixel = 10; // (f32) AUDIO_SAMPLE_RATE / (f32) w;
#else
    const f32 frames_per_pixel = (f32) synth->available_frames / (f32) w;
#endif

    //printf("Frames per pixel: %f\n", frames_per_pixel);

    f32 frame = 0.f;
    for(s32 x = 0; x < w; ++x) {
        u64 first_frame = (u64) roundf(frame), one_plus_last_frame = min(synth->available_frames, (u64) roundf(frame + frames_per_pixel));

        if(first_frame == one_plus_last_frame) continue;
        
        f32 avg = 0.f;
        for(u64 i = first_frame; i < one_plus_last_frame; ++i) avg += synth->buffer[i * synth->channels + channel_index];

        avg /= (f32) (one_plus_last_frame - first_frame);

        // printf("First: %u, 1+Last: %u, Avg: %f\n", first_frame, one_plus_last_frame, avg); // nocheckin

        f32 y = (y0 + y1) / 2.f;
        draw_quad(x0 + x, (s32) y, x0 + x + 1, (s32) (y - avg * 0.5f * h), Color(255, 255, 255, 255));

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
               
    Audio_Player player;
    Error_Code error = create_audio_player(&player);
    if(error != Success) printf("Error Initialization Player: %.*s\n", (u32) error_string(error).count, error_string(error).data);

    Audio_Buffer buffer;
    create_streaming_audio_buffer(&buffer, AUDIO_BUFFER_FORMAT_Float32, synth.channels, synth.sample_rate, synth.buffer_size_in_frames, "Streaming Buffer"_s);

    Audio_Source *source = acquire_audio_source(&player, AUDIO_VOLUME_Master);
    play_audio_buffer(source, &buffer);

    u32 frames_to_generate = AUDIO_SAMPLES_PER_UPDATE;
    
    while(!window.should_close) {
        Hardware_Time frame_start = os_get_hardware_time();
        
        update_window(&window);

        //printf("Frames to generate: %u\n", frames_to_generate);

        update_synth(&synth, frames_to_generate);
        update_streaming_audio_buffer(&buffer, (u8 *) synth.buffer, synth.available_frames);
        update_audio_player(&player);

        frames_to_generate = (u32) (AUDIO_SAMPLES_PER_UPDATE - (buffer.frame_count - source->frame_offset_in_buffer));
        consume_frames(&synth, source->frame_offset_in_buffer);
        source->frame_offset_in_buffer = 0;
        source->state = AUDIO_SOURCE_Playing;
        
        bind_frame_buffer(&frame_buffer);
        clear_frame(Color(50, 100, 200, 255));

        // Draw the synth
        {
            f32 samples[] = { 0.f, 0.25f, 1.f, -.2f, -.3f, -1.f, .5f, .7f, 0.2f, 0.f };
            u32 sample_count = ARRAY_COUNT(samples);

            for(u8 i = 0; i < synth.channels; ++i)
                draw_channel(&window, &synth, i);
        }

        swap_buffers(&frame_buffer);

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

#endif
