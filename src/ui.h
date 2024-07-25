#pragma once

#include "foundation.h"
#include "string_type.h"
#include "memutils.h"
#include "text_input.h"

#define UI_TEXT_INPUT_EVENT_CAPACITY 16
#define UI_STACK_CAPACITY 16

struct UI_Element;
struct Font;
struct Window;

struct UI_Vector2 {
    f32 x, y;
};

struct UI_Color {
    u8 r, g, b, a;
};

struct UI_Rect {
    f32 x0, y0, x1, y1;
};

typedef u64 UI_Hash;

typedef void(*UI_Draw_Text_Callback)(void *, string, UI_Vector2, UI_Color, UI_Color); // user_pointer, text, position, foreground_color, background_color
typedef void(*UI_Draw_Quad_Callback)(void *, UI_Vector2, UI_Vector2, f32, UI_Color); // user_pointer, top_left, bottom_right, rounding, color
typedef void(*UI_Set_Scissors_Callback)(void *, UI_Rect); // user_pointer, visible_rect
typedef void(*UI_Clear_Scissors_Callback)(void *); // user_pointer
typedef void(*UI_Custom_Draw_Callback)(void *, UI_Element *, void *); // user_pointer, element, element_custom_draw_data

#define UI_NULL_HASH ((UI_Hash) 0)
#define UI_DEBUG_PRINT true
#define UI_DEBUG_DRAW  true

#define UI_COLOR_TRANSITION_TIME .1f // Time in seconds it takes for a color transition to complete.
#define UI_SIZE_TRANSITION_TIME  (UI_COLOR_TRANSITION_TIME * 4) // Time in seconds it takes for a size transition to complete
#define UI_COLOR_TRANSITION_SPEED (1.f / UI_COLOR_TRANSITION_TIME) // Speed at which a color transition animates to fulfill the transition time
#define UI_SIZE_TRANSITION_SPEED  (1.f / UI_SIZE_TRANSITION_TIME) // Speed at which a size transition animates to fulfill the transition time
#define UI_DEACTIVE_ALPHA_DENOMINATOR 3 // The alpha value of all colors is divided by this value whenever the UI is in deactivated mode.


enum UI_Window_Flags {
    UI_WINDOW_Default         = 0x0,
    UI_WINDOW_Closeable       = 0x1,
    UI_WINDOW_Collapsable     = 0x2,
    UI_WINDOW_Draggable       = 0x4,
    UI_WINDOW_Center_Children = 0x8,
};

enum UI_Window_State {
    UI_WINDOW_Closed    = 0x0,
    UI_WINDOW_Open      = 0x1,
    UI_WINDOW_Collapsed = 0x2,
};

enum UI_Text_Input_State {
    UI_TEXT_INPUT_Inactive = 0x0,
    UI_TEXT_INPUT_Active   = 0x1,
    UI_TEXT_INPUT_Entered  = 0x2,
};

enum UI_Flags {
    UI_Empty = 0x0,

    /* Rendering Flags */
    UI_Label      = 1 << 0,
    UI_Background = 1 << 1,
    UI_Border     = 1 << 2,

    /* Interaction Flags */
    UI_Clickable   = 1 << 3,
    UI_Activatable = 1 << 4,
    UI_Draggable   = 1 << 5,
    UI_Text_Input  = 1 << 6,

    /* Layout Flags */
    UI_Floating             = 1 << 7, // Other elements ignore this one during the layout phase.
    UI_Center_Label         = 1 << 8,
    UI_Center_Children      = 1 << 9,
    UI_Extrude_Children     = 1 << 10,
    UI_Detach_From_Parent   = 1 << 11, // Ignore the parent stack for this element and instead place this element in the root node.
    UI_View_Scroll_Children = 1 << 12, // The children of this element can scroll in the layout direction, and only a part of the elements are actually visible.

    /* Misc Flags */
    UI_Snap_Draggable_Children_On_Click  = 1 << 13, // When an element has a draggable child, snap that child to the cursor when the left button gets pressed
    UI_Deactivate_Automatically_On_Click = 1 << 14, // When an element is active and the mouse button is pressed somewhere else on the screen, deactivate it
    UI_Drag_On_Screen_Space              = 1 << 15, // The drag position of an element is usually normalized to (0,1) to work screen-resolution independent. Sometimes however that behaviour is unwanted though, e.g. when wanting to position the element under the mouse cursor.

    /* Animation Flags */
    UI_Animate_Size_On_Activation = 1 << 16,
    UI_Animate_Size_On_Hover      = 1 << 17,

    /* User Level Customization */
    UI_Custom_Drawing_Procedure = 1 << 18, // This UI element requires user-level drawing mechanisms, so call the callback pointer when drawing the element.
};

BITWISE(UI_Flags);

enum UI_Signals {
    UI_SIGNAL_None            = 0,
    UI_SIGNAL_Hovered         = 1 << 0,
    UI_SIGNAL_Subtree_Hovered = 1 << 1,
    UI_SIGNAL_Clicked         = 1 << 2,
    UI_SIGNAL_Active          = 1 << 3,
    UI_SIGNAL_Dragged         = 1 << 4,
};

BITWISE(UI_Signals);

