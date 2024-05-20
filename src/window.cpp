#include "window.h"
#include "memutils.h"
#include "os_specific.h" // For win32_last_error_to_string

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720
#define DEFAULT_WINDOW_BACKGROUND_COLOR_r 30
#define DEFAULT_WINDOW_BACKGROUND_COLOR_g 30
#define DEFAULT_WINDOW_BACKGROUND_COLOR_b 30

#if FOUNDATION_WIN32
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>
# include <shellscalingapi.h>

#define WIN32_CLASS_NAME "Foundation"

static b8 win32_class_registered = false;

struct Window_Win32_State {
    HWND hwnd;
    HDC dc;
};

static_assert(sizeof(Window_Win32_State) <= sizeof(Window::platform_data), "Window_Win32_State is bigger than expected.");

static
Key_Code win32_key_map(WPARAM vk) {
    if(vk >= 'A' && vk <= 'Z') return (Key_Code) (vk - 'A' + KEY_A);
    if(vk >= '0' && vk <= '9') return (Key_Code) (vk - '0' + KEY_0);
    if(vk >= VK_F1 && vk <= VK_F12) return (Key_Code) (vk - VK_F1 + KEY_F1);

    Key_Code code;

    switch(vk) {
    case VK_OEM_COMMA:  code = KEY_Comma;  break;
    case VK_OEM_PERIOD: code = KEY_Period; break;
    case VK_OEM_MINUS:  code = KEY_Minus;  break;
    case VK_OEM_PLUS:   code = KEY_Plus;   break;

    case VK_DOWN:  code = KEY_Arrow_Down;  break;
    case VK_UP:    code = KEY_Arrow_Up;    break;
    case VK_LEFT:  code = KEY_Arrow_Left;  break;
    case VK_RIGHT: code = KEY_Arrow_Right; break;

    case VK_RETURN:  code = KEY_Enter;   break;
    case VK_SPACE:   code = KEY_Space;   break;
    case VK_SHIFT:   code = KEY_Shift;   break;
    case VK_ESCAPE:  code = KEY_Escape;  break;
    case VK_MENU:    code = KEY_Menu;    break;
    case VK_CONTROL: code = KEY_Control; break;

    case VK_BACK:   code = KEY_Backspace; break;
    case VK_DELETE: code = KEY_Delete;    break;
    case VK_TAB:    code = KEY_Tab;       break;

    case VK_PRIOR: code = KEY_Page_Up;   break;
    case VK_NEXT:  code = KEY_Page_Down; break;
    case VK_END:   code = KEY_End;       break;
    case VK_HOME:  code = KEY_Home;      break;

    default: code = KEY_None; break;
    }
    
    return code;
}

static
void win32_add_character_text_input_event(Window *window, u32 utf32) {
    if(window->text_input_event_count >= ARRAY_COUNT(window->text_input_events)) return; // If the user somehow maanges to do more text inputs than that in one single frame, then just ignore those inputs.

    Text_Input_Event *event = &window->text_input_events[window->text_input_event_count];
    event->type         = TEXT_INPUT_EVENT_Character;
    event->utf32        = utf32;
    event->shift_held   = window->keys[KEY_Shift]   & KEY_Down;
    event->control_held = window->keys[KEY_Control] & KEY_Down;
    event->menu_held    = window->keys[KEY_Menu]    & KEY_Down;
    ++window->text_input_event_count;
}

static
void win32_add_control_text_input_event(Window *window, Key_Code key) {
    if(window->text_input_event_count >= ARRAY_COUNT(window->text_input_events)) return; // If the user somehow maanges to do more text inputs than that in one single frame, then just ignore those inputs.

    Text_Input_Event *event = &window->text_input_events[window->text_input_event_count];
    event->type         = TEXT_INPUT_EVENT_Control;
    event->control      = key;
    event->shift_held   = window->keys[KEY_Shift]   & KEY_Down;
    event->control_held = window->keys[KEY_Control] & KEY_Down;
    event->menu_held    = window->keys[KEY_Menu]    & KEY_Down;
    ++window->text_input_event_count;
}

