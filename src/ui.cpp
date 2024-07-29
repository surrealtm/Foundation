#include "ui.h"
#include "window.h"
#include "font.h"

#include <stdarg.h>


UI_Theme UI_Dark_Theme = {
    {  20,  20,  20, 255 }, // Default
    { 200, 200, 200, 255 }, // Border
    {  70,  70,  70, 255 }, // Hovered
    { 148,  56,  44, 255 }, // Accent
    { 255, 255, 255, 255 }, // Text
    { 148,  56,  44, 255 }, // Title bar background
    { 208,  49,  24, 255 }, // Hovered title bar
    { 131,  23,  10, 255 }, // Title bar buttons
    {  50,  50,  50, 220 }, // Background
    2,             // Border size
    .7f,           // Rounding
    UI_Background, // Button style
    false,         // Unlit border
};

UI_Theme UI_Light_Theme = {
    { 255, 255, 255, 255 }, // Default
    { 100, 100, 100, 255 }, // Border
    { 200, 200, 200, 255 }, // Hovered
    { 120, 120, 120, 255 }, // Accent
    {   0,   0,   0, 255 }, // Text
    { 220, 220, 220, 255 }, // Title bar background
    { 255, 255, 255, 255 }, // Hovered title bar
    { 229, 229, 229, 255 }, // Title bar buttons
    { 201, 205, 219, 220 }, // Background
    2,             // Border size
    .7f,           // Rounding
    UI_Background, // Button style
    false,         // Unlit border
};

UI_Theme UI_Blue_Theme = {
    {  42,  74, 123, 255 }, // Default
    { 200, 200, 200, 255 }, // Border
    {  92, 134, 173, 255 }, // Hovered
    { 112, 154, 193, 255 }, // Accent
    { 255, 255, 255, 255 }, // Text
    {  42,  74, 123, 255 }, // Title bar background
    {  43, 102, 192, 255 }, // Hovered title bar
    {  53,  93, 153, 255 }, // Title bar buttons
    {  38,  46,  63, 220 }, // Background
    0,             // Border size
    .7f,           // Rounding
    UI_Background, // Button style
    false,         // Unlit border
};

UI_Theme UI_Watermelon_Theme = {
    {  90, 145,  49, 255 }, // Default
    { 110, 160,  60, 255 }, // Border
    { 115, 191,  62, 255 }, // Hovered
    { 105, 205,  31, 255 }, // Accent
    { 255, 255, 255, 255 }, // Text
    { 131,  23,  10, 255 }, // Title bar background
    { 206,  40,  22, 255 }, // Hovered title bar
    { 168,  28,  13, 255 }, // Title bar buttons
    {  39,  63, 104, 220 }, // Background
    2,             // Border size
    .7f,           // Rounding
    UI_Background | UI_Border, // Button style
    true,         // Unlit border
};

UI_Theme UI_Green_Theme = {
    {  20,  20,  20, 255 }, // Default
    {  13, 137,   0, 255 }, // Border
    {  70,  70,  80, 255 }, // Hovered
    {  17, 180,   0, 255 }, // Accent
    { 233, 255, 231, 255 }, // Text
    {  29, 185,  12, 255 }, // Title bar background
    {  40, 229,  19, 255 }, // Hovered title bar
    {  35, 209,  10, 255 }, // Title bar buttons
    {  50,  50,  50, 220 }, // Background
    1,             // Border size
    1.f,           // Rounding
    UI_Background | UI_Border, // Button style
    true,         // Unlit border
};



/* ------------------------------------------- Internal Convenience ------------------------------------------- */

template<typename T>
static inline
void push_ui_stack(UI_Stack<T> *stack, T element) {
    assert(stack->count < UI_STACK_CAPACITY);
    stack->elements[stack->count] = element;
    ++stack->count;
}

template<typename T>
static inline
T query_ui_stack(UI_Stack<T> *stack) {
    assert(stack->count > 0);
    return stack->elements[stack->count - 1];
}

template<typename T>
static inline
T pop_ui_stack(UI_Stack<T> *stack) {
    assert(stack->count > 0);
    --stack->count;
    return stack->elements[stack->count];
}

template<typename T>
static inline
void clear_ui_stack(UI_Stack<T> *stack) {
    stack->count = 0;
}

static
UI_Hash ui_hash(UI *ui, string label) {
    UI_Hash seed = UI_NULL_HASH;
    u32 parent_index = ui->parent_stack.count;

    while(seed == UI_NULL_HASH && parent_index > 0) {
        seed = ui->parent_stack.elements[parent_index - 1]->hash;
        --parent_index;
    }

    UI_Hash prime  = 1099511628211;
    UI_Hash offset = 14695981039346656037;

    UI_Hash hash = offset + seed;

    for(s64 i = 0; i < label.count; ++i) {
        hash ^= label[i];
        hash *= prime;
    }

    return hash;
}

static inline
string ui_copy_string(UI *ui, string input) {
    return copy_string(&ui->allocator, input);
}

static inline
string ui_concat_strings(UI *ui, string lhs, string rhs) {
    return concatenate_strings(&ui->allocator, lhs, rhs);
}

static inline
string ui_format_string(UI *ui, string format, ...) {
    va_list args;
    va_start(args, format);
    string result = mprint(&ui->allocator, format, args);
    va_end(args);
    return result;
}

static inline
UI_Color ui_mix_colors(UI_Color lhs, UI_Color rhs, f32 t) {
    f32 one_minus_t = 1.0f - t;

    UI_Color result;
    result.r = (u8) (((f32) lhs.r * one_minus_t) + ((f32) rhs.r * t));
    result.g = (u8) (((f32) lhs.g * one_minus_t) + ((f32) rhs.g * t));
    result.b = (u8) (((f32) lhs.b * one_minus_t) + ((f32) rhs.b * t));
    result.a = (u8) (((f32) lhs.a * one_minus_t) + ((f32) rhs.a * t));
    return result;
}

static inline
UI_Color ui_multiply_color(UI_Color color, f32 t) {
    UI_Color result;
    result.r = (u8) ((f32) color.r * t);
    result.g = (u8) ((f32) color.g * t);
    result.b = (u8) ((f32) color.b * t);
    result.a = color.a;
    return result;
}

static inline
f32 ui_size_animation_factor(f32 t) {
    return sinf(t * 10.0f) * (1.0f - t) * 0.25f;
}

static inline
b8 ui_mouse_over_rect(UI *ui, UI_Rect rect) {
    if(!ui->window->mouse_active_this_frame || ui->deactivated) return false;

    return ui->window->mouse_x >= rect.x0 && ui->window->mouse_x <= rect.x1 &&
        ui->window->mouse_y >= rect.y0 && ui->window->mouse_y <= rect.y1;
}

static inline
string ui_semantic_size_tag_to_string(UI_Semantic_Size_Tag tag) {
    string result;

    switch(tag) {
    case UI_SEMANTIC_SIZE_Pixels:                        result = "Pixels"_s; break;
    case UI_SEMANTIC_SIZE_Label_Size:                    result = "Label Size"_s; break;
    case UI_SEMANTIC_SIZE_Percentage_Of_Parent:          result = "Percentage_Of_Parent"_s; break;
    case UI_SEMANTIC_SIZE_Sum_Of_Children:               result = "Sum_Of_Children"_s; break;
    case UI_SEMANTIC_SIZE_Percentage_Of_Other_Dimension: result = "Percentage_Of_Other_Dimension"_s; break;
    default: result = "Unknown"_s; break;
    }
    
    return result;
}

static inline
string ui_direction_to_string(UI_Direction direction) {
    string result;

    switch(direction) {
    case UI_DIRECTION_Vertical:   result = "Vertical"_s; break;
    case UI_DIRECTION_Horizontal: result = "Horizontal"_s; break;
    default: result = "Unknown"_s; break;
    }

    return result;
}

static inline
UI_Direction get_opposite_layout_direction(UI_Direction direction) {
    return direction == UI_DIRECTION_Horizontal ? UI_DIRECTION_Vertical : UI_DIRECTION_Horizontal;
}

static inline
UI_Direction get_opposite_layout_direction(UI *ui) {
    // We (should) always have a parent, since the UI will always push the root element
    // as the first parent in a frame. If there is no more parent, the user of this module
    // has used it incorrectly and query_ui_stack will crash.
    UI_Element *parent = query_ui_stack(&ui->parent_stack);
    return get_opposite_layout_direction(parent->layout_direction);
}

// Some elements (mostly labels) want to have the same background color as their parent for visual reasons.
// The background color for a label must match the color the label is rendered on to avoid artifacts.
// For this reason, get the smallest parent that actually renders a background, and query the color of that
// background.
static inline
UI_Color get_default_color_recursively(UI_Element *element) {
    UI_Element *parent = element->parent;

    while(parent->parent && !(parent->flags & UI_Background)) parent = parent->parent;

    return parent->default_color;
}

static
void reset_element(UI *ui, UI_Element *element) {
    element->next               = null;
    element->prev               = null;
    element->first_child        = null;
    element->last_child         = null;
    element->parent             = null;
    element->created_this_frame = false;
    element->used_this_frame    = true;

    if(ui->font_changed) {
        element->label_size.x = (f32) get_string_width_in_pixels(ui->font, element->label);
        element->label_size.y = (f32) ui->font->glyph_height;
    }
}

static
void link_element(UI *ui, UI_Element *element) {
    if(element->flags & UI_Detach_From_Parent) {
        element->parent = &ui->root;
    } else {
        element->parent = query_ui_stack(&ui->parent_stack);
    }

    if(!element->parent->first_child) {
        element->parent->first_child = element;
        element->parent->last_child  = element;
    } else if(element->parent->last_child) {
        element->parent->last_child->next = element;
        element->prev = element->parent->last_child;
    }

    element->parent->last_child = element;
    ui->last_element = element;
}

static
UI_Element *insert_new_element(UI *ui, UI_Hash hash, string label, UI_Flags flags) {
    assert(ui->element_count < ui->element_capacity);

    UI_Element *element = &ui->elements[ui->element_count];
    ++ui->element_count;

    *element                    = UI_Element();
    element->flags              = flags;
    element->hash               = hash;
    element->label              = ui_copy_string(ui, label);
    element->label_size.x       = (f32) get_string_width_in_pixels(ui->font, element->label);
    element->label_size.y       = (f32) ui->font->glyph_height;
    element->created_this_frame = true;
    element->used_this_frame    = true;

    link_element(ui, element);

    return element;
}

static
UI_Element *insert_element_with_hash(UI *ui, UI_Hash hash, string label, UI_Flags flags) {
    // Check if this element already existed during the last frame, and also make sure that no element with that
    // exact hash has already been created during this frame to prevent any hash collisions.
    for(u64 i = 0; i < ui->element_count; ++i) {
        UI_Element *element = &ui->elements[i];
        if(element->hash == hash) {
            assert(element->used_this_frame == false, "UI Element has collision.");
            element->label = ui_copy_string(ui, label);
            element->flags = flags;
            reset_element(ui, element);
            link_element(ui, element);
            return element;
        }
    }

    // If that element was not part of the UI last frame (or it is a non-stateful element), create a new one with
    // the given data and insert that into the tree structure.
    return insert_new_element(ui, hash, label, flags);
}

