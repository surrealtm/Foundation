#include "window.h"
#include "d3d11.h"
#include "math/v3.h"
#include "os_specific.h"

int main() {
    enable_high_resolution_clock();

	Window window;
	create_window(&window, "Hello World"_s);
    set_window_icon(&window, "diffraction.ico"_s);
    show_window(&window);

    create_d3d11_context(&window);
    
    Frame_Buffer *default_frame_buffer = get_default_frame_buffer(&window);

    Frame_Buffer my_frame_buffer;
    create_frame_buffer(&my_frame_buffer);
    create_frame_buffer_color_attachment(&my_frame_buffer, window.w, window.h);
    create_frame_buffer_depth_stencil_attachment(&my_frame_buffer, window.w, window.h);
    
    Pipeline_State pipeline_state = { false, false, false, false };
    create_pipeline_state(&pipeline_state);

    f32 vertices[] = { -.5, .5,    .5, .5,    -.5, -.5,
                       -.5, -.5,   .5, .5,     .5, -.5 };
    f32 uvs[] = { 0, 0,   1, 0,    0, 1,
                  0, 1,   1, 0,    1, 1 };

    Vertex_Buffer_Array vertex_buffer;
    create_vertex_buffer_array(&vertex_buffer, VERTEX_BUFFER_Triangles);
    add_vertex_data(&vertex_buffer, vertices, ARRAY_COUNT(vertices), 2);
    add_vertex_data(&vertex_buffer, uvs, ARRAY_COUNT(uvs), 2);

    v3f color = v3f(1, 0, 0);
    Shader_Constant_Buffer constants;
    create_shader_constant_buffer(&constants, 0, sizeof(v3f), &color);
    
    Shader_Input_Specification inputs[] = {
        { "POSITION", 2, 0 },
        { "UV", 2, 1 },
    };
    
    Shader shader;
    create_shader_from_file(&shader, "data\\shader\\rgba.hlsl"_s, inputs, ARRAY_COUNT(inputs));

    Texture texture;
    create_texture_from_file(&texture, "data\\textures\\rock.png"_s);
    
    f32 total_time = 0.f;
    
	while(!window.should_close) {
        Hardware_Time frame_start = os_get_hardware_time();

        {
            update_window(&window);

            color.x = cosf(total_time) * 0.5f + 0.5f;
            color.y = sinf(total_time) * 0.5f + 0.5f;
            total_time += window.frame_time;
            update_shader_constant_buffer(&constants, &color);
            
            bind_frame_buffer(&my_frame_buffer);
            clear_frame_buffer(&my_frame_buffer, .1f, .1f, .1f);
            
            bind_shader(&shader);
            bind_shader_constant_buffer(&constants, SHADER_Pixel);
            bind_vertex_buffer_array(&vertex_buffer);
            bind_pipeline_state(&pipeline_state);
            bind_texture(&texture, 0);
            draw_vertex_buffer_array(&vertex_buffer);

            blit_frame_buffer(default_frame_buffer, &my_frame_buffer);

            swap_d3d11_buffers(&window);
        }
        
        Hardware_Time frame_end = os_get_hardware_time();
        window_ensure_frame_time(frame_start, frame_end, 60);
    }

    destroy_texture(&texture);
    destroy_pipeline_state(&pipeline_state);
    destroy_shader_constant_buffer(&constants);
    destroy_shader(&shader);
    destroy_vertex_buffer_array(&vertex_buffer);
    destroy_frame_buffer(&my_frame_buffer);
    
    destroy_d3d11_context(&window);
	destroy_window(&window);
	return 0;
}