static
b8 win32_register_raw_mouse_input(HWND hwnd) {
    RAWINPUTDEVICE device;
    device.usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
    device.usUsage     = 0x02; // HID_USAGE_GENERIC_MOUSE
    device.dwFlags     = RIDEV_INPUTSINK;
    device.hwndTarget  = hwnd;
    b8 success = RegisterRawInputDevices(&device, 1, sizeof(RAWINPUTDEVICE));
    
    if(!success) foundation_error("Failed to register win32 raw mouse input: '%s'.", win32_last_error_to_string());
    
    return success;
}

// When the mouse leaves the window's client area, the window no longer gets mouse move messages, which leads
// the window to think the mouse is still at inside the window area. The window should however recognize when
// the mouse is no longer hovering the window, since that information is important for e.g. UI.
static
b8 win32_register_mouse_tracking(HWND hwnd) {
    TRACKMOUSEEVENT track_mouse_event;
    track_mouse_event.cbSize      = sizeof(TRACKMOUSEEVENT);
    track_mouse_event.dwFlags     = TME_HOVER | TME_LEAVE;
    track_mouse_event.hwndTrack   = hwnd;
    track_mouse_event.dwHoverTime = HOVER_DEFAULT;
    b8 success = TrackMouseEvent(&track_mouse_event);

    if(!success) foundation_error("Failed to track mouse event: '%s'.", win32_last_error_to_string());

    return success;
}

