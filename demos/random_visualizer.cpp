#include "window.h"
#include "software_renderer.h"
#include "os_specific.h"
#include "random.h"
#include "memutils.h"

#define BUCKET_COUNT (65536)
#define BATCH_COUNT  4096

#define LOGARITHMIC_SCALE faöse

#define UNIFORM      0
#define NORMAL       1
#define LINEAR       2
#define EXPONENTIAL  3
#define INVERSE      4

#define DISTRIBUTION UNIFORM

int main() {
    Window window;
    create_window(&window, "Hello Windows"_s);
    show_window(&window);
    os_enable_high_resolution_clock();
    create_temp_allocator(4 * ONE_MEGABYTE);

    create_software_renderer(&window);
    
    Frame_Buffer frame_buffer;
    create_frame_buffer(&frame_buffer, window.w, window.h, 4);

    Random_Generator generator;

    f32 *buckets = (f32 *) Default_Allocator->allocate(BUCKET_COUNT * sizeof(f32));
    f32 max_bucket_value = 0.f;
    
    memset(buckets, 0, BUCKET_COUNT * sizeof(f32));

    while(!window.should_close) {
        CPU_Time frame_start = os_get_cpu_time();
        update_window(&window);

        u64 temp_mark = mark_temp_allocator();

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
#elif DISTRIBUTION == INVERSE
                f32 value = generator.random_f32_inverse_distribution();
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

            s32 height = window.h - 20;
            s32 width = height;

            s32 x0 = (window.w - width) / 2, x1 = x0 + width;
            s32 y0 = window.h / 2 - height / 2, y1 = y0 + height;

            draw_quad(x0, y0, x1, y1, Color(50, 50, 50, 200));

            f32 scale = 1;
            
            f32 inverse_max_value = 1.f / max_bucket_value;
            f32 inverse_max_value_log = 1.f / (logf(10.f) * logf(max_bucket_value));

            for(s32 x = 0; x < width; ++x) {
#if LOGARITHMIC_SCALE
                scale *= 1.01f;
#else
                scale += 1;
#endif

                u64 bucket_index = (u64) roundf(scale) - 1;

                if(bucket_index >= BUCKET_COUNT) break;

                f32 value = buckets[bucket_index];

#if LOGARITHMIC_SCALE
                value = logf(value + 1.f) * inverse_max_value_log;
#else
                value = value * inverse_max_value;
#endif

                s32 bucket_x0 = x0 + x;
                s32 bucket_x1 = x0 + x + 1;
                s32 bucket_y0 = y1 - (s32) (value * height);
                s32 bucket_y1 = y1;

                u8 color = (u8) (((f32) bucket_index / (f32) BUCKET_COUNT) * 255.f);
                draw_quad(bucket_x0, bucket_y0, bucket_x1, bucket_y1, Color(color, color, color, 255));
            }
            
            swap_buffers(&frame_buffer);
        }
        
        release_temp_allocator(temp_mark);

        CPU_Time frame_end = os_get_cpu_time();
        window_ensure_frame_time(frame_start, frame_end, 60);
    }

    destroy_frame_buffer(&frame_buffer);
    destroy_software_renderer();
    destroy_window(&window);
    return 0;
}
