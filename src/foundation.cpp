#include "foundation.h"
#include "os_specific.h"

#include <stdarg.h> // For va_list
#include <string.h> // For strlen

b8 foundation_do_assertion_fail(const char *assertion_text, const char *format, ...) {
    char message[1024];
    u32 message_length = 0;
    s64 header_length  = 0;
    
    if(*format != 0) {    
        va_list args;
        va_start(args, format);
#if FOUNDATION_WIN32
        message_length = (u32) vsprintf_s(message, sizeof(message), format, args);
#else
        message_length = (u32) vsprintf(message, format, args);
#endif
        va_end(args);
        
        header_length = message_length + strlen("*  Assertion Failed: ") + 3;
    } else {
        header_length = strlen("*  Assertion Failed: ") + 2;
    }
    
    for(s64 i = 0; i < header_length; ++i) printf("*");
    printf("\n");
    
    if(message_length) {
        printf("*  Assertion Failed: %.*s  *\n", message_length, message);
    } else {
        printf("*  Assertion Failed:  *\n"); // Only do two spaces instead of three on message_length == 0
    }
    
    for(s64 i = 0; i < header_length; ++i) printf("*");
    printf("\n\n");
    
    printf("Expression: '%s'\n\n", assertion_text);
    
#if FOUNDATION_DEVELOPER
    Stack_Trace trace = os_get_stack_trace(Default_Allocator, 1);
    
    printf("Stack Trace:\n");
    
    for(s64 i = 0; i < trace.frame_count; ++i) {
        auto &frame = trace.frames[i];
        
        if(frame.source_file.count) {
            printf("  %.*s // %.*s:%u\n", (u32) frame.description.count, frame.description.data, (u32) frame.source_file.count, frame.source_file.data, (u32) frame.source_line);
        } else {
            printf("  %.*s\n", (u32) trace.frames[i].description.count, trace.frames[i].description.data);
        }
    }
    
    printf("\n\n");
    
    os_free_stack_trace(Default_Allocator, &trace);
    
    if(message_length) {
        os_message_box(string_view((u8 *) message, message_length));
    } else {
        os_message_box(cstring_view(assertion_text));
    }
    
    os_debug_break();
#else
    os_terminate_process(0);
#endif
    
    return true;
}

const char *time_unit_suffix(Time_Unit unit) {
    const char *string;
    
    switch(unit) {
        case Nanoseconds:  string = "ns"; break;
        case Microseconds: string = "us"; break;
        case Milliseconds: string = "ms"; break;
        case Seconds:      string = "s";  break;
        case Minutes:      string = "m";  break;
        default: string = "<>"; break;
    }
    
    return string;
}