enum UI_Alignment {
    UI_ALIGN_Left,
    UI_ALIGN_Center,
    UI_ALIGN_Right,
};

enum UI_Direction {
    UI_DIRECTION_Horizontal,
    UI_DIRECTION_Vertical,
};

enum UI_Semantic_Size_Tag {
    UI_SEMANTIC_SIZE_Pixels,
    UI_SEMANTIC_SIZE_Label_Size,
    UI_SEMANTIC_SIZE_Percentage_Of_Parent,
    UI_SEMANTIC_SIZE_Sum_Of_Children,
    UI_SEMANTIC_SIZE_Percentage_Of_Other_Dimension,
};

enum UI_Violation_Resolution {
    UI_VIOLATION_RESOLUTION_Align_To_Pixels,
    UI_VIOLATION_RESOLUTION_Cap_At_Parent_Size,
    UI_VIOLATION_RESOLUTION_Squish,    
};

struct UI_Theme {
    UI_Color default_color;
    UI_Color border_color;
    UI_Color hovered_color;
    UI_Color accent_color;
    UI_Color text_color;

    UI_Color window_title_bar_color;
    UI_Color hovered_window_title_bar_color;
    UI_Color window_title_bar_button_color;
    UI_Color window_background_color;

    f32 border_size;
    f32 rounding;

    UI_Flags button_style;
    b8 lit_border;
};

struct UI_Semantic_Size {
    UI_Semantic_Size_Tag tag;
    f32 value;
    f32 strictness;
};

struct UI_Element {
    /* Tree structure for this element */
    UI_Element *next;
    UI_Element *prev;
    UI_Element *first_child;
    UI_Element *last_child;
    UI_Element *parent;

    /* Basic information */
    b8 created_this_frame;
    b8 used_this_frame;
    UI_Hash hash;
    string label;

    /* Styling */
    UI_Flags flags;
    UI_Alignment alignment;
    UI_Direction layout_direction;

    UI_Color default_color;
    UI_Color hovered_color;
    UI_Color active_color;
    f32 rounding;

    /* State */
    UI_Signals signals;
    f32 active_t;
    f32 hover_t;
    f32 size_t;
    UI_Vector2 drag_offset;
    UI_Vector2 drag_vector;
    UI_Vector2 view_scroll_screen_offset;
    UI_Vector2 view_scroll_screen_size;
    Text_Input *text_input;

    /* Layout */
    UI_Semantic_Size semantic_width;
    UI_Semantic_Size semantic_height;
    UI_Vector2 screen_position;
    UI_Vector2 screen_size;
    UI_Vector2 label_size;

    /* Custom Handling */
    void *custom_state;
    UI_Custom_Draw_Callback custom_draw;
};

struct UI_Callbacks {
    void *user_pointer;
    UI_Draw_Text_Callback draw_text;
    UI_Draw_Quad_Callback draw_quad;
    UI_Set_Scissors_Callback set_scissors;
    UI_Clear_Scissors_Callback clear_scissors;
};

template<typename T>
struct UI_Stack {
    T elements[UI_STACK_CAPACITY];
    u32 count;
};

struct UI {
    /* Memory Managment */
    Memory_Arena arena;
    Allocator allocator;

    /* Callbacks */
    UI_Callbacks callbacks;

    /* Tree structure */
    UI_Element *elements;
    u64 elements_allocated; // The size of the elements array
    u64 element_count; // The number of active UI elements in the array.
    UI_Element *last_element; // This is used for inserting UI elements.
    UI_Stack<UI_Element*> parent_stack;

    /* Text Input Pool */
    Linked_List<Text_Input> text_input_pool; // Whenever an element requires a text input, this text input gets added to this list here and gets removed again when the owning element is destroyed. This way we avoid storing the Text_Input inside the elements, which would be wayy too large.
    Text_Input *active_text_input;

    /* Global layout and styling information */
    UI_Stack<UI_Semantic_Size> semantic_width_stack;
    UI_Stack<UI_Semantic_Size> semantic_height_stack;
    b8 one_pass_semantic_width; // Remove the top semantic width after it has been used once. This improves the API when just wanting to use a semantic width once.
    b8 one_pass_semantic_height; // Remove the top semantic height after it has been used once. This improves the API when just wanting to use a semantic height once.

    /* Root element responsible for total UI size, global layout direction, global UI signals... */
    UI_Element root;
    b8 hovered_last_frame; // Set to true after a UI frame has been completed if the root's signals contain the subtree_hovered flag.

    /* Internal state keeping */
    b8 hovered_element_found;

    /* Input data provided by the user application */
    Window *window;
    Font   *font;
    UI_Theme theme;
    b8 deactivated; // The user application may deactivate the entire UI system, in which case no interaction happens and the UI is drawn with a faded alpha value.
    b8 font_changed; // The builder code may want to change the font size dynamically, in which case we need to update all cached label sizes of existing elements.
};



/* ------------------------------------------------ Basic API ------------------------------------------------ */

UI_Semantic_Size ui_query_width(UI *ui);
UI_Semantic_Size ui_query_height(UI *ui);
UI_Element *ui_element(UI *ui, string label, UI_Flags flags);