static
void set_element_drag(UI *ui, UI_Element *element) {
    // It can happen that the element has a bigger size in a dimension than the parent, e.g. if this
    // is a scrollbar knob moving on the scrollbar background (which is made smaller for more visual
    // clarity when dragging...)
    // In that case, we want to restrict the movement on that axis.

    if(element->parent->screen_size.x >= element->screen_size.x) {
        element->float_vector.x = clamp((ui->window->mouse_x - element->parent->screen_position.x - element->drag_offset.x) / (element->parent->screen_size.x - element->screen_size.x), 0, 1);
    } else {
        element->float_vector.x = ((element->screen_size.x - element->parent->screen_size.x) / 2) / element->screen_size.x;
    }

    if(element->parent->screen_size.y >= element->screen_size.y) {
        element->float_vector.y = clamp((ui->window->mouse_y - element->parent->screen_position.y - element->drag_offset.y) / (element->parent->screen_size.y - element->screen_size.y), 0, 1);
    } else {
        element->float_vector.y = ((element->screen_size.y - element->parent->screen_size.y) / 2) / element->screen_size.y;
    }
}

static
void set_element_parent_tree_to_hovered(UI_Element *element) {
    while(element) {
        element->signals |= UI_SIGNAL_Subtree_Hovered;
        element = element->parent;
    }
}

static
UI_Rect calculate_rect_for_element_children(UI *ui, UI_Element *element, UI_Rect parent_rect) {
    UI_Rect rect;

    if(element->flags & UI_Extrude_Children) {
        if(element->parent && (element->parent->flags & UI_View_Scroll_Children)) {
            rect = parent_rect;
        } else {
            rect = { 0, 0, ui->root.screen_size.x, ui->root.screen_size.y };
        }
    } else {
        rect = { max(element->screen_position.x, parent_rect.x0),
                 max(element->screen_position.y, parent_rect.y0),
                 min(element->screen_position.x + element->screen_size.x - 1, parent_rect.x1),
                 min(element->screen_position.y + element->screen_size.y - 1, parent_rect.y1) };
    }

    if(element->flags & UI_Border) {
        rect.x0 += ui->theme.border_size;
        rect.y0 += ui->theme.border_size;
        rect.x1 -= ui->theme.border_size;
        rect.y1 -= ui->theme.border_size;
    }
    
    return rect;
}

static
UI_Semantic_Size ui_query_width(UI *ui) {
    UI_Semantic_Size size = query_ui_stack(&ui->semantic_width_stack);

    if(ui->one_pass_semantic_width) {
        pop_ui_stack(&ui->semantic_width_stack);
        ui->one_pass_semantic_width = false;
    }

    return size;
}

static
UI_Semantic_Size ui_query_height(UI *ui) {
    UI_Semantic_Size size = query_ui_stack(&ui->semantic_height_stack);

    if(ui->one_pass_semantic_height) {
        pop_ui_stack(&ui->semantic_height_stack);
        ui->one_pass_semantic_height = false;
    }

    return size;    
}



/* ---------------------------------------------- Debug Printing ---------------------------------------------- */

static
s64 get_debug_print_indentation_level(UI_Element *element) {
    s64 indentation = 0;

    while(element) {
        indentation += 2;
        element = element->parent;
    }

    return indentation;
}

static
void debug_print_element(UI_Element *element) {
    s64 indentation = get_debug_print_indentation_level(element);
    for(s64 i = 0; i < indentation; ++i) printf(" ");

    string width_string = ui_semantic_size_tag_to_string(element->semantic_width.tag);
    string height_string = ui_semantic_size_tag_to_string(element->semantic_height.tag);
    string direction_string = ui_direction_to_string(element->layout_direction);
    
    printf("Element %" PRIu64 " ('%.*s'): %f,%f : %fx%f | Width: %.*s, %f, %f | Height: %.*s, %f, %f | Layout Direction: %.*s\n", element->hash, (u32) element->label.count, element->label.data, element->screen_position.x, element->screen_position.y, element->screen_size.x, element->screen_size.y, (u32) width_string.count, width_string.data, element->semantic_width.value, element->semantic_width.strictness, (u32) height_string.count, height_string.data, element->semantic_height.value, element->semantic_height.strictness, (u32) direction_string.count, direction_string.data);
}



/* --------------------------------------------- Layout Algorithm --------------------------------------------- */

static
UI_Violation_Resolution get_ui_violation_resolution_rule(f32 total_size, f32 available_size, b8 parent_layout_in_this_direction, UI_Flags parent_flags) {
    UI_Violation_Resolution resolution;

    if(parent_layout_in_this_direction && total_size > available_size && !((parent_flags & UI_Extrude_Children) || (parent_flags & UI_View_Scroll_Children))) {
        resolution = UI_VIOLATION_RESOLUTION_Squish;
    } else if(!(parent_flags & UI_Extrude_Children) || !parent_layout_in_this_direction) {
        resolution = UI_VIOLATION_RESOLUTION_Cap_At_Parent_Size;
    } else {
        resolution = UI_VIOLATION_RESOLUTION_Align_To_Pixels;
    }
    
    return resolution;
}

static
void calculate_standalone_screen_size_for_element_recursively(UI_Element *element) {
    if(element->semantic_width.tag == UI_SEMANTIC_SIZE_Pixels) {
        element->screen_size.x = element->semantic_width.value;
    } else if(element->semantic_width.tag == UI_SEMANTIC_SIZE_Label_Size) {
        element->screen_size.x = element->label_size.x + element->semantic_width.value;
    }

    if(element->semantic_height.tag == UI_SEMANTIC_SIZE_Pixels) {
        element->screen_size.y = element->semantic_height.value;
    } else if(element->semantic_height.tag == UI_SEMANTIC_SIZE_Label_Size) {
        element->screen_size.y = element->label_size.y + element->semantic_height.value;
    }

    for(auto *child = element->first_child; child != null; child = child->next) {
        calculate_standalone_screen_size_for_element_recursively(child);
    }
}

static
void calculate_upwards_dependent_screen_size_for_element_recursively(UI_Element *element) {
    if(element->semantic_width.tag == UI_SEMANTIC_SIZE_Percentage_Of_Parent) {
        element->screen_size.x = element->semantic_width.value * element->parent->screen_size.x;
    }

    if(element->semantic_height.tag == UI_SEMANTIC_SIZE_Percentage_Of_Parent) {
        element->screen_size.y = element->semantic_height.value * element->parent->screen_size.y;
    }

    for(auto *child = element->first_child; child != null; child = child->next) {
        calculate_upwards_dependent_screen_size_for_element_recursively(child);
    }    
}

static
void calculate_downwards_dependent_screen_size_for_element_recursively(UI_Element *element) {
    UI_Vector2 sum_of_children = { 0, 0 };

    for(auto *child = element->first_child; child != null; child = child->next) {
        assert(element->semantic_width.tag != UI_SEMANTIC_SIZE_Sum_Of_Children || child->semantic_width.tag != UI_SEMANTIC_SIZE_Percentage_Of_Parent, "Detected a circular dependency in the UI semantic widths.");
        assert(element->semantic_height.tag != UI_SEMANTIC_SIZE_Sum_Of_Children || child->semantic_height.tag != UI_SEMANTIC_SIZE_Percentage_Of_Parent, "Detected a circular dependency in the UI semantic heights.");

        calculate_downwards_dependent_screen_size_for_element_recursively(child);

        if(element->layout_direction == UI_DIRECTION_Horizontal) {
            sum_of_children.x += child->screen_size.x;
            sum_of_children.y  = max(child->screen_size.y, sum_of_children.y);
        } else if(element->layout_direction == UI_DIRECTION_Vertical) {
            sum_of_children.x  = max(child->screen_size.x, sum_of_children.x);
            sum_of_children.y += child->screen_size.y;
        }
    }

    if(element->semantic_width.tag == UI_SEMANTIC_SIZE_Sum_Of_Children) {
        element->screen_size.x = sum_of_children.x + element->semantic_width.value;
    } 

    if(element->semantic_height.tag == UI_SEMANTIC_SIZE_Sum_Of_Children) {
        element->screen_size.y = sum_of_children.y + element->semantic_height.value;
    } 
    
    if(element->semantic_width.tag == UI_SEMANTIC_SIZE_Percentage_Of_Other_Dimension) {
        assert(element->semantic_height.tag != UI_SEMANTIC_SIZE_Percentage_Of_Other_Dimension, "Detected a bidirectional dependency in the UI semantic width.");
        element->screen_size.x = element->screen_size.y * element->semantic_width.value;
    }

    if(element->semantic_height.tag == UI_SEMANTIC_SIZE_Percentage_Of_Other_Dimension) {
        assert(element->semantic_width.tag != UI_SEMANTIC_SIZE_Percentage_Of_Other_Dimension, "Detected a bidirectional dependency in the UI semantic width.");
        element->screen_size.y = element->screen_size.x * element->semantic_height.value;
    }
}

