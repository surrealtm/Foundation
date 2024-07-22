#include "window.h"
#include "software_renderer.h"
#include "os_specific.h"

int main() {
    Window window;
    create_window(&window, "Foundation UI"_s);
    show_window(&window);

    create_software_renderer(&window);

    while(!window.should_close) {
        Hardware_Time frame_start, frame_end;

        frame_start = os_get_hardware_time();
        update_window(&window);
        maybe_resize_back_buffer();

        // Update the UI
        {
        
        }

        // Draw the UI
        {
            clear_frame(Color(100, 100, 100, 255));

            draw_quad(10, 10, 100, 100, Color(230, 100, 80, 255));

            swap_buffers();
        }

        frame_end = os_get_hardware_time();
        window_ensure_frame_time(frame_start, frame_end, 60);
    }

    destroy_software_renderer();
    destroy_window(&window);
    return 0;
}
