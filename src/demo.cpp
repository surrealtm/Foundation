#include "memutils.h"
#include "os_specific.h"

#include "math/v3.h"
#include "math/m4.h"
#include "math/algebra.h"

#include "window.h"
#include "d3d11_layer.h"
#include "font.h"

#define D3D11_DEMO false
#define LINUX_DEMO true

#if D3D11_DEMO

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
    return 0;
}

#endif


