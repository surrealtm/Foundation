#include "window.h"
#include "font.h"
#include "software_renderer.h"
#include "ui.h"
#include "os_specific.h"
#include "catalog.h"

struct Demo {
    Window window;
    Software_Font software_font;
    UI ui;

    Catalog<Texture> texture_catalog;
};

static b8 DARK_THEME;
static b8 WINDOW_OPEN;

static
void draw_ui_text(void *user_pointer, string text, UI_Vector2 position, UI_Color foreground, UI_Color background) {
    Demo *demo = (Demo *) user_pointer;
    draw_text(&demo->software_font, text, (s32) position.x, (s32) position.y, TEXT_ALIGNMENT_Left | TEXT_ALIGNMENT_Bottom, { foreground.r, foreground.g, foreground.b, foreground.a });
}

static
void draw_ui_quad(void *demo, UI_Vector2 top_left, UI_Vector2 bottom_right, f32 rounding, UI_Color color) {
    draw_quad((s32) top_left.x, (s32) top_left.y, (s32) bottom_right.x, (s32) bottom_right.y, { color.r, color.g, color.b, color.a });
}

static
void set_ui_scissors(void *demo, UI_Rect rect) {
    set_scissors((s32) rect.x0, (s32) rect.y0, (s32) rect.x1, (s32) rect.y1);
}

static
void clear_ui_scissors(void *demo) {
    clear_scissors();
}

static
void demo_window(UI *ui) {
    UI_Vector2 position = { 1, 0 };
    UI_Window_State state = ui_push_window(ui, "Window"_s, UI_WINDOW_Draggable | UI_WINDOW_Collapsable | UI_WINDOW_Closeable, &position);

    if(state != UI_WINDOW_Collapsed) {    
        ui_push_width(ui, UI_SEMANTIC_SIZE_Pixels, 256, 1);
        ui_label(ui, false, "This is a window!"_s);
        ui_toggle_button(ui, "Toggle Me"_s);
        ui_check_box(ui, "Dark Theme"_s, &DARK_THEME);
        ui_slider(ui, "Slide Me"_s, 0, 1);
        ui_text_input(ui, "Input Me"_s, TEXT_INPUT_Everything);
        
        ui_spacer(ui);
        
        if(ui_push_collapsable(ui, "Data"_s, true)) {
            ui_text_input(ui, "Some Value"_s, TEXT_INPUT_Floating_Point);
            ui_text_input(ui, "Some Text"_s, TEXT_INPUT_Everything);
        }
        ui_pop_collapsable(ui);
    
        if(ui_push_collapsable(ui, "Options"_s, false)) {
            ui_check_box(ui, "Hidden Toggle"_s, &DARK_THEME);
            ui_slider(ui, "Hidden Slider"_s, -1, 1);
            ui_button(ui, "Hidden Button"_s);
        }
        ui_pop_collapsable(ui);

        ui_divider(ui, true);
        
        ui_set_height(ui, UI_SEMANTIC_SIZE_Pixels, 150, 1);
        ui_push_scroll_view(ui, "ScrollContent"_s, UI_DIRECTION_Vertical);
        for(s32 i = 0; i < 4; ++i) {
            if(ui_button(ui, UI_FORMAT_STRING(ui, "Content: %d", i))) printf("Content Button: %d\n", i);
        }
        ui_pop_scroll_view(ui);
        ui_pop_width(ui);
    }
    ui_pop_window(ui);
    
    if(state == UI_WINDOW_Closed) {
        WINDOW_OPEN = false;
    }
}

static
void demo_ui(UI *ui) {
    if(ui_button(ui, "Hello!"_s)) printf("Hello!\n");
    
    ui_toggle_button_with_pointer(ui, "Window?"_s, &WINDOW_OPEN);

    if(ui_push_dropdown(ui, "Hover me!"_s)) {
        if(ui_button(ui, "Dropped Button"_s)) printf("Dropped button\n");
        
        if(ui_push_dropdown(ui, "Dropped Dropdown"_s)) {
            if(ui_button(ui, "Twice dropped button"_s)) printf("Twice dropped button\n");
            if(ui_button(ui, "Thrice dropped button"_s)) printf("Thrice dropped button\n");
        }
        ui_pop_dropdown(ui);
    }
    ui_pop_dropdown(ui);

    ui_text_input(ui, "Enter something..."_s, TEXT_INPUT_Everything);
    ui_text_input(ui, "Enter a number..."_s,  TEXT_INPUT_Floating_Point);

    if(WINDOW_OPEN) demo_window(ui);
}

int main() {
    Demo demo;
    
    create_temp_allocator(64 * ONE_MEGABYTE);
    create_window(&demo.window, "Foundation UI"_s);
    show_window(&demo.window);

    create_software_renderer(&demo.window, &temp);
    create_software_font_from_file(&demo.software_font, "C:/Windows/fonts/segoeui.ttf"_s, 12, GLYPH_SET_Extended_Ascii);

    UI_Callbacks callbacks = { &demo, draw_ui_text, draw_ui_quad, set_ui_scissors, clear_ui_scissors };
    create_ui(&demo.ui, callbacks, UI_Dark_Theme, &demo.window, demo.software_font.underlying);

    demo.texture_catalog.create_from_file_system("C:/source/Foundation/data/textures"_s, ".png"_s, create_texture_from_memory, null, destroy_texture);
    
    Texture *texture = demo.texture_catalog.query("door_inactive"_s);
    
    while(!demo.window.should_close) {
        Hardware_Time frame_start, frame_end;

        frame_start = os_get_hardware_time();
        update_window(&demo.window);
        maybe_resize_back_buffer();
        begin_ui_frame(&demo.ui, { 128, 25 });

#if FOUNDATION_DEVELOPER
        demo.texture_catalog.check_for_reloads();
#endif

        // Update the UI
        {
            demo_ui(&demo.ui);
        }

        // Draw the UI
        {
            clear_frame(Color(100, 100, 100, 255));
            draw_quad(64, 64, 128, 128, texture);
            draw_ui_frame(&demo.ui);            
            swap_buffers();
        }

        release_temp_allocator();

        frame_end = os_get_hardware_time();
        window_ensure_frame_time(frame_start, frame_end, 60);
    }

    demo.texture_catalog.release(texture);
    demo.texture_catalog.destroy();

    destroy_ui(&demo.ui);
    destroy_software_font(&demo.software_font);
    destroy_software_renderer();
    destroy_window(&demo.window);

    Default_Allocator->print_stats();

    return 0;
}