static
void solve_screen_size_violations_recursively(UI_Element *parent) {
    //
    // First up, calculate the actual size the children take on, since that must be shrinked to fit the available
    // screen space. Also keep track of how much space in total the children are willing to give up due to their
    // strictness value being set.
    //

    UI_Vector2 children_screen_size  = { 0, 0 };
    UI_Vector2 children_fixup_budget = { 0, 0 };

    for(auto *child = parent->first_child; child != null; child = child->next) {
        if((child->flags & UI_Floating) == 0) {
            // Floating elements are ignored in screen size violations, as these are laid out independently
            // from the rest of the elements
            if(parent->layout_direction == UI_DIRECTION_Horizontal) {
                children_screen_size.x  += child->screen_size.x;
                children_screen_size.y   = max(child->screen_size.y, children_screen_size.y);
                children_fixup_budget.x += (1 - child->semantic_width.strictness) * child->screen_size.x;
            } else if(parent->layout_direction == UI_DIRECTION_Vertical) {
                children_screen_size.x   = max(child->screen_size.x, children_screen_size.x);
                children_screen_size.y  += child->screen_size.y;
                children_fixup_budget.y += (1 - child->semantic_height.strictness) * child->screen_size.y;
            }
        }
    }

    UI_Vector2 screen_size_violation = { children_screen_size.x - parent->screen_size.x, children_screen_size.y - parent->screen_size.y };

    //
    // Solve horizontal violations.
    //

    UI_Violation_Resolution horizontal_resolution = get_ui_violation_resolution_rule(children_screen_size.x, parent->screen_size.x, parent->layout_direction == UI_DIRECTION_Horizontal, parent->flags);

    switch(horizontal_resolution) {
    case UI_VIOLATION_RESOLUTION_Align_To_Pixels: {
        for(auto *child = parent->first_child; child != null; child = child->next) {
            child->screen_size.x = floorf(child->screen_size.x);
        }
    } break;

    case UI_VIOLATION_RESOLUTION_Cap_At_Parent_Size: {
        for(auto *child = parent->first_child; child != null; child = child->next) {
            if((child->flags & UI_Floating) == 0) {
                child->screen_size.x = floorf(min(parent->screen_size.x, child->screen_size.x));
            } else {
                child->screen_size.x = floorf(child->screen_size.x);
            }
        }        
    } break;

    case UI_VIOLATION_RESOLUTION_Squish: {
        //
        //    :UISubpixelTracking
        // When children were assigned fractional sizes during the layout algorithm, then we always round
        // that value down to an integer number of pixels for better visuals. That may however mean that
        // we "lose" some pixels by rounding down too often. Therefore, we keep track of the "lost" pixels
        // in this variable, and give them back to the next element once we have a full pixel.
        // Example: Say we have 101 of parent space, and two children with 50% parent screen space each.
        // Both children would be assigned 50.5 pixels, which does not work on a screen, so we round them
        // both down to 50, but now they only occupy 100 of the 101 pixels of the parent. Therefore, the
        // second child gets this "lost" pixel added, so we are back to 50 + 51 = 101 pixels.
        //
        f32 subpixel = 0;

        if(children_fixup_budget.x > 0.0f) {
            for(auto *child = parent->first_child; child != null; child = child->next) {
                if((child->flags & UI_Floating) == 0) {
                    f32 child_fixup_budget = ((1.f - child->semantic_width.strictness) * child->screen_size.x);
                    f32 child_fixup_size   = min(child_fixup_budget * (screen_size_violation.x / children_fixup_budget.x), child_fixup_budget);
                    child->screen_size.x  -= child_fixup_size;

                    subpixel += child->screen_size.x - floorf(child->screen_size.x);
                    child->screen_size.x = floorf(child->screen_size.x) + floorf(subpixel + 0.05f); // Avoid this weird glitch where subpixel does not quite reach 1.0 due to numerical imprecision.
                    subpixel -= floorf(subpixel + 0.05f);
                } else {
                    child->screen_size.x = floorf(child->screen_size.x);
                }
            }
        }
    } break;
    }

    //
    // Solve vertical violations.
    //

    UI_Violation_Resolution vertical_resolution = get_ui_violation_resolution_rule(children_screen_size.y, parent->screen_size.y, parent->layout_direction == UI_DIRECTION_Vertical, parent->flags);

    switch(vertical_resolution) {
    case UI_VIOLATION_RESOLUTION_Align_To_Pixels: {
        for(auto *child = parent->first_child; child != null; child = child->next) {
            child->screen_size.y = floorf(child->screen_size.y);
        }
    } break;
        
    case UI_VIOLATION_RESOLUTION_Cap_At_Parent_Size: {
        for(auto *child = parent->first_child; child != null; child = child->next) {
            child->screen_size.y = floorf(min(parent->screen_size.y, child->screen_size.y));
        }
    } break;
        
    case UI_VIOLATION_RESOLUTION_Squish: {
        f32 subpixel = 0; // :UISubpixelTracking

        if(children_fixup_budget.y > 0.0f) {
            for(auto *child = parent->first_child; child != null; child = child->next) {
                if((child->flags & UI_Floating) == 0) {
                    f32 child_fixup_budget = ((1 - child->semantic_height.strictness) * child->screen_size.y);
                    f32 child_fixup_size   = min(child_fixup_budget * (screen_size_violation.y / children_fixup_budget.y), child_fixup_budget);
                    child->screen_size.y  -= child_fixup_size;

                    subpixel += child->screen_size.y - floorf(child->screen_size.y);
                    child->screen_size.y = floorf(child->screen_size.y) + floorf(subpixel + 0.05f);
                    subpixel -= floorf(subpixel + 0.05f);
                } else {
                    child->screen_size.y = floorf(child->screen_size.y);
                }
            }
        }
    } break;
    }

    //
    // Recursively resolve screen size violations for the children of this element
    //
    {
        for(auto *child = parent->first_child; child != null; child = child->next) {
            if(child->first_child) solve_screen_size_violations_recursively(child);
        }
    }
}

static
void position_and_update_element_recursively(UI *ui, UI_Element *element, UI_Rect parent_rect, UI_Vector2 *cursor, f32 frame_time) {
    //
    // Position this element.
    //
    if(element->signals & UI_SIGNAL_Dragged && ui->window->mouse_active_this_frame) {
        // If the element is currently being dragged, update the dragging offset. This should be done before
        // the actual element is positioned to avoid any latency in the dragging.
        set_element_drag(ui, element);
    }

    if(element->flags & UI_Floating) {
        // Floating and draggable elements are offset from the cursor position by their drag vector from the last
        // frame.
        // Every draggable element needs to be floating, but non-draggable elements may have a custom drag vector
        // (e.g. to be positioned under the mouse cursor).
        if(element->flags & UI_Draggable) {
            // See set_ui_elemnt_drag.
            if(element->parent->screen_size.x < element->screen_size.x) {
                element->float_vector.x = ((element->screen_size.x - element->parent->screen_size.x) / 2) / element->screen_size.x;
            }

            if(element->parent->screen_size.y < element->screen_size.y) {
                element->float_vector.y = ((element->screen_size.y - element->parent->screen_size.y) / 2) / element->screen_size.y;
            }
        }

        UI_Vector2 available_parent_size = element->parent->screen_size;

        if(!(element->flags & UI_Drag_On_Screen_Space)) {
            available_parent_size.x -= element->screen_size.x;
            available_parent_size.y -= element->screen_size.y;
        }

        element->screen_position.x = element->parent->screen_position.x + roundf(element->float_vector.x * available_parent_size.x);
        element->screen_position.y = element->parent->screen_position.y + roundf(element->float_vector.y * available_parent_size.y);
    } else {
        // Non-floating elements are just set to the current cursor position
        element->screen_position = *cursor;

        // Potentially center the element in its parent's space in the non-layout direction if that was requested.
        if((element->parent->flags & UI_Center_Children) && element->parent->layout_direction == UI_DIRECTION_Horizontal) {
            element->screen_position.y = element->parent->screen_position.y + roundf((element->parent->screen_size.y - element->screen_size.y) / 2);
        } else if((element->parent->flags & UI_Center_Children) && element->parent->layout_direction == UI_DIRECTION_Vertical) {
            element->screen_position.x = element->parent->screen_position.x + roundf((element->parent->screen_size.x - element->screen_size.x) / 2);
        }

        // Increase the cursor position.
        if(element->parent->layout_direction == UI_DIRECTION_Horizontal) {
            cursor->x += element->screen_size.x;
        } else if(element->parent->layout_direction == UI_DIRECTION_Vertical) {
            cursor->y += element->screen_size.y;
        }
    }

    // Reset the volatile signals before the children are updated, since the children may set the Subtree_Hovered
    // flag already.
    b8 hovered_last_frame = !!(element->signals & UI_SIGNAL_Hovered);
    element->signals = (element->signals & UI_SIGNAL_Active) | (element->signals & UI_SIGNAL_Dragged);

    //
    // Debug print this element.
    //
#if UI_DEBUG_PRINT
    debug_print_element(element);
#endif


    //
    // Recursively position and update all children of this widget.
    // This order ensures that interaction works in the same way as rendering, where children will receive
    // interaction before their parents, and they are drawn on top of their parents.
    // It is only "necessary" in cases where both the parents and their children are interactable, which
    // rarely happens (e.g. the close button in the window header).
    //
    UI_Vector2 child_cursor = element->screen_position;
    UI_Rect child_rect = calculate_rect_for_element_children(ui, element, parent_rect);

    //
    // If the children of this element should be extruded (outside of the parent element), then set the cursor
    // to be just outside of this element for the first child.
    // If the semantic size in the layout direction is the sum of its children, apply the appropriate padding.
    //
    if(element->layout_direction == UI_DIRECTION_Horizontal) {
        if(element->flags & UI_Extrude_Children)     child_cursor.x += element->screen_size.x;
        if(element->flags & UI_View_Scroll_Children) child_cursor.x += element->view_scroll_screen_offset.x;
    } else if(element->layout_direction == UI_DIRECTION_Vertical) {
        if(element->flags & UI_Extrude_Children)     child_cursor.y += element->screen_size.y;
        if(element->flags & UI_View_Scroll_Children) child_cursor.y += element->view_scroll_screen_offset.y;
    }

    //
    // If a semantic size depends on the sum of the children, the value of that size is a border around the
    // children, which should be applied here.
    //
    if(element->semantic_width.tag == UI_SEMANTIC_SIZE_Sum_Of_Children)   child_cursor.x += roundf(element->semantic_width.value / 2);
    if(element->semantic_height.tag == UI_SEMANTIC_SIZE_Sum_Of_Children)  child_cursor.y += roundf(element->semantic_height.value / 2);

    // Reset the accumulated screen size for this frame so that the children can re-calculate it.
    if(element->flags & UI_View_Scroll_Children) element->view_scroll_screen_size = { 0, 0 };

    // Actually position and update the children.
    for(auto *child = element->first_child; child != null; child = child->next) {
        position_and_update_element_recursively(ui, child, child_rect, &child_cursor, frame_time);
    }


    //
    // Handle interactions for this element.
    //

    // Check for mouse hover over this element
    UI_Rect element_rect = { element->screen_position.x, element->screen_position.y, element->screen_position.x + element->screen_size.x - 1, element->screen_position.y + element->screen_size.y - 1 };
    b8 mouse_over_element = ui_mouse_over_rect(ui, element_rect) && ui_mouse_over_rect(ui, parent_rect);
    
    b8 element_hovered = mouse_over_element && !ui->hovered_element_found && ((!(ui->window->buttons[BUTTON_Left] & BUTTON_Down) && !(ui->window->buttons[BUTTON_Left] & BUTTON_Released)) || ui->window->buttons[BUTTON_Left] & BUTTON_Pressed || element->signals & UI_SIGNAL_Dragged || hovered_last_frame); // For a clickable element to be considered, a few criteria have to be met. First up, no element can already be hovered during this frame, Secondly, the mouse actually has to be on top of the element. Thirdly, the mouse cannot be dragged upon this element (except if this element is already being dragged...)
    
    if(!ui->hovered_element_found && mouse_over_element) set_element_parent_tree_to_hovered(element->parent); // If the mouse is over this element, then the parent tree is considered to be hovered visually, whether or not this element actually cares about being hovered right now. This prevents weird glitches where small gaps between buttons on a window background make that window not be hovered.
    
    if(element_hovered && element->flags & UI_Clickable) {
        // Trigger the animation to display the 'hovered' status.
        element->signals |= UI_SIGNAL_Hovered;
        ui->hovered_element_found = true;    
    } else if(element_hovered && !(element->flags & UI_Clickable)) {
        // If this element is not actually clickable but it is currently being hovered, then we have
        // found our hovered element. We do this after all children have been updated, so as to not
        // take away input from our children. This prevents these glitches where hovering over the
        // border of a window still activates elements under the window.
        ui->hovered_element_found = true;
    }
    
    if(element->flags & UI_Clickable && element->signals & UI_SIGNAL_Hovered && ui->window->buttons[BUTTON_Left] & BUTTON_Released) {
        // If the element is currently hovered and the left button was released, this element is
        // considered clicked
        element->signals |= UI_SIGNAL_Clicked;
    }

    if(element->flags & UI_Activatable) {
        if(element->signals & UI_SIGNAL_Clicked) {
            // Toggle the active flag if the widget has been clicked
            element->signals ^= UI_SIGNAL_Active;
        }
        
        if(element->flags & UI_Deactivate_Automatically_On_Click && ui->window->buttons[BUTTON_Left] & BUTTON_Released && !(element->signals & UI_SIGNAL_Clicked)) {
            // If the left button has been released somewhere not on this element, deactivate it
            element->signals &= ~UI_SIGNAL_Active;
        }
    }
    
    if(element->flags & UI_Draggable) {
        if(element->signals & UI_SIGNAL_Hovered && ui->window->buttons[BUTTON_Left] & BUTTON_Pressed) {
            // Enter the dragging state for this element if it has been clicked
            element->signals |= UI_SIGNAL_Dragged;
            element->drag_offset = { ui->window->mouse_x - element->screen_position.x, ui->window->mouse_y - element->screen_position.y };
        }
        
        if(!(ui->window->buttons[BUTTON_Left] & BUTTON_Down)) {
            // Exit the dragging state if the left mouse button is not held down
            element->signals &= ~UI_SIGNAL_Dragged;
        }
    }
    
    if(element->flags & UI_View_Scroll_Children) {
        if(mouse_over_element) {
            // Scroll the content along the axis if the mouse wheel is turned.
            if(element->layout_direction == UI_DIRECTION_Horizontal && element->screen_size.x < element->view_scroll_screen_size.x) {
                element->view_scroll_screen_offset.x = clamp(element->view_scroll_screen_offset.x - ui->window->mouse_wheel_turns * 32, 0, element->view_scroll_screen_size.x - element->screen_size.x);
            } else if(element->layout_direction == UI_DIRECTION_Vertical && element->screen_size.y < element->view_scroll_screen_size.y) {
                element->view_scroll_screen_offset.y = clamp(element->view_scroll_screen_offset.y - ui->window->mouse_wheel_turns * 32, 0, element->view_scroll_screen_size.y - element->screen_size.y);
            }
        } else {
            // Ensure that the view scroll offset in pixels is actually valid, in case the size
            // of the scrollable content or this element's screen size changed.
            if(element->layout_direction == UI_DIRECTION_Horizontal) {
                if(element->screen_size.x < element->view_scroll_screen_size.x) {
                    element->view_scroll_screen_offset.x = clamp(element->view_scroll_screen_offset.x, 0, element->view_scroll_screen_size.x - element->screen_size.x);
                } else {
                    element->view_scroll_screen_offset.x = 0;
                }
            } else {
                if(element->screen_size.y < element->view_scroll_screen_size.y) {
                    element->view_scroll_screen_offset.y = clamp(element->view_scroll_screen_offset.y, 0, element->view_scroll_screen_size.y - element->screen_size.y);
                } else {
                    element->view_scroll_screen_offset.y = 0;  
                }
            }
        }
    }
    
    // If the parent is a View Scroll, then increase the parent's view_scroll_screen_size. This acts
    // similar to the Sum_Of_Children semantic size tag.
    if(element->parent->flags & UI_View_Scroll_Children) {
        if(element->parent->layout_direction == UI_DIRECTION_Horizontal) element->parent->view_scroll_screen_size.x += element->screen_size.x;
        if(element->parent->layout_direction == UI_DIRECTION_Vertical)   element->parent->view_scroll_screen_size.y += element->screen_size.y;
    }


    //    
    // If this element has a draggable child (and the appropriate flag is set), position that draggable
    // element under the cursor.
    //

    if(element->flags & UI_Snap_Draggable_Children_On_Click && element_hovered && ui->window->buttons[BUTTON_Left] & BUTTON_Pressed) {
        for(auto *child = element->first_child; child != null; child = child->next) {
            if(child->flags & UI_Draggable) {
                // Snap the center of the child element to the cursor
                child->drag_offset.x = child->screen_size.x * 0.5f;
                child->drag_offset.y = child->screen_size.y * 0.5f;
                
                child->signals |= UI_SIGNAL_Hovered | UI_SIGNAL_Clicked | UI_SIGNAL_Dragged;
                child->hover_t = 1;
                
                set_element_drag(ui, child);
            }
        }
    }    
    
    
    //
    // Update the different transition animations.
    //

    if(element->signals & UI_SIGNAL_Hovered || element->signals & UI_SIGNAL_Dragged) {
        element->hover_t = min(element->hover_t + frame_time * UI_COLOR_TRANSITION_SPEED, 1.f);
    } else {
        element->hover_t = max(element->hover_t - frame_time * UI_COLOR_TRANSITION_SPEED, 0.f);
    }
    
    if(element->signals & UI_SIGNAL_Active) {
        element->active_t = min(element->active_t + frame_time * UI_COLOR_TRANSITION_SPEED, 1.f);
    } else {
        element->active_t = max(element->active_t - frame_time * UI_COLOR_TRANSITION_SPEED, 0.f);
    }
    
    if((element->flags & UI_Animate_Size_On_Activation && element->signals & UI_SIGNAL_Clicked) ||
        (element->flags & UI_Animate_Size_On_Hover && element->signals & UI_SIGNAL_Hovered && !hovered_last_frame)) {
        element->size_t = 1;
    }
    
    element->size_t = max(element->size_t - frame_time * UI_SIZE_TRANSITION_SPEED, 0.f);
}

