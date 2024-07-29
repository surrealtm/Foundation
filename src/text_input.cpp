#include "text_input.h"
#include "window.h"
#include "os_specific.h"
#include "font.h"
#include "memutils.h"

enum Word_Mode {
    WORD_Alpha_Numeric_Characters,
    WORD_Special_Characters,
};

static
b8 is_empty_character(u8 character) {
    return character == ' ' || character == '\t';
}

static
b8 is_digit_character(u8 character) {
    return (character >= '0' && character <= '9');
}

static
b8 is_alpha_numeric_character(u8 character) {
    return (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9') ||
        (character == '_' || character == '.' || character == '-' || character == '+');
}

static
b8 is_special_character(u8 character) {
    return !is_empty_character(character) && !is_alpha_numeric_character(character);
}

static
b8 is_word_character(Word_Mode word_mode, u8 character) {
    if(word_mode == WORD_Alpha_Numeric_Characters)
        return is_alpha_numeric_character(character);
    else
        return is_special_character(character);
}

static
Word_Mode get_word_mode_for_character(u8 character) {
    return is_alpha_numeric_character(character) ? WORD_Alpha_Numeric_Characters : WORD_Special_Characters;
}

static
void insert_text(Text_Input *input, string text) {
    //
    // Check for remaining available space.
    //
    text.count = min(text.count, TEXT_INPUT_BUFFER_SIZE - input->count);
    if(text.count == 0) return;

    //
    // Check the text against the input mode.
    //
    if(input->mode == TEXT_INPUT_Integer) {
        b8 found_invalid = false;

        for(s64 i = 0; i < text.count; ++i) {
            char c = text[i];
            b8 valid = is_digit_character(c) || (input->cursor == 0 && i == 0 && (c == '+' || c == '-'));
            if(!valid) found_invalid = true;
        }
        
        if(found_invalid) return;
    } else if(input->mode == TEXT_INPUT_Floating_Point) {
        b8 found_invalid = false;
        b8 found_dot = search_string(text_input_string_view(input), '.') != -1;
        
        for(s64 i = 0; i < text.count; ++i) {
            char c = text[i];
            b8 valid = is_digit_character(c) || (input->cursor == 0 && i == 0 && (c == '+' || c == '-')) || (!found_dot && c == '.');
            if(c == '.') found_dot = true;
            if(!valid) found_invalid = true;
        }
        
        if(found_invalid) return;
    }
    
    //
    // Actually insert the text.
    //
    memmove(&input->buffer[input->cursor + text.count], &input->buffer[input->cursor], input->count - input->cursor);
    memcpy(&input->buffer[input->cursor], text.data, text.count);
    input->count  += text.count;
    input->cursor += text.count;
}

static
void erase_text(Text_Input *input, s64 first_to_remove, s64 last_to_remove) {
    assert(last_to_remove >= first_to_remove);
    s64 count = (last_to_remove - first_to_remove) + 1;
    assert(count <= input->count);
    memmove(&input->buffer[first_to_remove], &input->buffer[last_to_remove + 1], input->count - (last_to_remove + 1));
    input->count -= count;
    input->cursor = first_to_remove;
}

static
s64 get_start_of_current_word_towards_the_left(Text_Input *input) {
    s64 position = input->cursor;

    //
    // Skip all leading characters until a word has been found.
    //
    while(position > 0 && is_empty_character(input->buffer[position - 1])) --position;

    //
    // Skip this word until the next empty character.
    //
    if(position > 0) {
        Word_Mode word_mode = get_word_mode_for_character(input->buffer[position - 1]);
        while(position > 0 && is_word_character(word_mode, input->buffer[position - 1])) --position;
    }

    return position;
}

static
s64 get_end_of_next_word_towards_the_left(Text_Input *input) {
    s64 position = get_start_of_current_word_towards_the_left(input);

    //
    // Skip the empty characters until the start of the next word.
    //
    while(position > 0 && is_empty_character(input->buffer[position - 1])) --position;

    return position;
}

static
s64 get_start_of_next_word_towards_the_right(Text_Input *input) {
    s64 position = input->cursor;

    //
    // Skip all leading characters until a word has been found.
    //
    while(position < input->count && is_empty_character(input->buffer[position])) ++position;

    //
    // Skip this word until the next empty character.
    //
    Word_Mode word_mode = get_word_mode_for_character(input->buffer[position]);
    while(position < input->count && is_word_character(word_mode, input->buffer[position])) ++position;

    //
    // Skip the empty characters until the start of the next word.
    //
    while(position < input->count && is_empty_character(input->buffer[position])) ++position;

    return position;
}

static
void clear_selection(Text_Input *input) {
    input->selection_active = false;
    input->selection_pivot = 0;
}

static
void erase_selection(Text_Input *input) {
    if(input->cursor < input->selection_pivot) {
        erase_text(input, input->cursor, input->selection_pivot - 1);
    } else if(input->cursor > input->selection_pivot) {
        erase_text(input, input->selection_pivot, input->cursor - 1);
    }

    clear_selection(input);
}

static
void maybe_start_or_end_selection(Text_Input *input, Text_Input_Event *event) {
    if(event->shift_held && !input->selection_active) {
        input->selection_active = true;
        input->selection_pivot  = input->cursor;
    } else if(!event->shift_held) {
        clear_selection(input);
    }
}


void create_text_input(Text_Input *input, Text_Input_Mode mode) {
    input->mode              = mode;
    input->active_this_frame = false;
    clear_text_input(input);
}

b8 update_text_input(Text_Input *input, Window *window, Font *font) {
    b8 anything_changed = false;

    input->entered_this_frame = false;
    input->tabbed_this_frame  = false;
    
    //
    // Handle the text input events from the window.
    //
    if(input->active_this_frame) {
        for(s64 i = 0; i < window->text_input_event_count; ++i) {
            Text_Input_Event *event = &window->text_input_events[i];

            if(event->type == TEXT_INPUT_EVENT_Control) {               
                switch(event->control) {
                case KEY_Enter: input->entered_this_frame = true; break;
                case KEY_Tab:   input->tabbed_this_frame = true; break;
                    
                case KEY_Backspace:
                    if(input->selection_active) {
                        erase_selection(input);
                        anything_changed = true;
                    } else if(event->control_held && input->cursor > 0) {
                        erase_text(input, get_start_of_current_word_towards_the_left(input), input->cursor - 1);
                        anything_changed = true;
                    } else if(input->cursor > 0) {
                        erase_text(input, input->cursor - 1, input->cursor - 1); 
                        anything_changed = true;
                    }
                    break;
                
                case KEY_Delete:
                    if(input->selection_active) {
                        erase_selection(input);
                        anything_changed = true;
                    } else if(event->control_held && input->cursor < input->count) {
                        erase_text(input, input->cursor, get_start_of_next_word_towards_the_right(input) - 1);
                        anything_changed = true;
                    } else if(input->cursor < input->count) {
                        erase_text(input, input->cursor, input->cursor);
                        anything_changed = true;
                    }
                    break;

                case KEY_Arrow_Left:
                    maybe_start_or_end_selection(input, event);    

                    if(event->control_held) {
                        input->cursor = get_start_of_current_word_towards_the_left(input);
                        anything_changed = true;
                    } else if(input->cursor > 0) {
                        --input->cursor;
                        anything_changed = true;
                    }
                    break;

                case KEY_Arrow_Right:
                    maybe_start_or_end_selection(input, event);    
                    
                    if(event->control_held) {
                        input->cursor = get_start_of_next_word_towards_the_right(input);
                        anything_changed = true;
                    } else if(input->cursor < input->count) {
                        ++input->cursor;
                        anything_changed = true;
                    }
                    break;

                case KEY_Home:
                    maybe_start_or_end_selection(input, event);    
                    input->cursor = 0;
                    break;

                case KEY_End:
                    maybe_start_or_end_selection(input, event);    
                    input->cursor = input->count;
                    anything_changed = true;
                    break;
                    
                case KEY_A:
                    assert(event->control_held);
                    input->selection_active = true;
                    input->selection_pivot = 0;
                    input->cursor = input->count;
                    anything_changed = true;
                    break;

                case KEY_C:
                    if(input->selection_active) {
                        set_clipboard_data(window, text_input_selected_string_view(input));
                    } else {
                        set_clipboard_data(window, text_input_string_view(input));
                    }
                    anything_changed = true;
                    break;

                case KEY_X:
                    if(input->selection_active) {
                        set_clipboard_data(window, text_input_selected_string_view(input));
                        erase_selection(input);
                    } else {
                        set_clipboard_data(window, text_input_string_view(input));
                        clear_text_input(input);
                    }
                    anything_changed = true;
                    break;

                case KEY_V: {
                    if(input->selection_active) erase_selection(input);
                    string string = get_clipboard_data(window, Default_Allocator);
                    if(input->selection_active) erase_selection(input);
                    clear_selection(input);
                    insert_text(input, string);
                    deallocate_clipboard_data(Default_Allocator, &string);
                    anything_changed = true;
                } break;

                default: break; // So that clang doesn't complain
                }
            } else if(event->type == TEXT_INPUT_EVENT_Character) {
                if(input->selection_active) erase_selection(input);
                clear_selection(input);

                string _string;
                _string.count = 1;
                _string.data = (u8 *) &event->utf32;
                insert_text(input, _string);
                anything_changed = true;
            }
            
            input->time_of_last_input = os_get_hardware_time();
        }
    }
    
    //
    // Update the rendering data
    //
    if(font) {
        f32 interpolation             = min(window->frame_time * 20.f, 1.f);
        string string_to_cursor       = string_view(input->buffer, input->cursor);
        input->target_cursor_x        = (f32) get_string_width_in_pixels(font, string_to_cursor);
        input->interpolated_cursor_x += (input->target_cursor_x - input->interpolated_cursor_x) * interpolation;
        input->cursor_x               = roundf(input->interpolated_cursor_x);

        if(input->selection_active) {
            if(input->cursor <= input->selection_pivot) {
                input->selection_start_x = input->cursor_x;
                input->selection_end_x   = (f32) get_string_width_in_pixels(font, string_view(input->buffer, input->selection_pivot));
            } else {
                input->selection_start_x = (f32) get_string_width_in_pixels(font, string_view(input->buffer, input->selection_pivot));
                input->selection_end_x   = input->cursor_x;
            }
        } else {
            input->selection_start_x = 0.f;
            input->selection_end_x   = 0.f;
        }
        
        f32 seconds_since_last_input = (f32) os_convert_hardware_time(os_get_hardware_time() - input->time_of_last_input, Seconds);
        if(seconds_since_last_input < 5.f) {
            input->cursor_alpha_zero_to_one = cosf(seconds_since_last_input * 3.14159f * 2.f) * 0.5f + 0.5f;
        } else {
            input->cursor_alpha_zero_to_one = 1.f;
        }
    }

    return anything_changed;
}

void clear_text_input(Text_Input *input) {
    input->count                    = 0;
    input->cursor                   = 0;
    input->selection_pivot          = 0;
    input->selection_active         = false;
    input->entered_this_frame       = false;
    input->target_cursor_x          = 0.f;
    input->interpolated_cursor_x    = 0.f;
    input->cursor_x                 = 0.f;
    input->cursor_alpha_zero_to_one = 1.f;
    input->selection_start_x        = 0.f;
    input->selection_end_x          = 0.f;
    input->time_of_last_input       = os_get_hardware_time();
}

void toggle_text_input_activeness(Text_Input *input, b8 active) {
    input->active_this_frame = active;
    input->time_of_last_input = os_get_hardware_time();
}

void set_text_input_string(Text_Input *input, string string) {
    // Don't clear out the rendering data here, that seems to look better.
    input->count              = 0;
    input->cursor             = 0;
    input->time_of_last_input = os_get_hardware_time();
    clear_selection(input);
    insert_text(input, string);
}

void insert_text_input_string(Text_Input *input, string string) {
    insert_text(input, string);
}

void remove_text_input_range(Text_Input *input, s64 first_to_remove, s64 last_to_remove) {
    erase_text(input, first_to_remove, last_to_remove);
}

string text_input_string_view(Text_Input *input) {
    return string_view(input->buffer, input->count);
}

string text_input_selected_string_view(Text_Input *input) {
    if(input->cursor <= input->selection_pivot) {
        return string_view(&input->buffer[input->cursor], input->selection_pivot - input->cursor);
    } else {
        return string_view(&input->buffer[input->selection_pivot], input->cursor - input->selection_pivot);
    }
}

string text_input_string_view_until_cursor(Text_Input *input) {
    return string_view(input->buffer, input->cursor);
}

string text_input_string_view_until_selection(Text_Input *input) {
    return string_view(input->buffer, min(input->cursor, input->selection_pivot));
}

string text_input_string_view_after_selection(Text_Input *input) {
    s64 start = max(input->cursor, input->selection_pivot);
    return string_view(&input->buffer[start], input->count - start);
}