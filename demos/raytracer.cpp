#include "window.h"
#include "os_specific.h"
#include "ui.h"
#include "font.h"
#include "software_renderer.h"
#include "random.h"
#include "math/maths.h"

struct Viewport {
    s32 width_in_pixels;
    s32 height_in_pixels;
    f32 aspect_ratio;
    f32 depth;
    s32 samples;
    s32 max_depth;
};

struct Camera {
    f32 vertical_fov;
    v3f position;
    v3f rotation;

    f32 inverse_gamma;

    v3f forward;
    v3f right;
    v3f up;
};

struct Material {
    v3f albedo;
    v3f emission;
    f32 smoothness;
    f32 metalness;
};

struct Sphere {
    v3f origin;
    f32 radius;
    Material material;        
};

struct Plane {
    v3f u;
    v3f v;
    f32 ulength2;
    f32 vlength2;
    v3f normal;
    v3f origin;
    Material material;
};

struct Raytracer {   
    // Display
    Window window;
    UI ui;    
    Software_Font font;

    // World
    Camera camera;
    Viewport viewport;

    v3f horizon_color;
    v3f zenith_color;
    Resizable_Array<Sphere> spheres;
    Resizable_Array<Plane>  planes;

    // Util
    s32 frame_index;
    Random_Generator random;
};



/* ---------------------------------------------- Setup Code ---------------------------------------------- */

static
void set_camera(Raytracer *tracer, v3f position, v3f rotation) {
    tracer->camera.vertical_fov = degrees_to_turns(60.0f);
    tracer->camera.position     = position;
    tracer->camera.rotation     = rotation;

    tracer->camera.inverse_gamma = 1.f / 2.2f;
    
    qtf quat               = qt_from_euler_turns(rotation);
    tracer->camera.forward = qt_rotate(quat, v3f(0, 0, -1));
    tracer->camera.right   = qt_rotate(quat, v3f(1, 0, 0));
    tracer->camera.up      = qt_rotate(quat, v3f(0, 1, 0));
}

static
void set_viewport(Raytracer *tracer) {
    tracer->viewport.width_in_pixels  = tracer->window.w;
    tracer->viewport.height_in_pixels = tracer->window.h;
    tracer->viewport.aspect_ratio     = (f32) tracer->viewport.width_in_pixels / (f32) tracer->viewport.height_in_pixels;
    tracer->viewport.depth            = 100.0f;
    tracer->viewport.samples          = 8;
    tracer->viewport.max_depth        = 20;
}

static
void set_environment(Raytracer *tracer, v3f horizon, v3f zenith) {
    tracer->horizon_color = horizon;
    tracer->zenith_color  = zenith;
}

static
void make_sphere(Raytracer *tracer, f32 radius, v3f origin, v3f albedo, v3f emission, f32 smoothness, f32 metalness) {
    Sphere *sphere   = tracer->spheres.push();
    sphere->radius   = radius;
    sphere->origin   = origin;
    sphere->material = { albedo, emission, smoothness, metalness };
}

static
void make_plane(Raytracer *tracer, v3f origin, v3f u, v3f v, v3f albedo, v3f emission, f32 smoothness, f32 metalness) {
    Plane *plane    = tracer->planes.push();
    plane->origin   = origin;
    plane->u        = u;
    plane->v        = v;
    plane->ulength2 = v3_dot_v3(u, u);
    plane->vlength2 = v3_dot_v3(v, v);
    plane->normal   = v3_normalize(v3_cross_v3(u, v));
    plane->material = { albedo, emission, smoothness, metalness };
}



/* -------------------------------------------- Actual Ray Tracing -------------------------------------------- */

struct Ray_Cast_Result {
    b8 intersection;
    f32 distance;
    v3f point;
    v3f normal;
    Material *material;
};