static
void layout_ui(UI *ui, f32 frame_time) {
    // Make sure the user of this module did not forget some ui_pop... call somewhere, because that
    // will lead to issues sooner or later...
    assert(ui->semantic_width_stack.count == 1, "UI Semantic Width Stack is not empty, invalid use.");
    assert(ui->semantic_height_stack.count == 1, "UI Semantic Height Stack is not empty, invalid use.");
    assert(ui->parent_stack.count == 1, "UI Parent Stack is not empty, invalid use.");
        
#if UI_DEBUG_PRINT
    string layout_direction_string = ui_direction_to_string(ui->root.layout_direction);
    printf("============ UI FRAME ============\n");
    printf("  Screen Size: %f, %f | Layout: %.*s\n", ui->root.screen_size.x, ui->root.screen_size.y, (u32) layout_direction_string.count, layout_direction_string.data);
#endif

    // Calculate standalone sizes
    for(auto *element = ui->root.first_child; element != null; element = element->next) {
        calculate_standalone_screen_size_for_element_recursively(element);
    }
    
    // Calculate upwards dependent sizes
    for(auto *element = ui->root.first_child; element != null; element = element->next) {
        calculate_upwards_dependent_screen_size_for_element_recursively(element);
    }
    
    // Calculate downwards dependent sizes
    for(auto *element = ui->root.first_child; element != null; element = element->next) {
        calculate_downwards_dependent_screen_size_for_element_recursively(element);
    }
    
    // Resolve all screen size violations.
    solve_screen_size_violations_recursively(&ui->root);

    // Position and update all elements.
    UI_Vector2 cursor = { };
    UI_Rect screen_rect = { 0, 0, ui->root.screen_size.x - 1, ui->root.screen_size.y - 1 };
    for(auto *element = ui->root.first_child; element != null; element = element->next) {
        position_and_update_element_recursively(ui, element, screen_rect, &cursor, frame_time);
    }

    // If there is currently an active text input, update that with the text input information
    // supplied by the user.
    if(ui->active_text_input) {
        update_text_input(ui->active_text_input, ui->window, ui->font);
    }

    // If any ui element has been hovered during this frame, store that result in the UI structure.
    // We cannot use the root element's signals for this, as that will be cleared out during the
    // next frame.
    ui->hovered_last_frame = ui->root.signals & UI_SIGNAL_Subtree_Hovered;

#if UI_DEBUG_PRINT
    printf("============ UI FRAME ============\n");
#endif
}



/* ------------------------------------------------- Drawing ------------------------------------------------- */

static
void draw_text_input(UI *ui, Text_Input *text_input, UI_Vector2 screen_position, UI_Color background_color) {
    // Query the complete text to render
    string text = text_input_string_view(text_input);
    
    // Query the text until the cursor for rendering alignment
    string text_until_cursor = text_input_string_view_until_cursor(text_input);
    f32 width_until_cursor   = (f32) get_string_width_in_pixels(ui->font, text_until_cursor);
    
    UI_Color text_color = ui->theme.text_color;
    if(ui->deactivated) text_color.a /= UI_DEACTIVE_ALPHA_DENOMINATOR;
    
    if(text_input->selection_active) {
        // There is currently an active selection. Render the selection background with a specific color
        // under the location where the selected text will be rendered later.
        UI_Color selection_color = { 73, 149, 236, 255 };
        if(ui->deactivated) selection_color.a /= UI_DEACTIVE_ALPHA_DENOMINATOR;
        
        string text_until_selection = text_input_string_view_until_selection(text_input);
        string selection_text       = text_input_selected_string_view(text_input);
        string text_after_selection = text_input_string_view_after_selection(text_input);
        
        f32 selection_offset = (f32) get_string_width_in_pixels(ui->font, text_until_selection);
        f32 selection_width  = (f32) get_string_width_in_pixels(ui->font, selection_text);

        // Draw the actual selection background.        
        UI_Vector2 selection_top_left = { screen_position.x + selection_offset, screen_position.y - ui->font->ascender };
        UI_Vector2 selection_bottom_right = { screen_position.x + selection_offset + selection_width, screen_position.y + ui->font->descender };
        ui->callbacks.draw_quad(ui->callbacks.user_pointer, selection_top_left, selection_bottom_right, 0, selection_color);
        
        // Since the background color for the selected part of the input is different, we need to
        // split the text rendering into three parts and advance the cursor accordingly.
        ui->callbacks.draw_text(ui->callbacks.user_pointer, text_until_selection, screen_position, text_color, background_color);
        ui->callbacks.draw_text(ui->callbacks.user_pointer, selection_text, { screen_position.x + selection_offset, screen_position.y }, text_color, selection_color);
        ui->callbacks.draw_text(ui->callbacks.user_pointer, text_after_selection, { screen_position.x + selection_offset + selection_width, screen_position.y }, text_color, background_color);
    } else {
        // Render the actual input text with the default background color.
        ui->callbacks.draw_text(ui->callbacks.user_pointer, text, screen_position, text_color, background_color);
    }
    
    if(text_input->active_this_frame) {
        // Calculate the cursor size
        UI_Vector2 cursor_size = { (f32) get_character_width_in_pixels(ui->font, 'M'), (f32) ui->font->line_height };
        if(text_input->cursor != text_input->count) cursor_size.x = 2;
        
        // Render the cursor.
        UI_Vector2 cursor_top_left = { screen_position.x + text_input->cursor_x, screen_position.y - ui->font->ascender };
        UI_Vector2 cursor_bottom_right = { cursor_top_left.x + cursor_size.x, cursor_top_left.y + cursor_size.y };
        
        UI_Color cursor_color = { ui->theme.text_color.r, ui->theme.text_color.g, ui->theme.text_color.b, (u8) (text_input->cursor_alpha_zero_to_one * 255) };
        if(ui->deactivated) cursor_color.a /= UI_DEACTIVE_ALPHA_DENOMINATOR;
        ui->callbacks.draw_quad(ui->callbacks.user_pointer, cursor_top_left, cursor_bottom_right, 0, cursor_color);        
    } else if(text.count == 0 && text_input->tool_tip.count) {
        // Render the tool tip if the text input is not active, and no text is currently in the buffer
        ui->callbacks.draw_text(ui->callbacks.user_pointer, text_input->tool_tip, screen_position, ui_multiply_color(text_color, 0.5f), background_color);
    }
}

