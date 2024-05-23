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
    
    Pipeline_State pipeline_state = { false, false, false, false };
    create_pipeline_state(&pipeline_state);

    f32 vertices[] = { -.5, -.5,    .5, -.5,    -.5, .5 };
    f32 uvs[] = { 1, 1,   0, 1,    1, 0 };

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

    f32 total_time = 0.f;
    
	while(!window.should_close) {
        Hardware_Time frame_start = os_get_hardware_time();

        {
            update_window(&window);

            color.x = cosf(total_time) * 0.5 + 0.5;
            color.y = sinf(total_time) * 0.5 + 0.5;
            total_time += window.frame_time;
            update_shader_constant_buffer(&constants, &color);
            
            clear_d3d11_buffer(&window, 200, 200, 100);
            
            bind_shader(&shader);
            bind_shader_constant_buffer(&constants, SHADER_Pixel);
            bind_vertex_buffer_array(&vertex_buffer);
            bind_pipeline_state(&pipeline_state);
            draw_vertex_buffer_array(&vertex_buffer);

            swap_d3d11_buffers(&window);
        }
        
        Hardware_Time frame_end = os_get_hardware_time();
        window_ensure_frame_time(frame_start, frame_end, 60);
    }

    destroy_pipeline_state(&pipeline_state);
    destroy_shader_constant_buffer(&constants);
    destroy_shader(&shader);
    destroy_vertex_buffer_array(&vertex_buffer);

    destroy_d3d11_context(&window);
	destroy_window(&window);
	return 0;
}
