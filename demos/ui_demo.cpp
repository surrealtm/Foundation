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
void demo_window(Demo *demo) {
    ui_element(&demo->ui, "Hello"_s,     UI_Background | UI_Label | UI_Clickable | UI_Center_Label);
    ui_element(&demo->ui, "Click Me!"_s, UI_Background | UI_Label | UI_Clickable | UI_Center_Label);
}

int main() {
    Demo demo;

    create_window(&demo.window, "Foundation UI"_s);
    show_window(&demo.window);

    create_software_renderer(&demo.window);
    create_software_font_from_file(&demo.software_font, "C:/Windows/fonts/segoeui.ttf"_s, 12, GLYPH_SET_Extended_Ascii);

    UI_Callbacks callbacks = { &demo, draw_ui_text, draw_ui_quad, set_ui_scissors, clear_ui_scissors };
    create_ui(&demo.ui, callbacks, UI_Dark_Theme, &demo.window, demo.software_font.underlying);

    while(!demo.window.should_close) {
        Hardware_Time frame_start, frame_end;

        frame_start = os_get_hardware_time();
        update_window(&demo.window);
        maybe_resize_back_buffer();
        begin_ui_frame(&demo.ui, { 128, 20 });

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