static
void draw_element_recursively(UI *ui, UI_Element *element, UI_Rect parent_rect) {
    //
    // Animate the visual size of this element on specific transitions if that was requested.
    //
    UI_Vector2 drawn_top_left     = element->screen_position;
    UI_Vector2 drawn_bottom_right = { element->screen_position.x + element->screen_size.x, element->screen_position.y + element->screen_size.y };
    
    if(element->flags & UI_Animate_Size_On_Activation || element->flags & UI_Animate_Size_On_Hover) {
        f32 factor = ui_size_animation_factor(element->size_t);
        drawn_top_left.x     -= element->screen_size.x * factor * 0.5f;
        drawn_top_left.y     -= element->screen_size.y * factor * 0.5f;
        drawn_bottom_right.x += element->screen_size.x * factor * 0.5f;
        drawn_bottom_right.y += element->screen_size.y * factor * 0.5f;
    }
    
    UI_Vector2 drawn_size = { drawn_bottom_right.x - drawn_top_left.x, drawn_bottom_right.y - drawn_top_left.y };
    
    //
    // Overlap the parent's and the element's rectangle to find out what part of the element should
    // actually be drawn. This is in sync with what position_and_update_element_recursively does.
    // It enables us to only partly render ui elements that are almost out of view in a scrollable
    // area.
    //
    UI_Rect element_rect = { drawn_top_left.x, drawn_top_left.y, drawn_bottom_right.x, drawn_bottom_right.y };
    UI_Rect visible_rect = { max(parent_rect.x0, element_rect.x0), max(parent_rect.y0, element_rect.y0), min(parent_rect.x1, element_rect.x1), min(parent_rect.y1, element_rect.y1) };
    
    b8 visible_rect_is_not_empty = visible_rect.x1 >= visible_rect.x0 && visible_rect.y1 >= visible_rect.y0;
    b8 visible_rect_is_on_screen = (visible_rect.x0 < ui->root.screen_size.x && visible_rect.y0 < ui->root.screen_size.y) && (visible_rect.x1 >= 0 && visible_rect.y1 >= 0);
    
    //
    // Make sure the element is at least partially visible. It might not be if it is part of a
    // scroll view, or if it is part of a window that is moved off-screen.
    //
    if(visible_rect_is_not_empty && visible_rect_is_on_screen) {
        // Set the scissors.
        ui->callbacks.set_scissors(ui->callbacks.user_pointer, visible_rect);
        
        // Figure out the animated colors.
        f32 t = clamp(element->hover_t - element->active_t * 0.5f, 0, 1);
        UI_Color active_color = ui_mix_colors(element->default_color, element->active_color, element->active_t);
        UI_Color final_color = ui_mix_colors(active_color, element->hovered_color, t);
        if(ui->deactivated) final_color.a /= UI_DEACTIVE_ALPHA_DENOMINATOR;
        
        //
        // Render this element according to the calculated screen layout and set flags.
        //
        
        if(element->flags & UI_Background) {
            f32 pixel_rounding = min(drawn_size.x, drawn_size.y) * element->rounding / 2;
            ui->callbacks.draw_quad(ui->callbacks.user_pointer, drawn_top_left, drawn_bottom_right, pixel_rounding, final_color);
        }
        
        if(element->flags & UI_Border && ui->theme.border_size > 0) {
            // Draw the border using 4 separate quads.
            UI_Color lit_border_color = ui->theme.border_color;
            if(ui->deactivated) lit_border_color.a /= UI_DEACTIVE_ALPHA_DENOMINATOR;
            
            UI_Color unlit_border_color = lit_border_color;
            if(ui->theme.lit_border) {
                unlit_border_color.r = (u8) ((f32) unlit_border_color.r * 0.85f);
                unlit_border_color.g = (u8) ((f32) unlit_border_color.g * 0.85f);
                unlit_border_color.b = (u8) ((f32) unlit_border_color.b * 0.85f);
            }
     
            ui->callbacks.draw_quad(ui->callbacks.user_pointer, { drawn_top_left.x, drawn_top_left.y }, { drawn_top_left.x + ui->theme.border_size, drawn_bottom_right.y }, 0, unlit_border_color); // Left border
            ui->callbacks.draw_quad(ui->callbacks.user_pointer, { drawn_top_left.x, drawn_bottom_right.y - ui->theme.border_size }, { drawn_bottom_right.x, drawn_bottom_right.y }, 0, unlit_border_color); // Bottom border
            ui->callbacks.draw_quad(ui->callbacks.user_pointer, { drawn_bottom_right.x - ui->theme.border_size, drawn_top_left.y }, { drawn_bottom_right.x, drawn_bottom_right.y }, 0, unlit_border_color); // Right border
            ui->callbacks.draw_quad(ui->callbacks.user_pointer, { drawn_top_left.x, drawn_top_left.y }, { drawn_bottom_right.x, drawn_top_left.y + ui->theme.border_size }, 0, unlit_border_color); // Top border
        }
        
        if(element->flags & UI_Label && element->label.count) {
            // Render the label
            UI_Vector2 label_position;

            if(element->flags & UI_Center_Label) {
                f32 centered_baseline = floorf(drawn_size.y / 2 + (ui->font->ascender - ui->font->descender) / 2);
                label_position.x = drawn_top_left.x + roundf((drawn_size.x - element->label_size.x) / 2);
                label_position.y = drawn_top_left.y + min(drawn_size.y, centered_baseline);
            } else {
                f32 low_baseline = floorf(drawn_size.y - 2 - ui->font->line_height + ui->font->ascender);
                label_position.x = drawn_top_left.x;
                label_position.y = drawn_top_left.y + min(drawn_size.y, low_baseline);
            }
            
            UI_Color text_color = ui->theme.text_color;
            if(ui->deactivated) text_color.a /= UI_DEACTIVE_ALPHA_DENOMINATOR;
            
            ui->callbacks.draw_text(ui->callbacks.user_pointer, element->label, label_position, text_color, final_color);
        }
        
        if(element->flags & UI_Text_Input) {
            // Render the text input
            f32 low_baseline = floorf(drawn_size.y - 2 - ui->font->line_height + ui->font->ascender);
            UI_Vector2 text_input_position = { drawn_top_left.x + 5, drawn_top_left.y + min(drawn_size.y, low_baseline) };
            draw_text_input(ui, element->text_input, text_input_position, final_color);
        }
        
        if(element->flags & UI_Custom_Drawing_Procedure) {
            element->custom_draw(ui->callbacks.user_pointer, element, element->custom_state);
        }
    }

#if UI_DEBUG_DRAW
    // Debug draw the screen space of this element with a color encoded by the hash value of this
    // element to visually show the element layout on screen
    UI_Color hash_color = { (u8) (element->hash >> 24), (u8) (element->hash >> 16), (u8) (element->hash >> 8), 255 };
    ui->callbacks.draw_quad(ui->callbacks.user_pointer, drawn_top_left, drawn_bottom_right, 0, hash_color);
#endif

    //
    // Recursively render the children of this element in reverse order, so that the elements that
    // received input first are rendered last (on top of everything else).
    // Similar to how wehandled input, we pass along the parent_rect struct, to figure out the visible
    // part of what should be rendered.
    // This is mainly required for the View_Scroll_Children flag to ignore everything that is outside
    // of the view region.
    //
    UI_Rect child_rect = calculate_rect_for_element_children(ui, element, parent_rect);

    for(auto *child = element->last_child; child != null; child = child->prev) {
        draw_element_recursively(ui, child, child_rect);
    }
}



/* ------------------------------------------------ Setup API ------------------------------------------------ */

void create_ui(UI *ui, UI_Callbacks callbacks, UI_Theme theme, Window *window, Font *font) {
    ui->callbacks    = callbacks;
    ui->theme        = theme;
    ui->window       = window;
    ui->font         = font;

    ui->arena.create(1 * ONE_MEGABYTE);
    ui->allocator = ui->arena.allocator();
    
    ui->text_input_pool.allocator = Default_Allocator; // We reset the UI memory arena every frame at a fixed position, so we cannot use it for dynamic allocation of this list.
    ui->elements         = (UI_Element *) ui->allocator.allocate(UI_ELEMENT_CAPACITY * sizeof(UI_Element));
    ui->element_capacity = UI_ELEMENT_CAPACITY;
    ui->element_count    = 0;
    ui->arena_frame_mark = ui->arena.mark();
    
    ui->root = { };

    ui->active_text_input     = null;
    ui->hovered_last_frame    = false;
    ui->hovered_element_found = false;
    ui->deactivated           = false;
    ui->font_changed          = false;
}

void destroy_ui(UI *ui) {
    for(u64 i = 0; i < ui->element_count; ++i) {
        if(ui->elements[i].custom_state) Default_Allocator->deallocate(ui->elements[i].custom_state);
    }

    ui->text_input_pool.clear();
    ui->allocator.deallocate(ui->elements);
    ui->arena.destroy();
}

void change_ui_font(UI *ui, Font *font) {
    ui->font = font;
    ui->font_changed = true;
}

void begin_ui_frame(UI *ui, UI_Vector2 default_size) {
    // Prepare the input status
    ui->hovered_element_found = false;
    ui->root.screen_position = { 0, 0 };
    ui->root.screen_size = { (f32) ui->window->w, (f32) ui->window->h };

    // Prepare the UI stacks.
    clear_ui_stack(&ui->parent_stack);
    clear_ui_stack(&ui->semantic_width_stack);
    clear_ui_stack(&ui->semantic_height_stack);
    
    // Set up the default styling for this frame
    ui_push_width(ui, UI_SEMANTIC_SIZE_Pixels, default_size.x, 0.6f);
    ui_push_height(ui, UI_SEMANTIC_SIZE_Pixels, default_size.y, 0.6f);
    
    ui->one_pass_semantic_width  = false;
    ui->one_pass_semantic_height = false;
    
    // Set up the root element as the first implicit element for this frame, so that there is always
    // something in the UI element tree for linkage.
    reset_element(ui, &ui->root);
    ui->root.signals = UI_SIGNAL_None;
    ui->last_element = &ui->root;
    ui_push_parent(ui, UI_DIRECTION_Horizontal);
}

void draw_ui_frame(UI *ui) {
    // Do the actual layout algorithm now that the frame is complete.
    layout_ui(ui, ui->window->frame_time);

    // Render all elements in the tree (reverse, for better depth testing) order.
    UI_Rect root_rect = { ui->root.screen_position.x, ui->root.screen_position.y, ui->root.screen_position.x + ui->root.screen_size.x, ui->root.screen_position.y + ui->root.screen_size.y };
    draw_element_recursively(ui, &ui->root, root_rect);

    // Reset the scissors which have been set by the individual elements for easier integration into user code.
    ui->callbacks.clear_scissors(ui->callbacks.user_pointer);

    // Remove all spacers that the UI is not supposed to cache (since they do not have any state). Also remove
    // elements that were not used in the last frame to free up space.
    for(u64 i = 0; i < ui->element_count; ) {
        auto &element = ui->elements[i];

        if(element.hash == UI_NULL_HASH || !element.used_this_frame) {
            // Remove from the element array
            if(element.text_input) {
                if(ui->active_text_input == element.text_input) ui->active_text_input = null;
                ui->text_input_pool.remove_value_pointer(element.text_input);
            }

            if(element.custom_state) {
                Default_Allocator->deallocate(element.custom_state);
            }

            memcpy(&ui->elements[i], &ui->elements[i + 1], (ui->element_count - i - 1) * sizeof(UI_Element));
            --ui->element_count;
        } else {
            element.used_this_frame = false; // Prepare for the next frame
            ++i;
        }
    }

    // Reset the UI status for the next frame.
    ui->font_changed = false;
    ui->arena.release_from_mark(ui->arena_frame_mark);
}



