#include "window.h"
#include "os_specific.h"
#include "ui.h"
#include "font.h"
#include "software_renderer.h"
#include "threads.h"
#include "jobs.h"
#include "random.h"
#include "math/maths.h"

#define RAYTRACER_JOB_COUNT 128

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

struct Raytracer_Job {
    struct Raytracer *tracer;
    s64 ray_count;
    s32 y0;
    s32 y1;    
    b8 should_stop;
};

typedef b8(*Panel_Procedure)(Raytracer *, UI *);

enum Panel_Kind {
    PANEL_Perf,
    PANEL_Scene,
    PANEL_Count,
};

struct Panel {
    Panel_Procedure procedure;
    UI_Vector2 position;
    UI_Window_State state;
};

struct Raytracer {   
    // Display
    Window window;
    UI ui;    
    Software_Font font;

    // Job System
    Frame_Buffer frame_buffer[2]; // Used for the actual raytracing, since we only want to swap that whenever a frame has been finished. We may want to swap the UI in between though, so we always need a valid previous frame.
    s32 front_buffer, back_buffer; // Indices into the frame_buffer array.
    Job_System job_system;
    Raytracer_Job jobs[RAYTRACER_JOB_COUNT];

    Hardware_Time frame_start_time;
    Hardware_Time frame_end_time;
    f32 previous_frame_time_seconds;
    s64 frame_ray_count; // Gathered from the local ray count of each thread
    s64 previous_frame_ray_count;
    
    // World
    Camera camera;
    Viewport viewport;

    v3f horizon_color;
    v3f zenith_color;
    Resizable_Array<Sphere> spheres;
    Resizable_Array<Plane>  planes;