static
LRESULT CALLBACK win32_callback(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    Window *window = (Window *) GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    if(!window) return DefWindowProcA(hwnd, message, wparam, lparam); // Win32 might send us messages during the CreateWindow procedure, at which point we have not yet had the chance to set the window user pointer up.

    LRESULT result = 0;

    switch(message) {
    case WM_CLOSE:
        window->should_close = true;
        break;

    case WM_SIZE:
        window->resized_this_frame = true;
        window->w         = (lparam & 0x0000ffff) >> 0;
        window->h         = (lparam & 0xffff0000) >> 16;
        window->maximized = wparam == SIZE_MAXIMIZED;
        break;

    case WM_MOVE:
        window->moved_this_frame = true;
        window->x = (lparam & 0x0000ffff) >> 0;
        window->y = (lparam & 0xffff0000) >> 16;
        break;

    case WM_SETFOCUS: {
        window->focused = true;

        // When the window lost focus, keyup messages were not sent to this window (if there were any),
        // meaning that keys which were held down before the window lost focus, and released after, are
        // still considered "down" in this window. Therefore, update the window's keyboard
        // state by going through all virtual key codes, and scanning for the current status.
        for(int i = 0; i < 256; ++i) {
            Key_Code key = win32_key_map(i);
            window->keys[key] = (GetAsyncKeyState(i) != 0) ? KEY_Down : KEY_Up;
        }
    } break;

    case WM_KILLFOCUS:
        window->focused = false;
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        Key_Code key = win32_key_map(wparam);
        Key_Status status = KEY_Down | KEY_Repeated;
        if(!(window->keys[key] & KEY_Down)) status |= KEY_Pressed;
        window->keys[key] = status;
        
        if(key == KEY_F4 && window->keys[KEY_Menu] & KEY_Down)   window->should_close = true; // Allow alt+f4 to close the window again, since syskeydown disables the built in windows thing for this.

        // Some special text handling characters are not included in the WM_CHAR message, since these do
        // not actually procedure characters. They are still tied to text input (similar to backspace, which
        // is handled in WM_CHAR for some reason), therefore register these unicodes manually here.
        if(key == KEY_Arrow_Left || key == KEY_Arrow_Right || key == KEY_Backspace || key == KEY_Delete || key == KEY_Enter || key == KEY_Home || key == KEY_End || key == KEY_V || key == KEY_C || key == KEY_A || key == KEY_X) {
            win32_add_control_text_input_event(window, key);
        }
    } break;

    case WM_KEYUP:
    case WM_SYSKEYUP: {
        Key_Code key = win32_key_map(wparam);
        window->keys[key] = KEY_Released;
    } break;

    case WM_CHAR: {
        wchar_t utf16 = (u16) wparam;

        // Make sure that only characters that are actually printable make it through here. All other input
        // events should be handled as control keys (e.g. backspace, escape...)
        WORD character_type;
        GetStringTypeW(CT_CTYPE1, &utf16, 1, &character_type);
        if(character_type & C1_ALPHA || character_type & C1_UPPER || character_type & C1_LOWER || character_type & C1_DIGIT || character_type & C1_PUNCT || character_type & C1_BLANK) {
            win32_add_character_text_input_event(window, utf16);
        }
    } break;

    case WM_LBUTTONDOWN:
        window->buttons[BUTTON_Left] = BUTTON_Down | BUTTON_Pressed;
        SetCapture(hwnd); // Track the dragging and release events of the mouse.
        break;

    case WM_LBUTTONUP:
        window->buttons[BUTTON_Left] = BUTTON_Released;
        ReleaseCapture();
        break;

    case WM_RBUTTONDOWN:
        window->buttons[BUTTON_Right] = BUTTON_Down | BUTTON_Pressed;
        SetCapture(hwnd); // Track the dragging and release events of the mouse.
        break;

    case WM_RBUTTONUP:
        window->buttons[BUTTON_Right] = BUTTON_Released;
        ReleaseCapture();
        break;

    case WM_MBUTTONDOWN:
        window->buttons[BUTTON_Middle] = BUTTON_Down | BUTTON_Pressed;
        SetCapture(hwnd); // Track the dragging and release events of the mouse.
        break;

    case WM_MBUTTONUP:
        window->buttons[BUTTON_Middle] = BUTTON_Released;
        ReleaseCapture();
        break;

    case WM_MOUSEWHEEL: {
        s16 mouse_wheel_delta = (wparam & 0xffff0000) >> 16;
        window->mouse_wheel_turns = (f32) mouse_wheel_delta / 120.0f;
    } break;

    case WM_MOUSEMOVE: {
        if(!window->mouse_active_this_frame) win32_register_mouse_tracking(hwnd);

        s16 new_mouse_x = (lparam & 0x0000ffff) >> 0;
        s16 new_mouse_y = (lparam & 0xffff0000) >> 16;

        window->mouse_delta_x += new_mouse_x - window->mouse_x;
        window->mouse_delta_y += new_mouse_y - window->mouse_y;
        window->mouse_x = new_mouse_x;
        window->mouse_y = new_mouse_y;
        window->mouse_active_this_frame = true;
    } break;

    case WM_MOUSELEAVE:
        window->mouse_active_this_frame = false;
        break;

    case WM_INPUT: {
        UINT rawinput_size = sizeof(RAWINPUT);
        RAWINPUT rawinput;
        UINT bytes = GetRawInputData((HRAWINPUT) lparam, RID_INPUT, &rawinput, &rawinput_size, sizeof(RAWINPUTHEADER));
        if(bytes == -1) break; // Failed to obtain the raw input data for some reason.

        if(rawinput.header.dwType == RIM_TYPEMOUSE && (rawinput.data.mouse.usFlags & MOUSE_MOVE_RELATIVE) == MOUSE_MOVE_RELATIVE) {
            window->raw_mouse_delta_x += rawinput.data.mouse.lLastX;
            window->raw_mouse_delta_y += rawinput.data.mouse.lLastY;
        }
    } break;

    default:
        result = DefWindowProcA(hwnd, message, wparam, lparam);
        break;
    }
    
    return result;
}

