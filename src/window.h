#pragma once

#include "foundation.h"
#include "string_type.h"

#define WINDOW_PLATFORM_STATE_SIZE 16 // This is the highest size of internal platform data needed to be stored, to avoid a memory allocation here (and to avoid platform headers in here...)
#define WINDOW_GRAPHICS_STATE_SIZE 280 // This is the highest size of graphics data needed to be stored, to avoid a memory allocation here.
#define WINDOW_DONT_CARE (-1)

enum Key_Code {
    KEY_None,
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    
    KEY_Comma,
    KEY_Period,
    KEY_Minus,
    KEY_Plus,
    
    KEY_Arrow_Up,
    KEY_Arrow_Down,
    KEY_Arrow_Left,
    KEY_Arrow_Right,
    
    KEY_Enter,
    KEY_Space,
    KEY_Shift,
    KEY_Escape,
    KEY_Menu,
    KEY_Control,
    
    KEY_Backspace,
    KEY_Delete,
    KEY_Tab,
    
    KEY_Page_Up,
    KEY_Page_Down,
    KEY_End,
    KEY_Home,
    
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    
    KEY_COUNT,
};

enum Button_Code {
    BUTTON_None,
    BUTTON_Left,
    BUTTON_Right,
    BUTTON_Middle,
    BUTTON_COUNT,
};

enum Key_Status {
    KEY_Up       = 0x0,
    KEY_Down     = 0x1,
    KEY_Pressed  = 0x2,
    KEY_Repeated = 0x4,
    KEY_Released = 0x8,
    
    KEY_STATUS_PERSISTENT_MASK = 0x1, // Only the bitflags present in this mask persist over the frame boundary.
};

BITWISE(Key_Status);

enum Button_Status {
    BUTTON_Up       = 0x0,
    BUTTON_Down     = 0x1,
    BUTTON_Pressed  = 0x2,
    BUTTON_Released = 0x8,
    
    BUTTON_STATUS_PERSISTENT_MASK = 0x1, // Only the bitflags present in this mask persist over the frame boundary.
};

BITWISE(Button_Status);

enum Text_Input_Event_Type {
    TEXT_INPUT_EVENT_Character,
    TEXT_INPUT_EVENT_Control,
    TEXT_INPUT_EVENT_COUNT,
};

struct Text_Input_Event {
    Text_Input_Event_Type type;
    b8 shift_held;
    b8 control_held;
    b8 menu_held;
    union {
        u32 utf32; // Used for character events
        Key_Code control; // Used for control events
    };
};

enum Window_Style_Flags {
    WINDOW_STYLE_Default        = 0x1,
    WINDOW_STYLE_Hide_Title_Bar = 0x2,
    WINDOW_STYLE_Maximized      = 0x4,
};

typedef void (*Window_Resize_Callback)(void *);

struct Window {
    u8 platform_data[WINDOW_PLATFORM_STATE_SIZE]; // Used by the different OS implementations (win32, linux) for platform dependent handles.
    u8 graphics_data[WINDOW_GRAPHICS_STATE_SIZE]; // Used by the different graphics backends (d3d11) for graphics handles.

    void *callback_during_resize_user_pointer;
    Window_Resize_Callback callback_during_resize;
    
    s32 x, y, w, h;
    
    b8 should_close,
        maximized,
        focused,
        resized_this_frame,
        moved_this_frame;
    
    s32 mouse_x, mouse_y; // The pixel position of the cursor inside the window.
    s32 mouse_delta_x, mouse_delta_y; // The pixel delta of mouse movement since the previous frame. 
    s32 raw_mouse_delta_x, raw_mouse_delta_y; // The "raw", undiscretized mouse movement since the previous frame.
    f32 mouse_wheel_turns; // The number of turns of the mouse wheel since the previous frame. Using the touchpad can result in fractions of turns for smoother scrolling.
    b8 mouse_active_this_frame; // Set to true if the mouse is inside the window OR if it is currently being dragged (potentially outside the window region).
    
    Key_Status keys[KEY_COUNT];
    Button_Status buttons[BUTTON_COUNT];
    
    Text_Input_Event text_input_events[16];
    u32 text_input_event_count;
    
    f32 frame_time; // Seconds elapsed since the last time the window was updated
    s64 time_of_last_update; // Hardware time of the last call to update_window
};

b8 create_window(Window *window, string title, s32 x = WINDOW_DONT_CARE, s32 y = WINDOW_DONT_CARE, s32 w = WINDOW_DONT_CARE, s32 h = WINDOW_DONT_CARE, Window_Style_Flags flags = WINDOW_STYLE_Default);
void update_window(Window *window);
void destroy_window(Window *window);
void show_window(Window *window);

b8 set_window_icon_from_file(Window *window, string file_path);
b8 set_window_icon_from_resource_name(Window *window, string resource_name);
void set_window_name(Window *window, string name);
void set_window_position_and_size(Window *window, s32 x, s32 y, s32 w, s32 h, b8 maximized);
void set_window_style(Window *window, Window_Style_Flags style_flags);
void set_cursor_position(Window *window, s32 x, s32 y);

void get_desktop_bounds(s32 *x0, s32 *y0, s32 *x1, s32 * y1);
void hide_cursor();
void show_cursor();
void confine_cursor(s32 x0, s32 y0, s32 x1, s32 y1);
void unconfine_cursor();

void window_sleep(f32 seconds);
void window_ensure_frame_time(s64 frame_start, s64 frame_end, f32 requested_fps);


void set_clipboard_data(Window *window, string data); // Pretty sure linux requires the window here...
string get_clipboard_data(Window *window, Allocator *allocator);
void deallocate_clipboard_data(Allocator *allocator, string *data);


struct Window_Buffer {
    s32 width, height;
    u8 *pixels;
};

void acquire_window_buffer(Window *window, Window_Buffer *buffer);
void destroy_window_buffer(Window_Buffer *buffer);
void clear_window_buffer(Window_Buffer *buffer, u8 r, u8 g, u8 b);
void paint_window_buffer(Window_Buffer *buffer, s32 x, s32 y, u8 r, u8 g, u8 b);
void query_window_buffer(Window_Buffer *buffer, s32 x, s32 y, u8 *r, u8 *g, u8 *b);
void blit_window_buffer(Window *window, Window_Buffer *buffer);
void blit_pixels_to_window(Window *window, u8 *pixels, s32 width, s32 height);
u8 *convert_window_buffer_to_rgba(Window_Buffer *buffer);



#if FOUNDATION_WIN32
void *window_extract_hwnd(Window *window);
#endif
