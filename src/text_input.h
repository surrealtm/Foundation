#pragma once

#include "foundation.h"
#include "string_type.h"

#define TEXT_INPUT_BUFFER_SIZE 512

struct Window;
struct Font;

struct Text_Input {
    //
    // Internal state.
    //
    b8 active;
    u8 buffer[TEXT_INPUT_BUFFER_SIZE];
    s64 count;
    s64 cursor;
    s64 selection_pivot; // The cursor position in which the selection originally started. The selection is currently between this and the cursor position. Note that this isn't sorted, meaning the cursor can be bigger or smaller than this pivot.

    //
    // Immediate mode exposed data.
    //
    b8 entered_this_frame;
    b8 selection_active;    

    //
    // Rendering data.
    //
    f32 target_cursor_x;
    f32 interpolated_cursor_x;
    f32 cursor_x; // This is the rounded interpolated_cursor_x, use this for rendering!
    f32 cursor_alpha_zero_to_one;
    f32 selection_start_x;
    f32 selection_end_x;
    s64 time_of_last_input; // Hardware_Time
};

void create_text_input(Text_Input *input);
void update_text_input(Text_Input *input, Window *window, Font *font); // The font is used for rendering data (e.g. the cursor position requires knowledge of the string width in pixels...)
void clear_text_input(Text_Input *input);
void toggle_text_input_activeness(Text_Input *input, b8 active);
void set_text_input_string(Text_Input *input, string string);
string text_input_string_view(Text_Input *input);
string text_input_selected_string_view(Text_Input *input);
