#include "window.h"
#include "font.h"
#include "software_renderer.h"
#include "ui.h"
#include "os_specific.h"

struct Demo {
    Window window;
    Software_Font software_font;
    UI ui;
};

static
void draw_ui_text(void *user_pointer, string text, UI_Vector2 position, UI_Color foreground, UI_Color background) {
    Demo *demo = (Demo *) user_pointer;
    draw_text(&demo->software_font, text, (s32) position.x, (s32) position.y, TEXT_ALIGNMENT_Left, { foreground.r, foreground.g, foreground.b, foreground.a });
}

static
void draw_ui_quad(void *demo, UI_Vector2 top_left, UI_Vector2 bottom_right, f32 rounding, UI_Color color) {
    // @Incomplete
}

static
void set_ui_scissors(void *demo, UI_Rect rect) {
    // @Incomplete
}

static
void clear_ui_scissors(void *demo) {
    // @Incomplete
}

static
void demo_window(Demo *demo) {
    ui_element(&demo->ui, "Hello"_s, UI_Background | UI_Label | UI_Clickable | UI_Center_Label);
    ui_element(&demo->ui, "Click Me!"_s, UI_Background | UI_Label | UI_Clickable | UI_Center_Label);
}

int main() {
    Demo demo;

    create_window(&demo.window, "Foundation UI"_s);
    show_window(&demo.window);

    create_software_renderer(&demo.window);
    create_software_font_from_file(&demo.software_font, "C:/Windows/fonts/segoeui.ttf"_s, 17, GLYPH_SET_Extended_Ascii);

    UI_Callbacks callbacks = { &demo, draw_ui_text, draw_ui_quad, set_ui_scissors, clear_ui_scissors };
    create_ui(&demo.ui, callbacks, UI_Dark_Theme, &demo.window, demo.software_font.underlying);

    while(!demo.window.should_close) {
        Hardware_Time frame_start, frame_end;

        frame_start = os_get_hardware_time();
        update_window(&demo.window);
        maybe_resize_back_buffer();
        begin_ui_frame(&demo.ui, { 256, 32 });

        // Update the UI
        {
            demo_window(&demo);
        }

        // Draw the UI
        {
            clear_frame(Color(100, 100, 100, 255));
            draw_ui_frame(&demo.ui);            
            swap_buffers();
        }

        frame_end = os_get_hardware_time();
        window_ensure_frame_time(frame_start, frame_end, 60);
    }

    destroy_software_font(&demo.software_font);
    destroy_software_renderer();
    destroy_window(&demo.window);
    return 0;
}
