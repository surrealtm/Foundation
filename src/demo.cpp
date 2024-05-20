#include "window.h"

int main() {
	Window window;
	create_window(&window, "Hello World"_s);
    set_window_icon(&window, "diffraction.ico"_s);
    show_window(&window);
    
    
	while(!window.should_close) {
        update_window(&window);
	}
    
	destroy_window(&window);
	return 0;
}