static
b8 win32_register_class() {
    if(win32_class_registered) return true;

    b8 success;

    // Prevent issues on devices which use the windows-scaling thing, which essentially
    // just zooms into our window and makes everything look blurry and sad.
    // This must be called before any window is created.
    success = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    if(success != S_OK) foundation_error("Failed to set current process as dpi aware: '%s'.", win32_last_error_to_string());

    // nocheckin
    WNDCLASSEXA window_class = { 0 };
    window_class.cbSize        = sizeof(WNDCLASSEXA);
    window_class.style         = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc   = win32_callback;
    window_class.lpszClassName = WIN32_CLASS_NAME;
    window_class.hCursor       = LoadCursorA(null, IDC_ARROW);
    window_class.hbrBackground = CreateSolidBrush(RGB(DEFAULT_WINDOW_BACKGROUND_COLOR_r, DEFAULT_WINDOW_BACKGROUND_COLOR_g, DEFAULT_WINDOW_BACKGROUND_COLOR_b));
    success = RegisterClassExA(&window_class);

    DWORD error = GetLastError();

    if(success == 0 && error != ERROR_CLASS_ALREADY_EXISTS)  foundation_error("Failed to register windows class: '%s'.", win32_last_error_to_string());
    
    win32_class_registered = true;
    return true;
}

static
DWORD win32_get_window_style(Window_Style flags) {
    DWORD win32_style = 0;

    if(flags & WINDOW_STYLE_Default) {
        win32_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    }

    if(flags & WINDOW_STYLE_Maximized) {
        win32_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_MAXIMIZE;
    }

    if(flags & WINDOW_STYLE_Hide_Title_Bar) {
        win32_style = WS_POPUP | WS_SYSMENU;
    }
    
    return win32_style;
}

static
void win32_adjust_position_and_size(s32 *x, s32 *y, s32 *w, s32 *h, DWORD style, DWORD exstyle) {
    if(*w == WINDOW_DONT_CARE) *w = DEFAULT_WINDOW_WIDTH;
    if(*h == WINDOW_DONT_CARE) *h = DEFAULT_WINDOW_WIDTH;
    
    RECT rect = { 0, 0, *w, *h };
    AdjustWindowRectEx(&rect, style, false, exstyle);
    *w = rect.right - rect.left;
    *h = rect.top - rect.bottom;

    if(*x != WINDOW_DONT_CARE) {
        rect = RECT { *x, 0, *w, *h };
        AdjustWindowRectEx(&rect, style, false, exstyle);
        *x = rect.left;
    } else {
        *x = CW_USEDEFAULT;
    }

    if(*y != WINDOW_DONT_CARE) {
        rect = RECT { 0, *y, *w, *h };
        AdjustWindowRectEx(&rect, style, false, exstyle);
        *y = rect.top;
    } else {
        *y = CW_USEDEFAULT;
    }
}

static
void win32_query_position_and_size(Window *window) {    
    Window_Win32_State *win32 = (Window_Win32_State *) window->platform_data;
    
    RECT client_rect;
    GetClientRect(win32->hwnd, &client_rect);
    window->w = client_rect.right  - client_rect.left;
    window->h = client_rect.bottom - client_rect.top;

    POINT client_position = { 0, 0 };
    ClientToScreen(win32->hwnd, &client_position);
    window->x = client_position.x;
    window->y = client_position.y;
}

