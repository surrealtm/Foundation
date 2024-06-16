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
        
    case WM_SIZE: {
        window->resized_this_frame = true;
        window->w         = (lparam & 0x0000ffff) >> 0;
        window->h         = (lparam & 0xffff0000) >> 16;
        window->maximized = wparam == SIZE_MAXIMIZED;
        
        if(window->callback_during_resize) {
            PAINTSTRUCT ps{ 0 };
            BeginPaint(hwnd, &ps);
            window->callback_during_resize(window->callback_during_resize_user_pointer);
            EndPaint(hwnd, &ps);
        }
    } break;
        
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
        if(key == KEY_Arrow_Left || key == KEY_Arrow_Right || key == KEY_Backspace || key == KEY_Delete || key == KEY_Enter || key == KEY_Home || key == KEY_End || key == KEY_Tab) {
            win32_add_control_text_input_event(window, key);
        } else if(window->keys[KEY_Control] & KEY_Down && (key == KEY_V || key == KEY_C || key == KEY_A || key == KEY_X)) {
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
        if((character_type & C1_ALPHA || character_type & C1_UPPER || character_type & C1_LOWER || character_type & C1_DIGIT || character_type & C1_PUNCT || character_type & C1_BLANK) && utf16 != VK_TAB) {
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
    
    WNDCLASSEXA window_class = { 0 };
    window_class.cbSize        = sizeof(WNDCLASSEXA);
    window_class.style         = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc   = win32_callback;
    window_class.lpszClassName = WIN32_CLASS_NAME;
    window_class.hCursor       = LoadCursor(null, IDC_ARROW);
    window_class.hbrBackground = CreateSolidBrush(RGB(DEFAULT_WINDOW_BACKGROUND_COLOR_r, DEFAULT_WINDOW_BACKGROUND_COLOR_g, DEFAULT_WINDOW_BACKGROUND_COLOR_b));
    success = RegisterClassExA(&window_class);
    
    DWORD error = GetLastError();
    
    if(success == 0 && error != ERROR_CLASS_ALREADY_EXISTS)  foundation_error("Failed to register windows class: '%s'.", win32_last_error_to_string());
    
    win32_class_registered = true;
    return true;
}

static
DWORD win32_get_window_style(Window_Style_Flags style_flags) {
    DWORD win32_style = 0;
    
    if(style_flags & WINDOW_STYLE_Default) {
        win32_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    }
    
    if(style_flags & WINDOW_STYLE_Maximized) {
        win32_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_MAXIMIZE;
    }
    
    if(style_flags & WINDOW_STYLE_Hide_Title_Bar) {
        win32_style = WS_POPUP | WS_SYSMENU;
    }
    
    return win32_style;
}

static
void win32_adjust_position_and_size(s32 *x, s32 *y, s32 *w, s32 *h, DWORD style, DWORD exstyle) {
    if(*w == WINDOW_DONT_CARE) *w = DEFAULT_WINDOW_WIDTH;
    if(*h == WINDOW_DONT_CARE) *h = DEFAULT_WINDOW_HEIGHT;
    
    RECT rect = { 0, 0, *w, *h };
    AdjustWindowRectEx(&rect, style, false, exstyle);
    *w = rect.right - rect.left;
    *h = rect.bottom - rect.top;
    
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
b8 win32_create_window(Window *window, string title, s32 x, s32 y, s32 w, s32 h, Window_Style_Flags flags) {
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

#elif FOUNDATION_LINUX
namespace X11 {
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <X11/Xos.h>
};

struct Window_X11_State {
    X11::Display *display;
    s32 screen_number;
    X11::Window window_handle;
    X11::Atom window_delete_atom;
    X11::GC gc;
};

static_assert(sizeof(Window_X11_State) <= sizeof(Window::platform_data), "Window_X11_State is bigger than expected.");

Key_Code x11_key_map(X11::XEvent *keyevent) {
    s32 keysymbol = X11::XLookupKeysym(&keyevent->xkey, 0);
        
    if(keysymbol >= 'A' && keysymbol <= 'Z')       return (Key_Code) (keysymbol - 'A' + KEY_A);
    if(keysymbol >= '0' && keysymbol <= '9')       return (Key_Code) (keysymbol - '0' + KEY_0);
    if(keysymbol >= XK_F1 && keysymbol <= XK_F12)  return (Key_Code) (keysymbol - XK_F1 + KEY_F1);

    Key_Code keycode;

    switch(keysymbol) {
    case XK_comma:  keycode = KEY_Comma; break;
    case XK_period: keycode = KEY_Period; break;
    case XK_minus:  keycode = KEY_Minus; break;
    case XK_plus:   keycode = KEY_Plus; break;
        
    case XK_Down:    keycode = KEY_Arrow_Down; break;
    case XK_Up:      keycode = KEY_Arrow_Up; break;
    case XK_Left:    keycode = KEY_Arrow_Left; break;
    case XK_Right:   keycode = KEY_Arrow_Right; break;

    case XK_Return:   keycode = KEY_Enter; break;
    case XK_space:    keycode = KEY_Space; break;
    case XK_Escape:   keycode = KEY_Escape; break;
    case XK_Menu:     keycode = KEY_Menu; break;
    case XK_Control_L: case XK_Control_R: keycode = KEY_Control; break;
    case XK_Shift_L:   case  XK_Shift_R:  keycode = KEY_Shift; break;

    case XK_BackSpace: keycode = KEY_Backspace; break;
    case XK_Delete:    keycode = KEY_Delete; break;
    case XK_Tab:       keycode = KEY_Tab; break;

    case XK_Prior:   keycode = KEY_Page_Up; break;
    case XK_Next:    keycode = KEY_Page_Down; break;
    case XK_End:     keycode = KEY_End; break;
    case XK_Home:    keycode = KEY_Home; break;
        
    default: keycode = KEY_None; break;
    }
    
    return keycode;    
}

string x11_request_code_to_string(s32 code) {
    string string = "(UnknownRequest)"_s;

    switch(code) {
    case 1:   string = "XCreateWindow"_s; break;
    case 2:   string = "XChangeWindowAttributes"_s; break;
    case 3:   string = "XGetWindowAttributes"_s; break;
    case 4:   string = "XDestroyWindow"_s; break;
    case 5:   string = "XDestroySubwindows"_s; break;
    case 6:   string = "XChangeSaveSet"_s; break;
    case 7:   string = "XReparentWindow"_s; break;
    case 8:   string = "XMapWindow"_s; break;
    case 9:   string = "XMapSubwindows"_s; break;
    case 10:  string = "XUnmapWindow"_s; break;
    case 11:  string = "XUnmapSubwindows"_s; break;
    case 12:  string = "XConfigureWindow"_s; break;
    case 13:  string = "XCirculateWindow"_s; break;
    case 14:  string = "XGetGeometry"_s; break;
    case 15:  string = "XQueryTree"_s; break;
    case 16:  string = "XInternAtom"_s; break;
    case 17:  string = "XGetAtomName"_s; break;
    case 18:  string = "XChangeProperty"_s; break;
    case 19:  string = "XDeleteProperty"_s; break;
    case 20:  string = "XGetProperty"_s; break;
    case 21:  string = "XListProperties"_s; break;
    case 22:  string = "XSetSelectionOwner"_s; break;
    case 23:  string = "XGetSelectionOwner"_s; break;
    case 24:  string = "XConvertSelection"_s; break;
    case 25:  string = "XSendEvent"_s; break;
    case 26:  string = "XGrabPointer"_s; break;
    case 27:  string = "X_UngrabPointer"_s; break;
    case 28:  string = "X_GrabButton"_s; break;
    case 29:  string = "X_UngrabButton"_s; break;
    case 30:  string = "X_ChangeActivePointerGrab"_s; break;
    case 31:  string = "X_GrabKeyboard"_s; break;
    case 32:  string = "X_UngrabKeyboard"_s; break;
    case 33:  string = "X_GrabKey"_s; break;
    case 34:  string = "X_UngrabKey"_s; break;
    case 35:  string = "X_AllowEvents"_s; break;
    case 36:  string = "X_GrabServer"_s; break;
    case 37:  string = "X_UngrabServer"_s; break;
    case 38:  string = "X_QueryPointer"_s; break;
    case 39:  string = "X_GetMotionEvents"_s; break;
    case 40:  string = "X_TranslateCoords"_s; break;
    case 41:  string = "X_WarpPointer"_s; break;
    case 42:  string = "X_SetInputFocus"_s; break;
    case 43:  string = "X_GetInputFocus"_s; break;
    case 44:  string = "X_QueryKeymap"_s; break;
    case 45:  string = "X_OpenFont"_s; break;
    case 46:  string = "X_CloseFont"_s; break;
    case 47:  string = "X_QueryFont"_s; break;
    case 48:  string = "X_QueryTextExtents"_s; break;
    case 49:  string = "X_ListFonts"_s; break;
    case 50:  string = "X_ListFontsWithInfo"_s; break;
    case 51:  string = "X_SetFontPath"_s; break;
    case 52:  string = "X_GetFontPath"_s; break;
    case 53:  string = "X_CreatePixmap"_s; break;
    case 54:  string = "X_FreePixmap"_s; break;
    case 55:  string = "X_CreateGC"_s; break;
    case 56:  string = "X_ChangeGC"_s; break;
    case 57:  string = "X_CopyGC"_s; break;
    case 58:  string = "X_SetDashes"_s; break;
    case 59:  string = "X_SetClipRectangles"_s; break;
    case 60:  string = "X_FreeGC"_s; break;
    case 61:  string = "X_ClearArea"_s; break;
    case 62:  string = "X_CopyArea"_s; break;
    case 63:  string = "X_CopyPlane"_s; break;
    case 64:  string = "X_PolyPoint"_s; break;
    case 65:  string = "X_PolyLine"_s; break;
    case 66:  string = "X_PolySegment"_s; break;
    case 67:  string = "X_PolyRectangle"_s; break;
    case 68:  string = "X_PolyArc"_s; break;
    case 69:  string = "X_FillPoly"_s; break;
    case 70:  string = "X_PolyFillRectangle"_s; break;
    case 71:  string = "X_PolyFillArc"_s; break;
    case 72:  string = "X_PutImage"_s; break;
    case 73:  string = "X_GetImage"_s; break;
    case 74:  string = "X_PolyText8"_s; break;
    case 75:  string = "X_PolyText16"_s; break;
    case 76:  string = "X_ImageText8"_s; break;
    case 77:  string = "X_ImageText16"_s; break;
    case 78:  string = "X_CreateColormap"_s; break;
    case 79:  string = "X_FreeColormap"_s; break;
    case 80:  string = "X_CopyColormapAndFree"_s; break;
    case 81:  string = "X_InstallColormap"_s; break;
    case 82:  string = "X_UninstallColormap"_s; break;
    case 83:  string = "X_ListInstalledColormaps"_s; break;
    case 84:  string = "X_AllocColor"_s; break;
    case 85:  string = "X_AllocNamedColor"_s; break;
    case 86:  string = "X_AllocColorCells"_s; break;
    case 87:  string = "X_AllocColorPlanes"_s; break;
    case 88:  string = "X_FreeColors"_s; break;
    case 89:  string = "X_StoreColors"_s; break;
    case 90:  string = "X_StoreNamedColor"_s; break;
    case 91:  string = "X_QueryColors"_s; break;
    case 92:  string = "X_LookupColor"_s; break;
    case 93:  string = "X_CreateCursor"_s; break;
    case 94:  string = "X_CreateGlyphCursor"_s; break;
    case 95:  string = "X_FreeCursor"_s; break;
    case 96:  string = "X_RecolorCursor"_s; break;
    case 97:  string = "X_QueryBestSize"_s; break;
    case 98:  string = "X_QueryExtension"_s; break;
    case 99:  string = "X_ListExtensions"_s; break;
    case 100: string = "X_ChangeKeyboardMapping"_s; break;
    case 101: string = "X_GetKeyboardMapping"_s; break;
    case 102: string = "X_ChangeKeyboardControl"_s; break;
    case 103: string = "X_GetKeyboardControl"_s; break;
    case 104: string = "X_Bell"_s; break;
    case 105: string = "X_ChangePointerControl"_s; break;
    case 106: string = "X_GetPointerControl"_s; break;
    case 107: string = "X_SetScreenSaver"_s; break;
    case 108: string = "X_GetScreenSaver"_s; break;
    case 109: string = "X_ChangeHosts"_s; break;
    case 110: string = "X_ListHosts"_s; break;
    case 111: string = "X_SetAccessControl"_s; break;
    case 112: string = "X_SetCloseDownMode"_s; break;
    case 113: string = "X_KillClient"_s; break;
    case 114: string = "X_RotateProperties"_s; break;
    case 115: string = "X_ForceScreenSaver"_s; break;
    case 116: string = "X_SetPointerMapping"_s; break;
    case 117: string = "X_GetPointerMapping"_s; break;
    case 118: string = "X_SetModifierMapping"_s; break;
    case 119: string = "X_GetModifierMapping"_s; break;
    case 127: string = "X_NoOperation"_s; break;
    }

    return string;
}

s32 x11_error_handler(X11::Display *display, X11::XErrorEvent *event) {
    char buffer[256];
    X11::XGetErrorText(display, event->error_code, buffer, ARRAY_COUNT(buffer)); // This puts a null-terminated string into the buffer
    s64 length = cstring_length(buffer);
    string request_code = x11_request_code_to_string(event->request_code);
    
    foundation_error("[X11]: %.*s : '%.*s'.\n", (u32) request_code.count, request_code.data, (u32) length, buffer);
    return 0;
}

void x11_event_handler(Window *window, X11::XEvent *event) {
    switch(event->type) {
    case FocusIn: window->focused = true; break;
    case FocusOut: window->focused = false; break;

    case CreateNotify:
        window->w = event->xcreatewindow.width;
        window->h = event->xcreatewindow.height;
        break;

    case ConfigureNotify:
        window->x = event->xconfigurerequest.x;
        window->y = event->xconfigurerequest.y;
        window->w = event->xconfigurerequest.width;
        window->h = event->xconfigurerequest.height;
        window->resized_this_frame = true;
        window->moved_this_frame   = true;
        break;

    case MotionNotify:
        window->mouse_delta_x    += event->xmotion.x - window->mouse_x;
        window->mouse_delta_y    += event->xmotion.y - window->mouse_y;
        window->mouse_x           = event->xmotion.x;
        window->mouse_y           = event->xmotion.y;
        window->raw_mouse_delta_x = window->mouse_delta_x;
        window->raw_mouse_delta_y = window->mouse_delta_y;
        window->mouse_active_this_frame = true;
        break;

    case LeaveNotify:
        window->mouse_active_this_frame = false;
        break;

    case ButtonPress:
        switch(event->xbutton.button) {
        case Button1:
            window->buttons[BUTTON_Left] = BUTTON_Down | BUTTON_Pressed;
            break;

        case Button3:
            window->buttons[BUTTON_Right] = BUTTON_Down | BUTTON_Pressed;
            break;

        case Button4: // Mouse wheel up
            window->mouse_wheel_turns += 1;
            break;

        case Button5: // Mouse wheel down
            window->mouse_wheel_turns -= 1;
            break;
        }
        
        break;

    case ButtonRelease:
        switch(event->xbutton.button) {
        case Button1:
            window->buttons[BUTTON_Left] = BUTTON_Released;
            break;

        case Button3:
            window->buttons[BUTTON_Right] = BUTTON_Released;
            break;
        }
        
        break;

    case KeyPress: {
        Key_Code key = x11_key_map(event);
        Key_Status status = KEY_Down | KEY_Repeated;
        if(!(window->keys[key] & KEY_Down)) status |= KEY_Pressed;
        window->keys[key] = status;
    } break;

    case KeyRelease: {
        Key_Code key = x11_key_map(event);
        window->keys[key] = KEY_Released;
    } break;

    case ClientMessage: {
        Window_X11_State *x11 = (Window_X11_State *) window->platform_data;
        if(event->xclient.data.l[0] == x11->window_delete_atom) {
            window->should_close = true;
        }
    } break;
    }
}

void x11_adjust_position_and_size(s32 *x, s32 *y, s32 *w, s32 *h) {
    if(*x == WINDOW_DONT_CARE) {
        *x = 0;
    }

    if(*y == WINDOW_DONT_CARE) {
        *y = 0;
    }

    if(*w == WINDOW_DONT_CARE) {
        *w = 0;
    }

    if(*h == WINDOW_DONT_CARE) {
        *h = 0;
    }
}

void x11_query_position_and_size(Window *window) {
    Window_X11_State *x11 = (Window_X11_State *) window->platform_data;

    X11::XWindowAttributes attributes;
    if(!X11::XGetWindowAttributes(x11->display, x11->window_handle, &attributes)) return;

    window->x = attributes.x;
    window->y = attributes.y;
    window->w = attributes.width;
    window->h = attributes.height;
}

X11::Visual *x11_get_visual_for_rgb8(Window *window) {
    Window_X11_State *x11 = (Window_X11_State *) window->platform_data;

    X11::XVisualInfo template_info;
    template_info.red_mask     = 0xff0000;
    template_info.green_mask   = 0x00ff00;
    template_info.blue_mask    = 0x0000ff;
    template_info.bits_per_rgb = 8;

    s32 info_count = 0;
    X11::XVisualInfo *infos = X11::XGetVisualInfo(x11->display, VisualRedMaskMask | VisualGreenMaskMask | VisualBlueMaskMask | VisualBitsPerRGBMask, &template_info, &info_count);

    if(info_count <= 0) return null;
    
    X11::Visual *visual = infos[0].visual;
    
    XFree(infos);
    
    return visual;
}

void x11_set_window_style(Window *window, Window_Style_Flags style_flags) {
    Window_X11_State *x11 = (Window_X11_State *) window->platform_data;

    //
    // Set the more common, better supported flags through this state atom.
    //
    {
        X11::Atom _NET_WM_STATE = X11::XInternAtom(x11->display, "_NET_WM_STATE", false);

        X11::Atom atoms_to_set[2]; // 2 is currently the maximum number of atoms that would be needed to encode all style flags.
        s64 atoms_to_set_count = 0;

        if(style_flags & WINDOW_STYLE_Maximized) {
            atoms_to_set[atoms_to_set_count] = X11::XInternAtom(x11->display, "_NET_WM_STATE_MAXIMIZED_VERT", false);
            ++atoms_to_set_count;
            
            atoms_to_set[atoms_to_set_count] = X11::XInternAtom(x11->display, "_NET_WM_STATE_MAXIMIZED_HORZ", false);
            ++atoms_to_set_count;
        }
        
        if(atoms_to_set_count) {
            X11::XChangeProperty(x11->display, x11->window_handle, _NET_WM_STATE, 4, 32, PropModeReplace, (const unsigned char *) atoms_to_set, atoms_to_set_count); // 4 : XA_ATOM
        } else {
            X11::XDeleteProperty(x11->display, x11->window_handle, _NET_WM_STATE);
        }
    }

    //
    // Try to get the window manager to not display a title bar. This depends on the WM (since it isn't standard
    // X11 or something), so just cross our fingers... Linux is an absolute shithole.
    //
    if(style_flags & WINDOW_STYLE_Hide_Title_Bar) {
        X11::Atom _MOTIF_WM_HINTS = X11::XInternAtom(x11->display, "_MOTIF_WM_HINTS", false);
        if(_MOTIF_WM_HINTS) {
            s64 options[5] = { 0 };
            options[0] = 1; // Flags
            options[1] = 0; // Functions
            options[2] = 0; // 1: Border, 0: No Border
            options[3] = 0; // Input mode
            options[4] = 0; // Status

            X11::XChangeProperty(x11->display, x11->window_handle, _MOTIF_WM_HINTS, _MOTIF_WM_HINTS, 32, PropModeReplace, (const unsigned char *) options, 5);
        }
    }
}

b8 x11_create_window(Window *window, string title, s32 x, s32 y, s32 w, s32 h, Window_Style_Flags flags) {
    X11::XSetErrorHandler(x11_error_handler);

    char *cstring = to_cstring(Default_Allocator, title);
    defer { free_cstring(Default_Allocator, cstring); };

    Window_X11_State *x11 = (Window_X11_State *) window->platform_data;
    x11->display          = X11::XOpenDisplay(null);
    x11->screen_number    = X11::XDefaultScreen(x11->display);

    x11_adjust_position_and_size(&x, &y, &w, &h);

    unsigned long white_pixel = X11::XWhitePixel(x11->display, x11->screen_number);
    unsigned long black_pixel = X11::XBlackPixel(x11->display, x11->screen_number);
    
    //
    // Actually create the window.
    //
    x11->window_handle = X11::XCreateSimpleWindow(x11->display, XRootWindow(x11->display, x11->screen_number), x, y, w, h, 2, black_pixel, white_pixel);
    X11::XStoreName(x11->display, x11->window_handle, cstring);
    X11::XSelectInput(x11->display, x11->window_handle, ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask | EnterWindowMask | LeaveWindowMask | PointerMotionMask | Button1MotionMask | VisibilityChangeMask | FocusChangeMask);
    X11::XMapWindow(x11->display, x11->window_handle);
    X11::XFlush(x11->display);
    X11::XSync(x11->display, false);

    //
    // Wait in a busy loop until the window has become visible.
    // Before the window is visible, it ignores any drawing to it, which is just simply terrible.
    // If we didn't do this, we couldn't set the set foreground color or immediately draw something to this window
    // in user code.
    //
    while(true) {
        X11::XEvent event;
        X11::XNextEvent(x11->display, &event);
        if(event.type == VisibilityNotify) break;
    }

    // Install a custom atom for window closing by the human user.
    // Apparently this isn't considered standard behaviour on linux or something, which is why we need to jump
    // through this stupid hoop to get the message that the human just closed our window...
    x11->window_delete_atom = X11::XInternAtom(x11->display, "WM_DELETE_WINDOW", false);
    X11::XSetWMProtocols(x11->display, x11->window_handle, &x11->window_delete_atom, 1);

    // Create a Graphics Context and initialize the fore- and background of the window. Without this, we can't
    // actually represent colors for some god-forsaken reason.
    x11->gc = X11::XCreateGC(x11->display, x11->window_handle, 0, null);
    X11::XSetForeground(x11->display, x11->gc, white_pixel);
    X11::XSetBackground(x11->display, x11->gc, black_pixel);

    // Set the initial window attributes
    x11_set_window_style(window, flags);
    x11_query_position_and_size(window);

    // Set the initial window attributes
    unsigned long ignored_one; // X11 doesn't like null pointers for seemingly optional parameters.
    unsigned int ignored_two;
    int ignored_three;
    X11::XQueryPointer(x11->display, x11->window_handle, &ignored_one, &ignored_one, &ignored_three, &ignored_three, &window->mouse_x, &window->mouse_y, &ignored_two);

    X11::Window current_focused;
    s32 current_focus_mode;
    X11::XGetInputFocus(x11->display, &current_focused, &current_focus_mode);
    
    window->focused = current_focused == x11->window_handle;    

    return true;
}

void x11_destroy_window(Window *window) {
    Window_X11_State *x11 = (Window_X11_State *) window->platform_data;
    XFreeGC(x11->display, x11->gc);
    XDestroyWindow(x11->display, x11->window_handle);
    XCloseDisplay(x11->display);
    x11->gc             = null;
    x11->window_handle  = null;
    x11->screen_number  = 0;
    x11->display        = null;
}

#endif

b8 create_window(Window *window, string title, s32 x, s32 y, s32 w, s32 h, Window_Style_Flags flags) {
    memset(window->keys, 0, sizeof(window->keys));
    memset(window->buttons, 0, sizeof(window->buttons));
    window->frame_time             = 0.f;
    window->maximized              = false;
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
#elif FOUNDATION_LINUX
    return x11_create_window(window, title, x, y, w, h, flags);
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
#elif FOUNDATION_LINUX
    //
    // Handle all X11 events.
    //
    Window_X11_State *x11 = (Window_X11_State *) window->platform_data;

    s32 event_count = XEventsQueued(x11->display, QueuedAfterFlush);
    X11::XEvent event;

    for(s32 i = 0; i < event_count; ++i) {
        X11::XNextEvent(x11->display, &event);
        x11_event_handler(window, &event);
    }

#endif
}

void destroy_window(Window *window) {
#if FOUNDATION_WIN32
    win32_destroy_window(window);
#elif FOUNDATION_LINUX
    x11_destroy_window(window);
#endif
}

void show_window(Window *window) {
#if FOUNDATION_WIN32
    Window_Win32_State *win32 = (Window_Win32_State *) window->platform_data;
    DWORD command = SW_SHOW;
    if(window->maximized) command = SW_SHOWMAXIMIZED;
    ShowWindow(win32->hwnd, command);
    win32_query_position_and_size(window);
#elif FOUNDATION_LINUX
    Window_X11_State *x11 = (Window_X11_State *) window->platform_data;
    X11::XMapWindow(x11->display, x11->window_handle);
    X11::XFlush(x11->display); // Wait until the window is actually shown (?)
    x11_query_position_and_size(window);
#endif
}

b8 set_window_icon_from_file(Window *window, string file_path) {
#if FOUNDATION_WIN32
    Window_Win32_State *win32 = (Window_Win32_State *) window->platform_data;
    
    char *cstring = to_cstring(Default_Allocator, file_path);
    defer { free_cstring(Default_Allocator, cstring); };
    
    HANDLE icon = LoadImageA(null, cstring, IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    if(icon == null) return false;
    
    SendMessageA(win32->hwnd, WM_SETICON, ICON_SMALL, (LPARAM) icon);
    SendMessageA(win32->hwnd, WM_SETICON, ICON_BIG,   (LPARAM) icon);
    
    return true;
#elif FOUNDATION_LINUX
    // @Incomplete:
    // https://www.gamedev.net/forums/topic/697892-solved-how-to-set-the-window-bars-icon-in-x11/
    return false;
#endif
}

b8 set_window_icon_from_resource_name(Window *window, string resource_name) {
#if FOUNDATION_WIN32
    Window_Win32_State *win32 = (Window_Win32_State *) window->platform_data;
    
    char *cstring = to_cstring(Default_Allocator, resource_name);
    defer { free_cstring(Default_Allocator, cstring); };
    
    HANDLE icon = LoadIconA(GetModuleHandleA(null), cstring);
    if(icon == null) return false;
    
    SendMessageA(win32->hwnd, WM_SETICON, ICON_SMALL, (LPARAM) icon);
    SendMessageA(win32->hwnd, WM_SETICON, ICON_BIG,   (LPARAM) icon);
    
    return true;
#elif FOUNDATION_LINUX
    return false;
#endif
}

void set_window_name(Window *window, string name) {
    char *cstring = to_cstring(Default_Allocator, name);
    
#if FOUNDATION_WIN32
    Window_Win32_State *win32 = (Window_Win32_State *) window->platform_data;
    SetWindowTextA(win32->hwnd, cstring);
#elif FOUNDATION_LINUX
    Window_X11_State *x11 = (Window_X11_State *) window->platform_data;
    X11::XStoreName(x11->display, x11->window_handle, cstring);
#endif
    
    free_cstring(Default_Allocator, cstring);
}

void set_window_position_and_size(Window *window, s32 x, s32 y, s32 w, s32 h, b8 maximized) {
#if FOUNDATION_WIN32
    Window_Win32_State *win32 = (Window_Win32_State *) window->platform_data;
    DWORD style = GetWindowLongA(win32->hwnd, GWL_STYLE);
    DWORD exstyle = GetWindowLongA(win32->hwnd, GWL_EXSTYLE);
    
    RECT current_rect;
    
    GetWindowRect(win32->hwnd, &current_rect);
    if(x == WINDOW_DONT_CARE) x = current_rect.left;
    if(y == WINDOW_DONT_CARE) y = current_rect.top;
    
    GetClientRect(win32->hwnd, &current_rect);
    if(w == WINDOW_DONT_CARE) w = current_rect.right - current_rect.left;
    if(h == WINDOW_DONT_CARE) h = current_rect.bottom - current_rect.top;
    
    win32_adjust_position_and_size(&x, &y, &w, &h, style, exstyle);
    
    SetWindowPos(win32->hwnd, null, x, y, w, h, 0);
    
    window->maximized = maximized;
    show_window(window); // This will toggle maximization on the window.
    
    win32_query_position_and_size(window);
#elif FOUNDATION_LINUX
    // @Incomplete
#endif
}

void set_window_style(Window *window, Window_Style_Flags style_flags) {
#if FOUNDATION_WIN32
    Window_Win32_State *win32 = (Window_Win32_State *) window->platform_data;
    
    s32 x = window->x;
    s32 y = window->y;
    s32 w = window->w;
    s32 h = window->h;
    
    DWORD new_style = win32_get_window_style(style_flags) | WS_VISIBLE;
    SetWindowLongA(win32->hwnd, GWL_STYLE, new_style);
    
    DWORD style   = GetWindowLongA(win32->hwnd, GWL_STYLE);
    DWORD exstyle = GetWindowLongA(win32->hwnd, GWL_EXSTYLE);
    
    win32_adjust_position_and_size(&x, &y, &w, &h, style, exstyle);
    SetWindowPos(win32->hwnd, null, x, y, w, h, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    
    if(style_flags & WINDOW_STYLE_Maximized) window->maximized = true;
    
    show_window(window);
    UpdateWindow(win32->hwnd);
    
    win32_query_position_and_size(window);
    window->resized_this_frame = true;
#elif FOUNDATION_LINUX
    // @Incomplete
#endif
}

void set_cursor_position(Window *window, s32 x, s32 y) {
#if FOUNDATION_WIN32
    Window_Win32_State *win32 = (Window_Win32_State *) window->platform_data;
    
    POINT point = { x, y };
    ClientToScreen(win32->hwnd, &point);
    SetCursorPos(point.x, point.y);
    
    GetCursorPos(&point);
    ScreenToClient(win32->hwnd, &point);
    window->mouse_x = point.x;
    window->mouse_y = point.y;
#elif FOUNDATION_LINUX
    // @Incomplete
#endif
}

void get_desktop_bounds(s32 *x0, s32 *y0, s32 *x1, s32 *y1) {
#if FOUNDATION_WIN32
    RECT desktop_rectangle;
    GetWindowRect(GetDesktopWindow(), &desktop_rectangle);
    *x0 = desktop_rectangle.left;
    *y0 = desktop_rectangle.top;
    *x1 = desktop_rectangle.right;
    *y1 = desktop_rectangle.bottom;
#elif FOUNDATION_LINUX
    // @Incomplete
#endif
}

void hide_cursor() {
#if FOUNDATION_WIN32
    while(ShowCursor(false) >= 0) {}
#elif FOUNDATION_LINUX
    // @Incomplete
#endif
}

void show_cursor() {
#if FOUNDATION_WIN32
    ShowCursor(true);
#elif FOUNDATION_LINUX
    // @Incomplete
#endif
}

void confine_cursor(s32 x0, s32 y0, s32 x1, s32 y1) {
#if FOUNDATION_WIN32
    RECT rect;
    rect.left   = x0;
    rect.top    = y0;
    rect.right  = x1;
    rect.bottom = y1;
    ClipCursor(&rect);
#elif FOUNDATION_LINUX
    // @Incomplete
#endif
}

void unconfine_cursor() {
#if FOUNDATION_WIN32
    ClipCursor(null);
#elif FOUNDATION_LINUX
    // @Incomplete
#endif
}

void window_sleep(f32 seconds) {
#if FOUNDATION_WIN32
    Sleep((DWORD) (seconds * 1000.0f));
#elif FOUNDATION_LINUX
    // @Incomplete
#endif
}

void window_ensure_frame_time(s64 frame_start, s64 frame_end, f32 requested_fps) {
    f64 frame_time_nanoseconds = os_convert_hardware_time(frame_end - frame_start, Nanoseconds);
    f64 expected_frame_time_nanoseconds = 1000000000 / requested_fps;
    
    if(frame_time_nanoseconds < expected_frame_time_nanoseconds) {
        s32 milliseconds = (s32) floor((expected_frame_time_nanoseconds - frame_time_nanoseconds) / 1000000);
        if(milliseconds > 1) {
#if FOUNDATION_WIN32
            Sleep(milliseconds - 1);
#elif FOUNDATION_LINUX
    // @Incomplete
#endif
        }
        
        frame_end = os_get_hardware_time();
        frame_time_nanoseconds = os_convert_hardware_time(frame_end - frame_start, Nanoseconds);
        while(frame_time_nanoseconds < expected_frame_time_nanoseconds) {
            frame_end = os_get_hardware_time();
            frame_time_nanoseconds = os_convert_hardware_time(frame_end - frame_start, Nanoseconds);
        }
    }
}


void set_clipboard_data(Window *window, string data) {
#if FOUNDATION_WIN32
    HGLOBAL clipboard_handle = GlobalAlloc(GMEM_MOVEABLE, data.count + 1);
    if(clipboard_handle == INVALID_HANDLE_VALUE) return;

    char *clipboard_data = (char *) GlobalLock(clipboard_handle);
    memcpy(clipboard_data, data.data, data.count);
    clipboard_data[data.count] = 0;
    GlobalUnlock(clipboard_handle);

    if(!OpenClipboard(null)) {
        GlobalFree(clipboard_handle);
        return;
    }

    EmptyClipboard();
    SetClipboardData(CF_TEXT, clipboard_handle);
    CloseClipboard();
#elif FOUNDATION_LINUX
    // @Incomplete
#endif
}

string get_clipboard_data(Window *window, Allocator *allocator) {
#if FOUNDATION_WIN32
    if(!IsClipboardFormatAvailable(CF_TEXT)) return string{};

    if(!OpenClipboard(null)) return string{};

    HGLOBAL clipboard_handle = GetClipboardData(CF_TEXT);
    char *clipboard_data = (char *) GlobalLock(clipboard_handle);

    s64 string_length = cstring_length(clipboard_data);
    string result = allocate_string(allocator, string_length);
    memcpy(result.data, clipboard_data, string_length);

    GlobalUnlock(clipboard_handle);
    CloseClipboard();

    return result;
#elif FOUNDATION_LINUX
    // @Incomplete
    return ""_s;
#endif
}

void deallocate_clipboard_data(Allocator *allocator, string *data) {
#if FOUNDATION_WIN32
    deallocate_string(allocator, data);
#elif FOUNDATION_LINUX
    // @Incomplete
#endif
}



/* -------------------------------------------- Window Buffer API -------------------------------------------- */

void acquire_window_buffer(Window *window, Window_Buffer *buffer) {
    s64 pixels = window->w * window->h * 4;
    buffer->pixels = (u8 *) Default_Allocator->allocate(pixels * sizeof(u8));
    
    if(buffer->pixels) {
        buffer->width  = window->w;
        buffer->height = window->h;
    }
}

void destroy_window_buffer(Window_Buffer *buffer) {
    Default_Allocator->deallocate(buffer->pixels);
    buffer->pixels = null;
    buffer->width = 0;
    buffer->height = 0;
}

void clear_window_buffer(Window_Buffer *buffer, u8 r, u8 g, u8 b) {
    for(s32 y = 0; y < buffer->height; ++y) {
        for(s32 x = 0; x < buffer->width; ++x) {
            paint_window_buffer(buffer, x, y, r, g, b);
        }
    }
}

void paint_window_buffer(Window_Buffer *buffer, s32 x, s32 y, u8 r, u8 g, u8 b) {
    assert(x >= 0 && x < buffer->width && y >= 0 && y < buffer->height);
    s64 offset = (x + y * buffer->width) * 4;
    buffer->pixels[offset + 0] = b;
    buffer->pixels[offset + 1] = g;
    buffer->pixels[offset + 2] = r;
    buffer->pixels[offset + 3] =  255;
}

void query_window_buffer(Window_Buffer *buffer, s32 x, s32 y, u8 *r, u8 *g, u8 *b) {
    assert(x >= 0 && x < buffer->width && y >= 0 && y < buffer->height);
    s64 offset = (x + y * buffer->width) * 4;
    *b = buffer->pixels[offset + 0];
    *g = buffer->pixels[offset + 1];
    *r = buffer->pixels[offset + 2];
}

void blit_window_buffer(Window *window, Window_Buffer *buffer) {
    blit_pixels_to_window(window, buffer->pixels, buffer->width, buffer->height);
}

void blit_pixels_to_window(Window *window, u8 *pixels, s32 width, s32 height) {
#if FOUNDATION_WIN32
    Window_Win32_State *win32 = (Window_Win32_State *) window->platform_data;
    
    BITMAPINFO bmi              = { 0 };
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage   = 0;
    
    if(!SetDIBitsToDevice(win32->dc, 0, 0, width, height, 0, 0, 0, height, pixels, &bmi, DIB_RGB_COLORS)) {
        foundation_error("SetDIBitsToDevice failed."); // Unfortunately, (most) GDI functions do not use GetLastError()
    }
#elif FOUNDATION_LINUX
    // @Incomplete
#endif
}

u8 *convert_window_buffer_to_rgba(Window_Buffer *buffer) {
    u8 *result = (u8 *) Default_Allocator->allocate(buffer->width * buffer->height * 4);
    
    for(s64 y = 0; y < buffer->height; ++y) {
        for(s64 x = 0; x < buffer->width; ++x) {
            s64 offset = (y * buffer->width + x) * 4;
            result[offset + 0] = buffer->pixels[offset + 2];
            result[offset + 1] = buffer->pixels[offset + 1];
            result[offset + 2] = buffer->pixels[offset + 0];
            result[offset + 3] = buffer->pixels[offset + 3];
        }
    }
    
    return result;
}



#if FOUNDATION_WIN32
void *window_extract_hwnd(Window *window) {
    Window_Win32_State *win32 = (Window_Win32_State *) window->platform_data;
    return win32->hwnd;
}
#endif
