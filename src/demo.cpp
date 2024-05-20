#include "window.h"

int main() {
	Window window;
	create_window(&window, "Hello World"_s);
    set_window_icon(&window, "diffraction.ico"_s);
    show_window(&window);
    
    Window_Buffer buffer;
    acquire_window_buffer(&window, &buffer);
    clear_window_buffer(&buffer, 200, 100, 150);
    
    s32 x = 0, y = 0;
	while(!window.should_close) {
        update_window(&window);
        
        if(y < window.h) paint_window_buffer(&buffer, x, y, 255, 255, 255);
        
        blit_window_buffer(&window, &buffer);
        
        ++x;
        if(x == window.w) {
            x = 0;
            ++y;
        }
    }
    
	destroy_window(&window);
	return 0;
}