static
f32 test_ray_sphere(Raytracer *tracer, Sphere *sphere, v3f ray_origin, v3f ray_direction) {
    v3f oo = { sphere->origin.x - ray_origin.x, sphere->origin.y - ray_origin.y, sphere->origin.z - ray_origin.z };
    f32 a = ray_direction.x * ray_direction.x + ray_direction.y * ray_direction.y + ray_direction.z * ray_direction.z;
    f32 h = ray_direction.x * oo.x + ray_direction.y * oo.y + ray_direction.z * oo.z;
    f32 c = oo.x * oo.x + oo.y * oo.y + oo.z * oo.z - sphere->radius * sphere->radius;

    f32 discriminant = h * h - a * c;

    if(discriminant < 0) return tracer->viewport.depth;

    f32 t = (h - sqrtf(discriminant)) / a;

    if(t < 0) return tracer->viewport.depth;

    return t;
}

static
f32 test_ray_plane(Raytracer *tracer, Plane *plane, v3f ray_origin, v3f ray_direction) {
    f32 denom = ray_direction.x * plane->normal.x + ray_direction.y * plane->normal.y + ray_direction.z * plane->normal.z;
    if(denom > -F32_EPSILON) return tracer->viewport.depth; // The ray and the normal must be facing in opposite directions
    
    v3f lane = { plane->origin.x - ray_origin.x, plane->origin.y - ray_origin.y, plane->origin.z - ray_origin.z };
    f32 t = (lane.x * plane->normal.x + lane.y * plane->normal.y + lane.z * plane->normal.z) / denom;
    
    if(t < F32_EPSILON) return tracer->viewport.depth; // The distance along the ray must be positive, or else the plane is behind the camera
    
    v3f intersection_to_origin = { ray_origin.x + ray_direction.x * t - plane->origin.x,
                                   ray_origin.y + ray_direction.y * t - plane->origin.y,
                                   ray_origin.z + ray_direction.z * t - plane->origin.z };
                                   
    f32 u = fabsf(plane->u.x * intersection_to_origin.x + plane->u.y * intersection_to_origin.y + plane->u.z * intersection_to_origin.z);
    f32 v = fabsf(plane->v.x * intersection_to_origin.x + plane->v.y * intersection_to_origin.y + plane->v.z * intersection_to_origin.z);

    if(u > plane->ulength2 || v > plane->vlength2) return tracer->viewport.depth; // The intersection point is outside the actual plane region
    
    return t;
}

static inline
v3f random_hemisphere(Raytracer *tracer, v3f normal) {
    v3f result = v3f(tracer->random.random_f32(-1.f, 1.f), tracer->random.random_f32(-1.f, 1.f), tracer->random.random_f32(-1.f, 1.f));
    f32 denom = sign(v3_dot_v3(result, normal)) / v3_length(result);
    return result * denom;
}

static
Ray_Cast_Result cast_ray(Raytracer *tracer, v3f ray_origin, v3f ray_direction) {
    Ray_Cast_Result result;
    result.intersection = false;
    result.distance     = tracer->viewport.depth;
    
    for(Sphere &sphere : tracer->spheres) {
        f32 distance_to_sphere = test_ray_sphere(tracer, &sphere, ray_origin, ray_direction);
        if(distance_to_sphere < result.distance) {
            result.intersection = true;
            result.distance     = distance_to_sphere;
            result.point        = ray_origin + ray_direction * result.distance;
            result.normal       = -ray_direction;
            result.material     = &sphere.material;
        }
    }
       
    for(Plane &plane : tracer->planes) {
        f32 distance_to_plane = test_ray_plane(tracer, &plane, ray_origin, ray_direction);
        if(distance_to_plane < result.distance) {
            result.intersection = true;
            result.distance     = distance_to_plane;
            result.point        = ray_origin + ray_direction * result.distance;
            result.normal       = plane.normal;
            result.material     = &plane.material;
        }
    }
            
    return result;    
}