    // UI
    Panel panels[PANEL_Count];
    Linked_List<Panel *> panel_stack;
    
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
    tracer->viewport.samples          = 100;
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

static inline
v3f random_sphere(Raytracer *tracer) {
    v3f result = v3f(tracer->random.random_f32(-1.f, 1.f), tracer->random.random_f32(-1.f, 1.f), tracer->random.random_f32(-1.f, 1.f));
    return v3_normalize(result);
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
            result.normal       = v3_normalize(result.point - sphere.origin);
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
v3f color_ray(Raytracer_Job *job, Raytracer *tracer, v3f ray_origin, v3f ray_direction) {
    v3f ray_color   = v3f(1, 1, 1);
    v3f light_color = v3f(0, 0, 0);
    
    for(s32 i = 0; i < tracer->viewport.max_depth; ++i) {        
        ++job->ray_count;

        Ray_Cast_Result result = cast_ray(tracer, ray_origin, ray_direction);
        if(!result.intersection) {
            // Sky background
            f32 a = (ray_direction.y / v3_length(ray_direction) + 1.0f) * 0.5f;
            v3f environment = (1.0f - a) * tracer->horizon_color + a * tracer->zenith_color;
            light_color = light_color + environment * ray_color;
            break;
        }
    
        v3f diffuse_direction = result.normal + random_sphere(tracer);
        if(v3_fuzzy_equals(diffuse_direction, v3f(0.f))) 
            diffuse_direction = result.normal; // Catch degenerate random directions.
        else
            diffuse_direction = v3_normalize(diffuse_direction);

        v3f specular_direction = v3_normalize(v3_reflect(ray_direction, result.normal) + random_sphere(tracer) * (1.0f - result.material->smoothness));
              
        ray_origin    = result.point + result.normal * 0.001f; 
        ray_direction = v3_lerp(diffuse_direction, specular_direction, result.material->metalness);
        
        light_color = light_color + result.material->emission * ray_color;
        ray_color   = result.material->albedo * ray_color;
    }

    return light_color;
}

static
void raytrace_pixel(Raytracer_Job *job, Raytracer *tracer, s32 x, s32 y) {
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
        v3f ray_direction = v3_normalize(tracer->camera.forward + eye_coordinates.x * tracer->camera.right + eye_coordinates.y * tracer->camera.up);

        raw_color = raw_color + color_ray(job, tracer, ray_origin, ray_direction);
    }

    raw_color = raw_color / (f32) tracer->viewport.samples;

    //    
    // Tone map from HDR to LDR
    //
    /*
    raw_color.x = raw_color.x / (raw_color.x + 1.0f);
    raw_color.y = raw_color.y / (raw_color.y + 1.0f);
    raw_color.z = raw_color.z / (raw_color.z + 1.0f);
    */
    
    //
    // Gamma correct the result
    //
    raw_color.x = powf(raw_color.x, tracer->camera.inverse_gamma);
    raw_color.y = powf(raw_color.y, tracer->camera.inverse_gamma);
    raw_color.z = powf(raw_color.z, tracer->camera.inverse_gamma);
    
    //
    // Clamp the result into the byte range
    //
    raw_color.x = clamp(raw_color.x, 0.0f, 1.0f);
    raw_color.y = clamp(raw_color.y, 0.0f, 1.0f);
    raw_color.z = clamp(raw_color.z, 0.0f, 1.0f);
    
    //
    // Mix into the output buffer
    //    
    Color current_color  = { (u8) (raw_color.x * 255.f), (u8) (raw_color.y * 255.f), (u8) (raw_color.z * 255.f), 255 };

    if(tracer->frame_index > 1) {
        Color previous_color = query_frame_buffer(&tracer->frame_buffer[tracer->back_buffer], x, y);
        Color final_color = lerp(previous_color, current_color, 1.f / (f32) tracer->frame_index);
        write_frame_buffer(&tracer->frame_buffer[tracer->front_buffer], x, y, final_color);
    } else {
        write_frame_buffer(&tracer->frame_buffer[tracer->front_buffer], x, y, current_color);
    }
}

static
void raytrace_viewport_area_job(Raytracer_Job *job) {
    for(s32 y = job->y0; y <= job->y1; ++y) {
        for(s32 x = 0; x <= job->tracer->viewport.width_in_pixels - 1; ++x) {
            raytrace_pixel(job, job->tracer, x, y);
        }
        
        if(job->should_stop) break;
    }
}

static
void start_raytrace_frame(Raytracer *tracer) {
    tracer->frame_start_time = os_get_hardware_time();
    tracer->previous_frame_ray_count = tracer->frame_ray_count;
    tracer->frame_ray_count = 0;

    tracer->front_buffer = tracer->back_buffer;
    tracer->back_buffer  = (tracer->front_buffer + 1) % 2;
    set_viewport(tracer);
    
    s32 height_per_job = tracer->window.h / RAYTRACER_JOB_COUNT;
    s32 y0 = 0;
    
    for(s32 i = 0; i < RAYTRACER_JOB_COUNT; ++i) {
        tracer->jobs[i].tracer      = tracer;
        tracer->jobs[i].ray_count   = 0;
        tracer->jobs[i].y0          = y0;
        tracer->jobs[i].y1          = (i + 1 < RAYTRACER_JOB_COUNT) ? (y0 + height_per_job - 1) : (tracer->window.h - 1);
        tracer->jobs[i].should_stop = false;
        spawn_job(&tracer->job_system, { (Job_Procedure) raytrace_viewport_area_job, &tracer->jobs[i] });
        
        y0 = y0 + height_per_job;
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



/* ---------------------------------------------------- UI ---------------------------------------------------- */

static
void create_panel(Raytracer *tracer, Panel_Kind kind, Panel_Procedure procedure, UI_Vector2 position, UI_Window_State state) {
    tracer->panels[kind].procedure = procedure;
    tracer->panels[kind].position  = position;
    tracer->panels[kind].state     = state;
    tracer->panel_stack.add(&tracer->panels[kind]);
}

static
const char *metric(s64 raw, f32 *value) {
    f32 fpraw = (f32) raw;

    const char *metrics[] = { "", "k", "m", "g", "t" };
    s32 metric_index = 0;
    
    while(fpraw >= 1000.0f && metric_index < ARRAY_COUNT(metrics)) {
        fpraw /= 1000.0f;
        ++metric_index;
    }
    
    *value = fpraw;
    return metrics[metric_index];    
}

static
b8 perf_panel(Raytracer *tracer, UI *ui) {
    if(tracer->panels[PANEL_Perf].state == UI_WINDOW_Closed) return false;
    
    tracer->panels[PANEL_Perf].state = ui_push_window(ui, "Performance"_s, UI_WINDOW_Closeable | UI_WINDOW_Collapsable | UI_WINDOW_Draggable | UI_WINDOW_Keep_Body_On_Screen, &tracer->panels[PANEL_Perf].position);

    if(tracer->panels[PANEL_Perf].state != UI_WINDOW_Collapsed) {
        f32 frame_rays, prev_frame_rays;
        const char *frame_rays_metric = metric(tracer->frame_ray_count, &frame_rays);
        const char *prev_frame_rays_metric = metric(tracer->previous_frame_ray_count, &prev_frame_rays);
     
        ui_push_width(ui, UI_SEMANTIC_SIZE_Pixels, 256, 1);
        ui_label(ui, false, UI_FORMAT_STRING(ui, "Frame Index:     %d", tracer->frame_index));
        ui_label(ui, false, UI_FORMAT_STRING(ui, "Frame Progress:  %d%%", (s32) ((1.0f - (f32) get_number_of_incomplete_jobs(&tracer->job_system) / (f32) RAYTRACER_JOB_COUNT) * 100.f)));
        ui_label(ui, false, UI_FORMAT_STRING(ui, "Frame Rays:      %.1f%s", frame_rays, frame_rays_metric));
        ui_label(ui, false, UI_FORMAT_STRING(ui, "Prev Frame Time: %.1fs", tracer->previous_frame_time_seconds));
        ui_label(ui, false, UI_FORMAT_STRING(ui, "Prev Frame Rays: %.1f%s", prev_frame_rays, prev_frame_rays_metric));
        ui_pop_width(ui);
    }

    return ui_pop_window(ui);
}



/* ----------------------------------------------- Entry Point ----------------------------------------------- */

static
void setup_basic_scene(Raytracer *tracer) {
    // These are the five differently colored, differently sized balls
    set_camera(tracer, { -3.2f, 1, 6 }, { 0, -0.06f, 0 });
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
    // nocheckin: What's the difference between smoothness and metalness? Do we need both parameters?

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

static
void destroy_scene(Raytracer *tracer) {
    tracer->spheres.clear();
    tracer->planes.clear();
}

int main() {
    Raytracer tracer{};
    tracer.frame_index = 0;
    
    // Create the output
    create_window(&tracer.window, "Raytracer"_s);
    show_window(&tracer.window);
    create_software_renderer(&tracer.window);

    create_frame_buffer(&tracer.frame_buffer[0], tracer.window.w, tracer.window.h, COLOR_FORMAT_RGB);
    create_frame_buffer(&tracer.frame_buffer[1], tracer.window.w, tracer.window.h, COLOR_FORMAT_RGB);
    tracer.front_buffer = tracer.back_buffer = 0;
    
    // Create the UI
    UI_Callbacks callbacks = { &tracer, draw_ui_text, draw_ui_quad, set_ui_scissors, clear_ui_scissors };
    create_software_font_from_file(&tracer.font, "C:/Windows/Fonts/Consola.ttf"_s, 11, GLYPH_SET_Extended_Ascii);
    create_ui(&tracer.ui, callbacks, UI_Dark_Theme, &tracer.window, tracer.font.underlying);

    create_panel(&tracer, PANEL_Perf, perf_panel, { 1, 1 }, UI_WINDOW_Closed);
    
    // Create the actual raytracer
    create_job_system(&tracer.job_system, os_get_number_of_hardware_threads());
    //setup_basic_scene(&tracer);
    setup_cornell_scene(&tracer);
    
    while(!tracer.window.should_close) {
        Hardware_Time frame_start = os_get_hardware_time();
        update_window(&tracer.window);

        // @Incomplete: Window resizing.
        
        // Prepare the frame.
        {
            begin_ui_frame(&tracer.ui, { 128, 24 });
        }

        // Render one frame.
        {
            // Restart the jobs for the next frame.
            if(get_number_of_incomplete_jobs(&tracer.job_system) == 0) {
                tracer.frame_end_time = os_get_hardware_time();
                if(tracer.frame_index > 0) tracer.previous_frame_time_seconds = (f32) os_convert_hardware_time(tracer.frame_end_time - tracer.frame_start_time, Seconds);
                
                ++tracer.frame_index;
                start_raytrace_frame(&tracer);
            }
            
            // Calculate the total number of rays.
            tracer.frame_ray_count = 0;
            for(Raytracer_Job &job : tracer.jobs) tracer.frame_ray_count += job.ray_count;
            
            // Blit the previous frame into the window because we clear the window every time for the UI.
            blit_frame_buffer(&tracer.frame_buffer[tracer.back_buffer]);
        }

        // Build the UI for this frame.
        {
            // Menu Bar
            ui_toggle_button_with_pointer(&tracer.ui, "Perf"_s, (b8 *) &tracer.panels[PANEL_Perf].state);

            // Panels
            Panel *interacted_panel = null;

            for(Panel *panel : tracer.panel_stack) {
                if(panel->procedure(&tracer, &tracer.ui)) {
                    interacted_panel = panel;
                }
            }

            if(interacted_panel) {
                tracer.panel_stack.remove_value(interacted_panel);
                tracer.panel_stack.add_first(interacted_panel);
            }
        }

        // Display the frame.
        {
            draw_ui_frame(&tracer.ui);
            swap_buffers();
        }
        
        Hardware_Time frame_end = os_get_hardware_time();        
        window_ensure_frame_time(frame_start, frame_end, 60);
    }

    for(Raytracer_Job &job : tracer.jobs) job.should_stop = true;

    destroy_job_system(&tracer.job_system, JOB_SYSTEM_Kill_Workers);
    destroy_scene(&tracer);

    tracer.panel_stack.clear();
    
    destroy_frame_buffer(&tracer.frame_buffer[0]);
    destroy_frame_buffer(&tracer.frame_buffer[1]);
    destroy_software_font(&tracer.font);
    destroy_ui(&tracer.ui);    
    destroy_software_renderer();
    destroy_window(&tracer.window);
    
    return 0;
}