/* ------------------------------------------------ Layout API ------------------------------------------------ */

void ui_push_width(UI *ui, UI_Semantic_Size_Tag tag, f32 value, f32 strictness) {
    push_ui_stack(&ui->semantic_width_stack, { tag, value, strictness });    
}

void ui_push_height(UI *ui, UI_Semantic_Size_Tag tag, f32 value, f32 strictness) {
    push_ui_stack(&ui->semantic_height_stack, { tag, value, strictness });
}

void ui_set_width(UI *ui, UI_Semantic_Size_Tag tag, f32 value, f32 strictness) {
    push_ui_stack(&ui->semantic_width_stack, { tag, value, strictness });    
    ui->one_pass_semantic_width = true;
}

void ui_set_height(UI *ui, UI_Semantic_Size_Tag tag, f32 value, f32 strictness) {
    push_ui_stack(&ui->semantic_height_stack, { tag, value, strictness });    
    ui->one_pass_semantic_height = true;
}

void ui_pop_width(UI *ui) {
    pop_ui_stack(&ui->semantic_width_stack);
}

void ui_pop_height(UI *ui) {
    pop_ui_stack(&ui->semantic_height_stack);
}

void ui_push_parent(UI *ui, UI_Direction layout_direction) {
    push_ui_stack(&ui->parent_stack, ui->last_element);
    ui->last_element->layout_direction = layout_direction;
}

void ui_pop_parent(UI *ui) {
    ui->last_element = pop_ui_stack(&ui->parent_stack);
}

void ui_horizontal_layout(UI *ui) {
    ui->last_element->layout_direction = UI_DIRECTION_Horizontal;
}

void ui_vertical_layout(UI *ui) {
    ui->last_element->layout_direction = UI_DIRECTION_Vertical;
}



/* ---------------------------------------------- Basic Widgets ---------------------------------------------- */

UI_Element *ui_element(UI *ui, UI_Hash hash, string label, UI_Flags flags) {
    UI_Element *element      = hash != UI_NULL_HASH ? insert_element_with_hash(ui, hash, label, flags) : insert_new_element(ui, UI_NULL_HASH, label, flags);
    element->semantic_width  = ui_query_width(ui);
    element->semantic_height = ui_query_height(ui);
    element->default_color   = ui->theme.default_color;
    element->hovered_color   = ui->theme.hovered_color;
    element->active_color    = ui->theme.accent_color;
    return element;
}

UI_Element *ui_element(UI *ui, string label, UI_Flags flags) {
    return ui_element(ui, ui_hash(ui, label), label, flags);
}

void ui_spacer(UI *ui) {
    ui_element(ui, UI_NULL_HASH, ""_s, UI_Spacer);
}

void ui_spacer(UI *ui, UI_Semantic_Size_Tag width_tag, f32 width_value, f32 width_strictness, UI_Semantic_Size_Tag height_tag, f32 height_value, f32 height_strictness) {
    UI_Element *element      = ui_element(ui, UI_NULL_HASH, ""_s, UI_Spacer);
    element->semantic_width  = { width_tag, width_value, width_strictness };
    element->semantic_height = { height_tag, height_value, height_strictness };
}

void ui_divider(UI *ui, b8 visual) {
    UI_Element *parent = query_ui_stack(&ui->parent_stack);
    UI_Direction parent_direction = parent->layout_direction;
    
    if(visual) {
        if(parent_direction == UI_DIRECTION_Horizontal) {
            ui_push_width(ui, UI_SEMANTIC_SIZE_Pixels, 4.f, 1.f);
            ui_spacer(ui);
            
            UI_Element *element     = ui_element(ui, UI_NULL_HASH, ""_s, UI_Background);
            element->semantic_width = { UI_SEMANTIC_SIZE_Pixels, 2, 1 };
            element->default_color  = ui->theme.text_color;
            
            ui_spacer(ui);
            ui_pop_width(ui);                    
        } else if(parent_direction == UI_DIRECTION_Vertical) {
            ui_push_height(ui, UI_SEMANTIC_SIZE_Pixels, 4.f, 1.f);
            ui_spacer(ui);
            
            UI_Element *element      = ui_element(ui, UI_NULL_HASH, ""_s, UI_Background);
            element->semantic_height = { UI_SEMANTIC_SIZE_Pixels, 2, 1 };
            element->default_color   = ui->theme.text_color;
            
            ui_spacer(ui);
            ui_pop_height(ui);
        }
    } else {
        if(parent_direction == UI_DIRECTION_Horizontal) {            
            UI_Element *element      = ui_element(ui, UI_NULL_HASH, ""_s, UI_Spacer);
            element->semantic_height = { UI_SEMANTIC_SIZE_Pixels, 10, 1 };
        } else if(parent_direction == UI_DIRECTION_Vertical) {
            UI_Element *element     = ui_element(ui, UI_NULL_HASH, ""_s, UI_Spacer);
            element->semantic_width = { UI_SEMANTIC_SIZE_Pixels, 10, 1 };
        }
    }
}


void ui_label(UI *ui, b8 centered, string label) {
    UI_Flags flags = UI_Label;
    if(centered) flags |= UI_Center_Label;
    
    UI_Element *element    = ui_element(ui, UI_NULL_HASH, label, flags);
    element->default_color = get_default_color_recursively(element);
}

b8 ui_button(UI *ui, string label) {
    UI_Element *element = ui_element(ui, label, ui->theme.button_style | UI_Label | UI_Center_Label | UI_Clickable);
    return element->signals & UI_SIGNAL_Clicked;
}

void ui_deactivated_button(UI *ui, string label) {
    UI_Element *element = ui_element(ui, label, ui->theme.button_style | UI_Label | UI_Center_Label);
    element->default_color = { 70, 70, 70, 255 };
    element->hovered_color = element->default_color;
    element->active_color  = element->default_color;
}

b8 ui_toggle_button(UI *ui, string label) {
    UI_Element *element = ui_element(ui, label, ui->theme.button_style | UI_Label | UI_Center_Label | UI_Clickable | UI_Activatable);
    return element->signals & UI_SIGNAL_Active;
}

b8 ui_toggle_button_with_pointer(UI *ui, string label, b8 *active) {
    UI_Element *element = ui_element(ui, label, ui->theme.button_style | UI_Label | UI_Center_Label | UI_Clickable | UI_Activatable);
    
    if(element->signals & UI_SIGNAL_Clicked) {
        *active = element->signals & UI_SIGNAL_Active;
    } else if(*active) {
        element->signals |= UI_SIGNAL_Active;    
    } else {
        element->signals &= ~UI_SIGNAL_Active;
    }
    
    return element->signals & UI_SIGNAL_Clicked;
}

// @Cleanup:
// Maybe only make the background interactable, so that we don't need to press on the teeny tiny 
// button but instead also on the label and shit, and then only use the teeny tiny button for visual
// representation of state?
b8 ui_check_box(UI *ui, string label, b8 *active) {
    //
    // The background is just a non-interactable button
    //
    ui_element(ui, label, UI_Center_Children);
    ui_push_parent(ui, UI_DIRECTION_Horizontal);
    
    //
    // Add the actual toggle box in the left corner of the background.
    //
    ui_push_height(ui, UI_SEMANTIC_SIZE_Pixels, 16, 1);
    ui_push_width(ui, UI_SEMANTIC_SIZE_Pixels, 16, 1);
    
    UI_Element *toggle_box = ui_element(ui, ui_concat_strings(ui, label, "_togglebox"_s), UI_Background | UI_Clickable | UI_Activatable | UI_Animate_Size_On_Activation);
    toggle_box->rounding   = ui->theme.rounding;
    
    if(toggle_box->signals & UI_SIGNAL_Clicked) {
        *active = toggle_box->signals & UI_SIGNAL_Active;
    } else if(*active) {
        toggle_box->signals |= UI_SIGNAL_Active;
        if(toggle_box->created_this_frame) toggle_box->active_t = 1.f;
    } else {
        toggle_box->signals &= ~UI_SIGNAL_Active;
    }
    
    ui_pop_width(ui);
    ui_pop_height(ui);
    
    //
    // Add a little space between the toggle box and the label.
    //
    ui_push_width(ui, UI_SEMANTIC_SIZE_Pixels, 8, 1);
    ui_spacer(ui);
    ui_pop_width(ui);
    
    //
    // Make the vertically centered label
    //
    ui_push_width(ui, UI_SEMANTIC_SIZE_Label_Size, 0, 0);
    ui_label(ui, true, label);
    ui_pop_width(ui);
    
    ui_pop_parent(ui);
    return toggle_box->signals & UI_SIGNAL_Clicked;
}

void ui_draggable_element(UI *ui, string label) {
    ui_element(ui, label, ui->theme.button_style | UI_Label | UI_Center_Label | UI_Clickable | UI_Floating | UI_Draggable);
}

void ui_slider(UI *ui, string label, f32 min, f32 max, f32 *value) {
    // The background is just a non-interactable button
    ui_element(ui, label, UI_Center_Children);
    ui_push_parent(ui, UI_DIRECTION_Horizontal);
    
    // Make the label for the slider
    ui_push_height(ui, UI_SEMANTIC_SIZE_Percentage_Of_Parent, 1, 0);
    ui_push_width(ui, UI_SEMANTIC_SIZE_Label_Size, 5, 1);
    ui_label(ui, false, label);
    ui_pop_width(ui);
    ui_pop_height(ui);
    
    // Make a little gap between the text and the actual slider bar.
    ui_set_width(ui, UI_SEMANTIC_SIZE_Pixels, 10, 1);
    ui_spacer(ui);
    
    // Make the small area on which the actual slider button can be moved
    ui_push_height(ui, UI_SEMANTIC_SIZE_Pixels, 15, 1);
    ui_push_width(ui, UI_SEMANTIC_SIZE_Percentage_Of_Parent, 1, 0);
    ui_element(ui, ui_concat_strings(ui, label, "_sliderarea"_s), UI_Center_Children | UI_Snap_Draggable_Children_On_Click);
    ui_push_parent(ui, UI_DIRECTION_Horizontal);
    
    // Make the little slider button
    UI_Element *slider_button      = ui_element(ui, ui_concat_strings(ui, label, "_sliderbutton"_s), UI_Background | UI_Clickable | UI_Floating | UI_Draggable);
    slider_button->semantic_width  = { UI_SEMANTIC_SIZE_Pixels, 7, 1 };
    slider_button->semantic_height = { UI_SEMANTIC_SIZE_Percentage_Of_Parent, 1, 0 };
    slider_button->default_color   = ui->theme.accent_color;
    slider_button->rounding        = ui->theme.rounding;
    
    if(value != null) {
        if(slider_button->signals & UI_SIGNAL_Dragged) {
            *value = (slider_button->float_vector.x * (max - min) + min);
        } else {
            slider_button->float_vector.x = clamp((*value - min) / (max - min), 0, 1);
        }    
    }
    
    // Make a visible bar in the area on which to drag
    UI_Element *slider_bar      = ui_element(ui, ui_concat_strings(ui, label, "_sliderbar"_s), UI_Background);
    slider_bar->semantic_width  = { UI_SEMANTIC_SIZE_Percentage_Of_Parent, 1, 0 };
    slider_bar->semantic_height = { UI_SEMANTIC_SIZE_Pixels, 1, 0 };
    slider_bar->default_color   = ui->theme.border_color;

    // Pop the slider area
    ui_pop_width(ui);
    ui_pop_height(ui);
    ui_pop_parent(ui);
    
    // Pop the background
    ui_pop_parent(ui);
}

