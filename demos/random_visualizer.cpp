#include "window.h"
#include "software_renderer.h"
#include "os_specific.h"
#include "random.h"

#define BUCKET_COUNT 600
#define BATCH_COUNT  4096

#define UNIFORM      0
#define NORMAL       1
#define LINEAR       2
#define EXPONENTIAL  3

#define DISTRIBUTION LINEAR

int main() {
    Window window;
    create_window(&window, "Hello Windows"_s);
    show_window(&window);
    os_enable_high_resolution_clock();

    create_software_renderer(&window);
    
    Frame_Buffer frame_buffer;
    create_frame_buffer(&frame_buffer, window.w, window.h, 4);

    Random_Generator generator;

    f32 buckets[BUCKET_COUNT] = { 0 };
    f32 max_bucket_value = 0.f;
    
    while(!window.should_close) {
        Hardware_Time frame_start = os_get_hardware_time();
        update_window(&window);

        // Update the random representation
        {
            for(s64 i = 0; i < BATCH_COUNT; ++i) {
#if DISTRIBUTION == UNIFORM
                f32 value = generator.random_f32_zero_to_one();
#elif DISTRIBUTION == NORMAL
                f32 value = generator.random_f32_normal_distribution(0.5f, 0.2f);
#elif DISTRIBUTION == LINEAR
                f32 value = generator.random_f32_linear_distribution(0.f, 1.f);
#elif DISTRIBUTION == EXPONENTIAL
                f32 value = generator.random_f32_exponential_distribution(2.5f);               
#endif

                if(value < 0.f || value > 1.f) continue; // Might happen for the normal distribution, ignore these (so that we don't get garbage for the first and last bucket)
                
                s64 bucket_index = clamp((s32) ceilf(value * (BUCKET_COUNT - 1)), 0, BUCKET_COUNT - 1);
                buckets[bucket_index] += 1.f;

                max_bucket_value = max(max_bucket_value, buckets[bucket_index]);
            }
        }

        // Draw the random representation
        {
            bind_frame_buffer(&frame_buffer);
            clear_frame(Color(50, 100, 200, 255));

            s32 height = BUCKET_COUNT;

            s32 bucket_width = height / BUCKET_COUNT;
            //s32 bucket_width = (window.w - 20) / BUCKET_COUNT; // Make sure every bucket has the same width for better visuals.
            s32 width = BUCKET_COUNT * bucket_width;


            s32 x0 = (window.w - width) / 2, x1 = x0 + width;
            s32 y0 = window.h / 2 - height / 2, y1 = y0 + height;

            draw_quad(x0, y0, x1, y1, Color(50, 50, 50, 200));

            for(s32 i = 0; i < BUCKET_COUNT; ++i) {
                s32 bucket_x0 = x0 + i * bucket_width;
                s32 bucket_x1 = x0 + (i + 1) * bucket_width;
                s32 bucket_y0 = y1 - (s32) (buckets[i] / max_bucket_value * height);
                s32 bucket_y1 = y1;

                u8 color = (u8) (((f32) i / (f32) BUCKET_COUNT) * 255.f);
                draw_quad(bucket_x0, bucket_y0, bucket_x1, bucket_y1, Color(color, color, color, 255));
            }
            
            swap_buffers(&frame_buffer);
        }
        
        Hardware_Time frame_end = os_get_hardware_time();
        window_ensure_frame_time(frame_start, frame_end, 60);
    }

    destroy_frame_buffer(&frame_buffer);
    destroy_software_renderer();
    destroy_window(&window);
    return 0;
}