static
v3f color_ray(Raytracer *tracer, v3f ray_origin, v3f ray_direction) {
    v3f ray_color = v3f(1, 1, 1);
    v3f light_color = v3f(0, 0, 0);
    
    for(s32 i = 0; i < tracer->viewport.max_depth; ++i) {        
        Ray_Cast_Result result = cast_ray(tracer, ray_origin, ray_direction);
        if(!result.intersection) {
            // Sky background
            f32 a = (ray_direction.y / v3_length(ray_direction) + 1.0f) * 0.5f;
            v3f environment = (1.0f - a) * tracer->horizon_color + a * tracer->zenith_color;
            light_color = light_color + environment * ray_color;
            break;
        }
    
        // Diffuse calculation
        ray_origin = result.point;    
        ray_direction = random_hemisphere(tracer, result.normal);
        
        light_color = light_color + result.material->emission * ray_color;
        ray_color = ray_color * result.material->albedo;
    }

    return light_color;
}

static
void raytrace_pixel(Raytracer *tracer, s32 x, s32 y) {
    //
    // Sum the raw color for this pixel over all samples
    //
    v3f raw_color = v3f(0, 0, 0);
    
    for(s32 i = 0; i < tracer->viewport.samples; ++i) {
        v2f screen_offset = v2f(tracer->random.random_f32(-.5f, .5f), tracer->random.random_f32(-.5f, .5f));
    
        v2f screen_coordinates = v2f((f32) x + screen_offset.x, (f32) y + screen_offset.y) / v2f((f32) tracer->viewport.width_in_pixels, (f32) tracer->viewport.height_in_pixels);
        v2f normalized_device_coordinates = v2f(2.0f * screen_coordinates.x - 1.0f, 1.0f - 2.0f * screen_coordinates.y);
        v2f eye_coordinates = normalized_device_coordinates * v2f(tracer->viewport.aspect_ratio * tracer->camera.vertical_fov * 2.0f, tracer->camera.vertical_fov * 2.0f);
        
        v3f ray_origin    = tracer->camera.position;
        v3f ray_direction = tracer->camera.forward + eye_coordinates.x * tracer->camera.right + eye_coordinates.y * tracer->camera.up;

        raw_color = raw_color + color_ray(tracer, ray_origin, ray_direction);
    }

    raw_color = raw_color / (f32) tracer->viewport.samples;

    //    
    // Tone map from HDR to LDR
    //
    /*
    raw_color.x = clamp(raw_color.x / (raw_color.x + 1.0f), 0.0f, 1.0f);
    raw_color.y = clamp(raw_color.y / (raw_color.y + 1.0f), 0.0f, 1.0f);
    raw_color.z = clamp(raw_color.z / (raw_color.z + 1.0f), 0.0f, 1.0f);
    */
    
    //
    // Gamma correct the result
    //
    raw_color.x = powf(raw_color.x, tracer->camera.inverse_gamma);
    raw_color.y = powf(raw_color.y, tracer->camera.inverse_gamma);
    raw_color.z = powf(raw_color.z, tracer->camera.inverse_gamma);

    //
    // Mix into the output buffer
    //    
    Color current_color  = { (u8) (raw_color.x * 255.f), (u8) (raw_color.y * 255.f), (u8) (raw_color.z * 255.f), 255 };

    if(tracer->frame_index > 1) {
        Color previous_color = query_frame_buffer(x, y);
        Color final_color = lerp(previous_color, current_color, 1.f / (f32) tracer->frame_index);
        write_frame_buffer(x, y, final_color);
    } else {
        write_frame_buffer(x, y, current_color);
    }
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

static
void setup_basic_scene(Raytracer *tracer) {
    // These are the five differently colored, differently sized balls
    set_camera(tracer, { -3, 1, 6 }, { 0, -0.06f, 0 });
    set_environment(tracer, { 1.0f, 1.0f, 1.0f }, { 0.5f, 0.7f, 1.0f });
    make_sphere(tracer, 1.0f, { -3.1f, 1.0f, 0.0f }, { 1.f, .1f, .1f }, { 0, 0, 0 }, 1.0f, 0.1f);
    make_sphere(tracer, 0.7f, { -1.3f, 0.7f, -.4f }, { .2f, 1.f, .2f }, { 0, 0, 0 }, 0.9f, 0.1f);
    make_sphere(tracer, 0.6f, {  0.2f, 0.6f, -.7f }, { .2f, .2f, 1.f }, { 0, 0, 0 }, 0.6f, 0.1f);
    make_sphere(tracer, 0.5f, {  1.3f, 0.5f, -.4f }, { .2f, .2f, .2f }, { 0, 0, 0 }, 0.4f, 0.1f);
    make_sphere(tracer, 0.4f, {  2.3f, 0.4f, -.1f }, { 1.f, 1.f, .2f }, { 0, 0, 0 }, 0.2f, 0.5f);
    make_plane(tracer, { 0, 0, 0 }, { 0, 0, 50 }, { 50, 0, 0 }, { 1.f, .4f, 1.f }, { 0, 0, 0 }, 0.0f, 0.0f);
}

static
void setup_cornell_scene(Raytracer *tracer) {
    // These are four uniform spheres in the cornell box of the following half sizes
    f32 w = 7.1111f; // Keep the 16:9 aspect ratio for the cornell box
    f32 h = 4;
    f32 d = 3;
    f32 l = 2;

    set_camera(tracer, { 0, 0, 10.95f }, { 0, 0, 0 });
    set_environment(tracer, { 1.0f, 1.0f, 1.0f }, { 0.5f, 0.7f, 1.0f });
    make_sphere(tracer, 1, { -4.4f, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0 }, 1.0f, 1.0f);
    make_sphere(tracer, 1, { -2.2f, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0 }, 1.0f, 0.8f);
    make_sphere(tracer, 1, {  0.0f, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0 }, 1.0f, 0.6f);
    make_sphere(tracer, 1, {  2.2f, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0 }, 1.0f, 0.4f);
    make_sphere(tracer, 1, {  4.4f, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0 }, 1.0f, 0.1f);
    make_plane(tracer, {  0,  -h,  0 }, { 0, 0, d }, { w, 0, 0 }, {  1,  1,   1  }, { 0, 0, 0 }, 0, 0); // Floor
    make_plane(tracer, {  0,   h,  0 }, { w, 0, 0 }, { 0, 0, d }, {  1,  1,   1  }, { 0, 0, 0 }, 0, 0); // Ceiling
    make_plane(tracer, {  0,   0, -d }, { w, 0, 0 }, { 0, h, 0 }, {  1,  1,   1  }, { 0, 0, 0 }, 0, 0); // Back wall
    make_plane(tracer, {  0,   0,  d }, { 0, h, 0 }, { w, 0, 0 }, {  0, .4f, .8f }, { 0, 0, 0 }, 0, 0); // Front wall
    make_plane(tracer, { -w,   0,  0 }, { 0, h, 0 }, { 0, 0, d }, { .3f, 1,  .3f }, { 0, 0, 0 }, 0, 0); // Right wall
    make_plane(tracer, {  w,   0,  0 }, { 0, 0, d }, { 0, h, 0 }, {  1, .3f, .3f }, { 0, 0, 0 }, 0, 0); // Left wall
    make_plane(tracer, {  0, h - 0.01f, 0 }, { l, 0, 0 }, { 0, 0, l / 2 }, {  1,  1,  1 }, { 2, 2, 2 }, 0, 0); // Light
}

int main() {
    Raytracer tracer;
    tracer.frame_index = 0;
    
    create_window(&tracer.window, "Raytracer"_s);
    show_window(&tracer.window);
    create_software_renderer(&tracer.window);

    create_software_font_from_file(&tracer.font, "C:/Windows/fonts/segoeui.ttf"_s, 13, GLYPH_SET_Extended_Ascii);

    UI_Callbacks callbacks = { &tracer, draw_ui_text, draw_ui_quad, set_ui_scissors, clear_ui_scissors };
    create_ui(&tracer.ui, callbacks, UI_Dark_Theme, &tracer.window, tracer.font.underlying);

    setup_basic_scene(&tracer);
    
    while(!tracer.window.should_close) {
        Hardware_Time frame_start = os_get_hardware_time();
        update_window(&tracer.window);

        // Prepare the frame.
        {
            begin_ui_frame(&tracer.ui, { 128, 24 });
        }

        // Render one frame.
        {
            ++tracer.frame_index;
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