static
b8 win32_create_window(Window *window, string title, s32 x, s32 y, s32 w, s32 h, Window_Style flags) {
    if(!win32_register_class()) return false;

    char *cstring = to_cstring(Default_Allocator, title);
    defer { free_cstring(Default_Allocator, cstring); };
    
    Window_Win32_State *win32 = (Window_Win32_State *) window->platform_data;

    // Figure out the properties for this window and create it.
    DWORD exstyle = 0;
    DWORD style = win32_get_window_style(flags);
    win32_adjust_position_and_size(&x, &y, &w, &h, exstyle, style);

    win32->hwnd = CreateWindowExA(exstyle, WIN32_CLASS_NAME, cstring, style, x, y, w, h, null, null, null, null);
    if(win32->hwnd == INVALID_HANDLE_VALUE) {
        foundation_error("Failed to create window: '%s'.", win32_last_error_to_string());
        return false;
    }

    SetWindowLongPtrA(win32->hwnd, GWLP_USERDATA, (LONG_PTR) window);
    win32->dc = GetDC(win32->hwnd);

    // Register custom win32 behaviour.
    win32_register_raw_mouse_input(win32->hwnd);
    win32_register_mouse_tracking(win32->hwnd);
    
    // Set the initial window attributes.
    POINT mouse;
    GetCursorPos(&mouse);

    win32_query_position_and_size(window);
    window->focused = GetFocus() == win32->hwnd;
    window->mouse_x = mouse.x;
    window->mouse_y = mouse.y;
    
    return true;
}

static inline
void win32_destroy_window(Window *window) {
    Window_Win32_State *win32 = (Window_Win32_State *) window->platform_data;
    ReleaseDC(win32->hwnd, win32->dc);
    DestroyWindow(win32->hwnd);
    win32->hwnd = null;
    win32->dc   = null;
}

#endif

b8 create_window(Window *window, string title, s32 x, s32 y, s32 w, s32 h, Window_Style flags) {
    memset(window->keys, 0, sizeof(window->keys));
    memset(window->buttons, 0, sizeof(window->buttons));
    window->frame_time             = 0.f;
    window->resized_this_frame     = false;
    window->moved_this_frame       = false;
    window->should_close           = false;
    window->mouse_delta_x          = 0;
    window->mouse_delta_y          = 0;
    window->raw_mouse_delta_x      = 0;
    window->raw_mouse_delta_y      = 0;
    window->mouse_wheel_turns      = 0;
    window->text_input_event_count = 0;
    window->time_of_last_update    = os_get_hardware_time();
    
#if FOUNDATION_WIN32
    return win32_create_window(window, title, x, y, w, h, flags);
#endif
}

void destroy_window(Window *window) {
#if FOUNDATION_WIN32
    win32_destroy_window(window);
#endif
}

void show_window(Window *window) {
#if FOUNDATION_WIN32
    Window_Win32_State *win32 = (Window_Win32_State *) window->platform_data;
    DWORD command = SW_SHOW;
    if(window->maximized) command = SW_SHOWMAXIMIZED;
    ShowWindow(win32->hwnd, command);
    win32_query_position_and_size(window);
#endif
}

void update_window(Window *window) {
    //
    // Calculate the frame time.
    //
    Hardware_Time update_time = os_get_hardware_time();
    window->frame_time = (f32) os_convert_hardware_time(update_time - window->time_of_last_update, Seconds);
    window->time_of_last_update = update_time;

    //
    // Reset the per-frame window input state.
    //
    window->resized_this_frame     = false;
    window->moved_this_frame       = false;
    window->mouse_delta_x          = 0;
    window->mouse_delta_y          = 0;
    window->raw_mouse_delta_x      = 0;
    window->raw_mouse_delta_y      = 0;
    window->mouse_wheel_turns      = 0;
    window->text_input_event_count = 0;

    for(s64 i = 0; i < KEY_COUNT; ++i) window->keys[i] = window->keys[i] & KEY_STATUS_PERSISTENT_MASK;
    for(s64 i = 0; i < BUTTON_COUNT; ++i) window->buttons[i] = window->buttons[i] & BUTTON_STATUS_PERSISTENT_MASK;

#if FOUNDATION_WIN32
    //
    // Handle all win32 messages.
    //
    Window_Win32_State *win32 = (Window_Win32_State *) window->platform_data;

    MSG msg;
    while(PeekMessageA(&msg, win32->hwnd, 0, 0, 1)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
#endif
}