UI_Text_Input_Data ui_text_input(UI *ui, string label, Text_Input_Mode mode) {
    UI_Element *element = ui_element(ui, label, ui->theme.button_style | UI_Clickable | UI_Activatable | UI_Deactivate_Automatically_On_Click | UI_Text_Input);
    
    if(element->created_this_frame) {
        element->text_input = ui->text_input_pool.push();
        create_text_input(element->text_input, mode);
        element->text_input->tool_tip = label;
    }
    
    b8 is_active_text_input = element->text_input == ui->active_text_input;
    
    UI_Text_Input_Data data;
    data.entered = false;
    
    if(element->signals & UI_SIGNAL_Clicked && !is_active_text_input) {
        // If this element has been clicked, active this text input.
        if(ui->active_text_input) toggle_text_input_activeness(ui->active_text_input, false);
        toggle_text_input_activeness(element->text_input, true);
        ui->active_text_input = element->text_input;
    } else if(!(element->signals & UI_SIGNAL_Active) && is_active_text_input) {
        // If this element was the active text input but has since gotten deactivated, then disable
        // the text input
        toggle_text_input_activeness(element->text_input, false);
        ui->active_text_input = null;
    } else if(element->text_input->entered_this_frame && is_active_text_input) {
        // The text input got entered, which automatically deactivates it.
        element->signals ^= UI_SIGNAL_Active;
        toggle_text_input_activeness(element->text_input, false);
        ui->active_text_input = null;
        data.entered = true;
    }

    switch(mode) {
    case TEXT_INPUT_Everything: 
        data._string = text_input_string_view(element->text_input); 
        data.valid = true;
        break;
        
    case TEXT_INPUT_Integer:
        data._integer = string_to_int(text_input_string_view(element->text_input), &data.valid);        
        break;    

    case TEXT_INPUT_Floating_Point:
        data._floating_point = string_to_double(text_input_string_view(element->text_input), &data.valid);        
        break;
    }
    
    return data;
}

b8 ui_text_input_with_string(UI *ui, string label, string *data, Allocator *data_allocator) {
    UI_Text_Input_Data result = ui_text_input(ui, label, TEXT_INPUT_Everything);
    
    if(result.entered && result.valid) {
        *data = copy_string(data_allocator, result._string);
    }
    
    return result.entered && result.valid;
}

b8 ui_text_input_with_pointer(UI *ui, string label, f32 *data) {
    UI_Text_Input_Data result = ui_text_input(ui, label, TEXT_INPUT_Everything);
    
    if(result.entered && result.valid) {
        *data = (f32) result._floating_point;
    }
    
    return result.entered && result.valid;    
}

UI_Custom_Widget_Data ui_custom_widget(UI *ui, string label, UI_Custom_Update_Callback update_procedure, UI_Custom_Draw_Callback draw_procedure, u64 required_custom_state_size) {
    UI_Element *element = ui_element(ui, label, UI_Custom_Drawing_Procedure | UI_Clickable | UI_Draggable); // So that the user code gets all the signals it may want.
    if(!element->custom_state) element->custom_state = Default_Allocator->allocate(required_custom_state_size);
    element->custom_draw = draw_procedure;

    update_procedure(ui, element, element->custom_state);
    
    UI_Custom_Widget_Data data;
    data.custom_state = element->custom_state;
    data.created_this_frame = element->created_this_frame;    
    return data;
}



/* --------------------------------------------- Advanced Widgets --------------------------------------------- */

UI_Window_State ui_push_window(UI *ui, string label, UI_Window_Flags window_flags, UI_Vector2 *position) {
    UI_Flags title_bar_flags = UI_Background | UI_Label | UI_Clickable | UI_Floating | UI_Center_Label | UI_Extrude_Children | UI_Detach_From_Parent;
    
    if(window_flags & UI_WINDOW_Draggable) title_bar_flags |= UI_Draggable;
    
    UI_Element *title_bar_container      = ui_element(ui, label, title_bar_flags);
    title_bar_container->semantic_width  = { UI_SEMANTIC_SIZE_Sum_Of_Children, 0, 1 };
    title_bar_container->semantic_height = { UI_SEMANTIC_SIZE_Pixels, 32, 1 };
    title_bar_container->default_color   = ui->theme.window_title_bar_color;
    
    if(window_flags & UI_WINDOW_Draggable) {
        title_bar_container->hovered_color = ui->theme.hovered_window_title_bar_color;
    } else {
        title_bar_container->hovered_color = ui->theme.window_title_bar_color;
    }
    
    ui_push_parent(ui, UI_DIRECTION_Vertical);
    
    UI_Window_State window_state = UI_WINDOW_Open;
    
    if(window_flags & UI_WINDOW_Collapsable) {
        UI_Element *collapse_button      = ui_element(ui, "_"_s, UI_Background | UI_Label | UI_Clickable | UI_Activatable | UI_Floating | UI_Center_Label);
        collapse_button->semantic_width  = { UI_SEMANTIC_SIZE_Pixels, 24, 1 };
        collapse_button->semantic_height = { UI_SEMANTIC_SIZE_Pixels, 24, 1 };
        collapse_button->default_color   = ui->theme.window_title_bar_button_color;
        collapse_button->hovered_color   = ui->theme.hovered_window_title_bar_color;
        
        if(window_flags & UI_WINDOW_Closeable) {
            collapse_button->float_vector = { (title_bar_container->screen_size.x - 34.0f) / title_bar_container->screen_size.x, 16.0f / title_bar_container->screen_size.y };
        } else {
            collapse_button->float_vector = { (title_bar_container->screen_size.x - 5.0f) / title_bar_container->screen_size.x, 20.0f / title_bar_container->screen_size.y };
        }

        if(collapse_button->signals & UI_SIGNAL_Active) {
            // When collapsing a window, we no longer generate the children elements, therefore the
            // usual Sum_Of_Children semantic size would make the window header extremely samll.
            // Therefore, we just forever retain the current screen size of the window title bar.
            window_state = UI_WINDOW_Collapsed;
            title_bar_container->semantic_width = { UI_SEMANTIC_SIZE_Pixels, title_bar_container->screen_size.x, 0 };
        }
    }

    if(window_flags & UI_WINDOW_Closeable) {
        UI_Element *close_button      = ui_element(ui, "x"_s, UI_Background | UI_Label | UI_Clickable | UI_Floating | UI_Center_Label);
        close_button->semantic_width  = { UI_SEMANTIC_SIZE_Pixels, 24, 1 };
        close_button->semantic_height = { UI_SEMANTIC_SIZE_Pixels, 24, 1 };
        close_button->default_color   = ui->theme.window_title_bar_button_color;
        close_button->hovered_color   = ui->theme.hovered_window_title_bar_color;
        close_button->float_vector    = { (title_bar_container->screen_size.x - 5.0f) / title_bar_container->screen_size.x, 16.0f / title_bar_container->screen_size.y };
    
        if(close_button->signals & UI_SIGNAL_Clicked) window_state = UI_WINDOW_Closed;
    }

    if(title_bar_container->created_this_frame && position != null) title_bar_container->float_vector = *position;

    UI_Flags background_flags = UI_Background;
    if(window_flags & UI_WINDOW_Center_Children) background_flags |= UI_Center_Children;
    
    UI_Element *background = ui_element(ui, "__background"_s, background_flags);
    background->default_color = ui->theme.window_background_color;
    
    if(window_state != UI_WINDOW_Collapsed) {
        // If the window just got closed this frame, then we have already started pushing the window header,
        // and should continue building the body so that the window does not flicker...
        // The window should then not appear at all during the next frame.
        background->semantic_width  = { UI_SEMANTIC_SIZE_Sum_Of_Children, 10, 1 };
        background->semantic_height = { UI_SEMANTIC_SIZE_Sum_Of_Children, 10, 1 };
    } else {
        // Completey hide the window body.
        background->semantic_width  = { UI_SEMANTIC_SIZE_Pixels, 0, 1 };
        background->semantic_height = { UI_SEMANTIC_SIZE_Pixels, 0, 1 };
    }

    ui_push_parent(ui, UI_DIRECTION_Vertical);

    if(position != null) *position = title_bar_container->float_vector;
    
    return window_state;
}

b8 ui_pop_window(UI *ui) {
    ui_pop_parent(ui);
    ui_pop_parent(ui);
    b8 hovered = (ui->last_element->signals & UI_SIGNAL_Subtree_Hovered) || (ui->last_element->signals & UI_SIGNAL_Hovered);
    return hovered && ui->window->buttons[BUTTON_Left] & BUTTON_Pressed;
}

void ui_push_growing_container(UI *ui, UI_Direction direction) {
    UI_Element *element      = ui_element(ui, UI_NULL_HASH, ""_s, UI_Spacer);
    element->semantic_width  = { UI_SEMANTIC_SIZE_Sum_Of_Children, 0, 0 };
    element->semantic_height = { UI_SEMANTIC_SIZE_Sum_Of_Children, 0, 0 };
    ui_push_parent(ui, direction);
}

void ui_push_fixed_container(UI *ui, UI_Direction direction) {
    UI_Element *element = ui_element(ui, UI_NULL_HASH, ""_s, UI_Spacer);
    element->semantic_width = ui_query_width(ui);
    element->semantic_height = ui_query_height(ui);
    ui_push_parent(ui, direction);
}

void ui_pop_container(UI *ui) {
    ui_pop_parent(ui);
}

