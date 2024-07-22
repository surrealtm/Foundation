#include "window.h"

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
