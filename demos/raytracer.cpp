#include "window.h"
#include "os_specific.h"
#include "ui.h"
#include "font.h"
#include "software_renderer.h"
#include "math/maths.h"

struct Viewport {
    s32 width_in_pixels;
    s32 height_in_pixels;
    f32 aspect_ratio;
};

struct Camera {
    f32 vertical_fov;
    v3f position;
    v3f rotation;

    v3f forward;
    v3f right;
    v3f up;
};

struct Raytracer {   
    // Display
    Window window;
    UI ui;    
    Software_Font font;

    // World
    Camera camera;
    Viewport viewport;
};



/* ---------------------------------------------- Setup Code ---------------------------------------------- */

static
void set_camera(Raytracer *tracer, v3f position, v3f rotation) {
    tracer->camera.vertical_fov = degrees_to_turns(60.0f);
    tracer->camera.position     = position;
    tracer->camera.rotation     = rotation;
    
    qtf quat = qt_from_euler_turns(rotation);
    tracer->camera.forward = qt_rotate(quat, v3f(0, 0, -1));
    tracer->camera.right   = qt_rotate(quat, v3f(1, 0, 0));
    tracer->camera.up      = qt_rotate(quat, v3f(0, 1, 0));
}

static
void set_viewport(Raytracer *tracer) {
    tracer->viewport.width_in_pixels  = tracer->window.w;
    tracer->viewport.height_in_pixels = tracer->window.h;
    tracer->viewport.aspect_ratio     = (f32) tracer->viewport.width_in_pixels / (f32) tracer->viewport.height_in_pixels;
}



/* -------------------------------------------- Actual Ray Tracing -------------------------------------------- */

static
Color cast_ray(Raytracer *tracer, v3f ray_origin, v3f ray_direction) {
    return { (u8) (ray_direction.x * 128 + 127), (u8) (ray_direction.y * 128 + 127), 255, 255 };
}

static
void raytrace_pixel(Raytracer *tracer, s32 x, s32 y) {
    Color color = { 255, 255, 255, 255 };

    v2f normalized_device_coordinates = v2f((f32) x, (f32) y) / v2f((f32) tracer->viewport.width_in_pixels, (f32) tracer->viewport.height_in_pixels) - v2f(0.5f, 0.5f);
    v2f eye_coordinates = normalized_device_coordinates * v2f(tracer->viewport.aspect_ratio * tracer->camera.vertical_fov, tracer->camera.vertical_fov);
    
    v3f ray_origin    = tracer->camera.position;
    v3f ray_direction = tracer->camera.forward + eye_coordinates.x * tracer->camera.right - eye_coordinates.y * tracer->camera.up;
    
    color = cast_ray(tracer, ray_origin, ray_direction);
    
    write_frame_buffer(x, y, color);
}

static
void raytrace_viewport_area(Raytracer *tracer, s32 y0, s32 y1) {
    for(s32 y = y0; y <= y1; ++y) {
        for(s32 x = 0; x <= tracer->viewport.width_in_pixels - 1; ++x) {
            raytrace_pixel(tracer, x, y);
        }
    }
}



/* ----------------------------------------------- UI Callbacks ----------------------------------------------- */

static
void draw_ui_text(void *user_pointer, string text, UI_Vector2 top_left, UI_Color foreground, UI_Color background) {
    Raytracer *tracer = (Raytracer *) user_pointer;
    draw_text(&tracer->font, text, (s32) top_left.x, (s32) top_left.y, TEXT_ALIGNMENT_Left | TEXT_ALIGNMENT_Bottom, { foreground.r, foreground.g, foreground.b, foreground.a });
}

static
void draw_ui_quad(void *user_pointer, UI_Vector2 top_left, UI_Vector2 bottom_right, f32 rounding, UI_Color color) {
    draw_quad((s32) top_left.x, (s32) top_left.y, (s32) bottom_right.x, (s32) bottom_right.y, { color.r, color.g, color.b, color.a });
}

static
void set_ui_scissors(void *user_pointer, UI_Rect rect) {
    set_scissors((s32) rect.x0, (s32) rect.y0, (s32) rect.x1, (s32) rect.y1);
}

static
void clear_ui_scissors(void *user_pointer) {
    clear_scissors();
}



/* ----------------------------------------------- Entry Point ----------------------------------------------- */

int main() {
    Raytracer tracer;
    
    create_window(&tracer.window, "Raytracer"_s);
    show_window(&tracer.window);
    create_software_renderer(&tracer.window);

    create_software_font_from_file(&tracer.font, "C:/Windows/fonts/segoeui.ttf"_s, 13, GLYPH_SET_Extended_Ascii);

    UI_Callbacks callbacks = { &tracer, draw_ui_text, draw_ui_quad, set_ui_scissors, clear_ui_scissors };
    create_ui(&tracer.ui, callbacks, UI_Dark_Theme, &tracer.window, tracer.font.underlying);
    
    while(!tracer.window.should_close) {
        Hardware_Time frame_start = os_get_hardware_time();
        update_window(&tracer.window);

        // Prepare the frame.
        {
            begin_ui_frame(&tracer.ui, { 128, 24 });
        }

        // Render one frame.
        {
            set_camera(&tracer, v3f(0, 0, 0), v3f(0, 0, 0));
            set_viewport(&tracer);
            raytrace_viewport_area(&tracer, 0, tracer.viewport.height_in_pixels - 1);
        }

        // Build the UI for this frame.
        {
            if(ui_button(&tracer.ui, "Hello"_s)) printf("Pressed the button.\n");
        }

        // Display the frame.
        {
            draw_ui_frame(&tracer.ui);
            swap_buffers();
        }

        Hardware_Time frame_end = os_get_hardware_time();        
        window_ensure_frame_time(frame_start, frame_end, 60);
    }

    destroy_ui(&tracer.ui);    
    destroy_software_font(&tracer.font);
    destroy_software_renderer();
    destroy_window(&tracer.window);
    
    return 0;
}