b8 ui_push_collapsable(UI *ui, string name, b8 open_by_default) {
    //
    // Build the header bar, which includes the toggle button and the label
    //    
    UI_Element *header = ui_element(ui, ui_hash(ui, ui_concat_strings(ui, name, "__collapsable_header"_s)), ""_s, UI_Background | UI_Label | UI_Clickable | UI_Activatable);
    header->default_color = ui->theme.window_title_bar_color;
    header->hovered_color = ui->theme.window_title_bar_color;
    header->active_color  = ui->theme.window_title_bar_color;
    
    if(header->created_this_frame && open_by_default) header->signals |= UI_SIGNAL_Active;
    
    b8 body_expanded = header->signals & UI_SIGNAL_Active;
    
    // We can only set the display label after we know whether the container is actually expanded or not.
    // This is pretty ugly, but I don't know a better way to solve this right now.
    if(body_expanded) {
        header->label = ui_concat_strings(ui, " v "_s, name);
    } else {
        header->label = ui_concat_strings(ui, " > "_s, name);
    }

    header->label_size.x = (f32) get_string_width_in_pixels(ui->font, header->label);
    header->label_size.y = (f32) ui->font->glyph_height;

    //
    // Build the body container, which only has a valid size if the body is currently opened.
    // The opened body then again consists of two elements. The left one is a spacer that indents the
    // children a little to the right for more visual clarity. The other element is then the child container.
    //
    if(body_expanded) {
        // The body container which will grow vertically
        UI_Element *body_container = ui_element(ui, ui_concat_strings(ui, name, "__collapsable_body"_s), UI_Spacer);
        body_container->semantic_height = { UI_SEMANTIC_SIZE_Sum_Of_Children, 0, 1 };        
        ui_push_parent(ui, UI_DIRECTION_Horizontal);
        
        // The little spacer for indentation of the actual children
        ui_spacer(ui, UI_SEMANTIC_SIZE_Pixels, 10, 1, UI_SEMANTIC_SIZE_Pixels, 0, 0);
        
        // The actual child container for the children yet to come.
        UI_Element *children_container = ui_element(ui, ui_concat_strings(ui, name, "__collapsable_children"_s), UI_Spacer);
        children_container->semantic_width = { UI_SEMANTIC_SIZE_Percentage_Of_Parent, 1, 0 };
        children_container->semantic_height = { UI_SEMANTIC_SIZE_Sum_Of_Children, 0, 1 };
        ui_push_parent(ui, UI_DIRECTION_Vertical);
    } else {
        // The body is collapsed, but the ui_pop_collapsable method does not actually know that, so "fake" the
        // body by creating some empty spacer, one for the body_container and one for the children_container.
        ui_spacer(ui, UI_SEMANTIC_SIZE_Pixels, 0, 0, UI_SEMANTIC_SIZE_Pixels, 0, 0);
        ui_push_parent(ui, UI_DIRECTION_Vertical);

        ui_spacer(ui, UI_SEMANTIC_SIZE_Pixels, 0, 0, UI_SEMANTIC_SIZE_Pixels, 0, 0);
        ui_push_parent(ui, UI_DIRECTION_Vertical);
    }
    
    return body_expanded;
}

void ui_pop_collapsable(UI *ui) {
    ui_pop_parent(ui); // Pop the children container    
    ui_pop_parent(ui); // Pop the body container
}

b8 ui_push_dropdown(UI *ui, string label) {
    UI_Element *element = ui_element(ui, label, ui->theme.button_style | UI_Label | UI_Center_Label | UI_Clickable | UI_Extrude_Children);
    ui_push_parent(ui, get_opposite_layout_direction(ui));
    return element->signals & UI_SIGNAL_Active || element->signals & UI_SIGNAL_Hovered || element->signals & UI_SIGNAL_Subtree_Hovered;
}

void ui_pop_dropdown(UI *ui) {
    ui_pop_parent(ui);
}

void ui_push_tooltip(UI *ui, UI_Vector2 screen_space_position) {
    UI_Element *background      = ui_element(ui, "__tooltip"_s, UI_Background | UI_Border | UI_Floating | UI_Drag_On_Screen_Space);
    background->semantic_width  = { UI_SEMANTIC_SIZE_Sum_Of_Children, 0, 1 };
    background->semantic_height = { UI_SEMANTIC_SIZE_Sum_Of_Children, 0, 1 };
    background->drag_offset     = { 0, 0 };
    background->float_vector    = screen_space_position;
    
    ui_push_parent(ui, UI_DIRECTION_Vertical);
}

void ui_pop_tooltip(UI *ui) {
    ui_pop_parent(ui);
}

void ui_push_scroll_view(UI *ui, string label, UI_Direction direction) {
    UI_Direction opposite_direction = get_opposite_layout_direction(direction);
    
    UI_Element *container = ui_element(ui, label, UI_Background | UI_Border);
    container->default_color = { ui->theme.window_background_color.r, ui->theme.window_background_color.g, ui->theme.window_background_color.b, 255 }; // Transparency just looks shit with scroll views.
    ui_push_parent(ui, opposite_direction);
    
    UI_Element *area      = ui_element(ui, "__scrollview_area"_s, UI_View_Scroll_Children);
    area->semantic_width  = { UI_SEMANTIC_SIZE_Percentage_Of_Parent, 1, 0 };
    area->semantic_height = { UI_SEMANTIC_SIZE_Percentage_Of_Parent, 1, 0 };
    ui_push_parent(ui, direction);
}

void ui_pop_scroll_view(UI *ui) {
    UI_Element *area = query_ui_stack(&ui->parent_stack);    
    ui_pop_parent(ui); // Pop the area    
    
    if(area->layout_direction == UI_DIRECTION_Vertical && area->view_scroll_screen_size.y > area->screen_size.y) {
        UI_Element *scrollbar_container      = ui_element(ui, "__scrollview_container"_s, UI_Background | UI_Center_Children);
        scrollbar_container->semantic_width  = { UI_SEMANTIC_SIZE_Pixels, 15, 1 };
        scrollbar_container->semantic_height = { UI_SEMANTIC_SIZE_Percentage_Of_Parent, 1, 0 };
        scrollbar_container->default_color   = { ui->theme.window_background_color.r, ui->theme.window_background_color.g, ui->theme.window_background_color.b, 255 };        
        ui_push_parent(ui, UI_DIRECTION_Vertical); // Center the children horizontally
        
        ui_spacer(ui, UI_SEMANTIC_SIZE_Pixels, 0, 0, UI_SEMANTIC_SIZE_Pixels, 5, 1);
        
        UI_Element *scrollbar_background      = ui_element(ui, "__scrollview_background"_s, UI_Background | UI_Snap_Draggable_Children_On_Click | UI_Extrude_Children);
        scrollbar_background->semantic_width  = { UI_SEMANTIC_SIZE_Pixels, 7, 1 };
        scrollbar_background->semantic_height = { UI_SEMANTIC_SIZE_Percentage_Of_Parent, 1, 0 };
        scrollbar_background->default_color   = ui->theme.border_color;
        scrollbar_background->hovered_color   = ui->theme.border_color;
        scrollbar_background->rounding        = ui->theme.rounding;
        ui_push_parent(ui, UI_DIRECTION_Vertical);

        f32 knob_size = roundf(min(area->screen_size.y / area->view_scroll_screen_size.y, 1.0f) * (scrollbar_background->screen_size.y));
        UI_Element *scrollbar_knob      = ui_element(ui, "__scrollview_knob"_s, UI_Background | UI_Clickable | UI_Floating | UI_Draggable);
        scrollbar_knob->semantic_width  = { UI_SEMANTIC_SIZE_Pixels, 7, 1 };
        scrollbar_knob->semantic_height = { UI_SEMANTIC_SIZE_Pixels, knob_size, 1 };
        scrollbar_knob->default_color   = ui->theme.accent_color;
        scrollbar_knob->hovered_color   = ui->theme.hovered_color;
        scrollbar_knob->rounding        = ui->theme.rounding;

        if(scrollbar_knob->signals & UI_SIGNAL_Hovered || scrollbar_knob->signals & UI_SIGNAL_Dragged) {
            scrollbar_background->semantic_width = { UI_SEMANTIC_SIZE_Pixels, 1, 1 };
        } else {
            scrollbar_knob->float_vector.y = area->view_scroll_screen_offset.y / (area->view_scroll_screen_size.y - area->screen_size.y); // If the scroll screen size has not yet been calculated, this would cause an NaN...
        }

        ui_pop_parent(ui);
        
        ui_spacer(ui, UI_SEMANTIC_SIZE_Pixels, 0, 0, UI_SEMANTIC_SIZE_Pixels, 5, 1);

        ui_pop_parent(ui);
        
        area->view_scroll_screen_offset.y = scrollbar_knob->float_vector.y * (area->view_scroll_screen_size.y - area->screen_size.y);
    } else if(area->layout_direction == UI_DIRECTION_Horizontal && area->view_scroll_screen_size.x > area->screen_size.x) {
        UI_Element *scrollbar_container      = ui_element(ui, "__scrollview_container"_s, UI_Background | UI_Center_Children);
        scrollbar_container->semantic_width  = { UI_SEMANTIC_SIZE_Percentage_Of_Parent, 1, 0 };
        scrollbar_container->semantic_height = { UI_SEMANTIC_SIZE_Pixels, 15, 1 };
        scrollbar_container->default_color   = { ui->theme.window_background_color.r, ui->theme.window_background_color.g, ui->theme.window_background_color.b, 255 };        
        ui_push_parent(ui, UI_DIRECTION_Horizontal); // Center the children vertically
        
        ui_spacer(ui, UI_SEMANTIC_SIZE_Pixels, 5, 1, UI_SEMANTIC_SIZE_Pixels, 0, 0);
        
        UI_Element *scrollbar_background      = ui_element(ui, "__scrollview_background"_s, UI_Background | UI_Snap_Draggable_Children_On_Click | UI_Extrude_Children);
        scrollbar_background->semantic_width  = { UI_SEMANTIC_SIZE_Percentage_Of_Parent, 1, 0 };
        scrollbar_background->semantic_height = { UI_SEMANTIC_SIZE_Pixels, 7, 1 };
        scrollbar_background->default_color   = ui->theme.border_color;
        scrollbar_background->hovered_color   = ui->theme.border_color;
        scrollbar_background->rounding        = ui->theme.rounding;
        ui_push_parent(ui, UI_DIRECTION_Horizontal);

        f32 knob_size = roundf(min(area->screen_size.x / area->view_scroll_screen_size.x, 1.0f) * (scrollbar_background->screen_size.x));
        UI_Element *scrollbar_knob      = ui_element(ui, "__scrollview_knob"_s, UI_Background | UI_Clickable | UI_Floating | UI_Draggable);
        scrollbar_knob->semantic_width  = { UI_SEMANTIC_SIZE_Pixels, knob_size, 1 };
        scrollbar_knob->semantic_height = { UI_SEMANTIC_SIZE_Pixels, 7, 1 };
        scrollbar_knob->default_color   = ui->theme.accent_color;
        scrollbar_knob->hovered_color   = ui->theme.hovered_color;
        scrollbar_knob->rounding        = ui->theme.rounding;

        if(scrollbar_knob->signals & UI_SIGNAL_Hovered || scrollbar_knob->signals & UI_SIGNAL_Dragged) {
            scrollbar_background->semantic_height = { UI_SEMANTIC_SIZE_Pixels, 1, 1 };
        } else {
            scrollbar_knob->float_vector.x = area->view_scroll_screen_offset.x / (area->view_scroll_screen_size.x - area->screen_size.x); // If the scroll screen size has not yet been calculated, this would cause an NaN...
        }

        ui_pop_parent(ui);
        
        ui_spacer(ui, UI_SEMANTIC_SIZE_Pixels, 5, 1, UI_SEMANTIC_SIZE_Pixels, 0, 0);

        ui_pop_parent(ui);
        
        area->view_scroll_screen_offset.x = scrollbar_knob->float_vector.x * (area->view_scroll_screen_size.x - area->screen_size.x);        
    }
    
    ui_pop_parent(ui); // Pop the complete container
}