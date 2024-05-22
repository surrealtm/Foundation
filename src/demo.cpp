#include "window.h"
#include "d3d11.h"
#include "os_specific.h"

int main() {
    enable_high_resolution_clock();

	Window window;
	create_window(&window, "Hello World"_s);
    set_window_icon(&window, "diffraction.ico"_s);
    show_window(&window);

    create_d3d11_context(&window);
    
    s32 x = 0, y = 0;
	while(!window.should_close) {
        Hardware_Time frame_start = os_get_hardware_time();

        {
            update_window(&window);
            swap_d3d11_buffers(&window);
        }
        
        Hardware_Time frame_end = os_get_hardware_time();
        window_ensure_frame_time(frame_start, frame_end, 60);
    }

    destroy_d3d11_context(&window);
	destroy_window(&window);
	return 0;
}
