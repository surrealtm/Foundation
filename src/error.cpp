#include "error.h"
#include <stdlib.h>
#include <stdarg.h>

string error_string_table[] = {
    "Success"_s,

    /* Default Errors */
    "<Unspecified Error>"_s,
    "The file does not exist"_s,
    "The file is too small"_s,
    "Invalid version"_s,

    /* D3D11 Layer Errors */
    "Invalid number of channels"_s,
    "Invalid data type"_s,
    "Invalid dimensions"_s,
    "The shader contained some errors"_s,
    "The shader input specification did not match up"_s,

    /* Socket Errors */
    "Address In Use"_s,
    "Address Not Available"_s,
    "Network Down"_s,
    "Network Unreachable"_s,
    "Network Reset"_s,
    "Connection Aborted"_s,
    "Connection Reset"_s,
    "Connection Refused"_s,
    "Host Down"_s,
    "Host Unreachable"_s,
    "Host Not Found"_s,

    /* Audio Errors */
    "Invalid Sample Rate"_s,
    "Invalid Channel Count"_s,
    "Invalid Bits Per Sample"_s,
    "Invalid Wav File"_s,
};

static_assert(ARRAY_COUNT(error_string_table) == ERROR_COUNT, "The error_string_table has the wrong size.");

static char internal_custom_error_message_buffer[1024];
static s64 internal_custom_error_message_count;

string error_string(Error_Code code) {
    if(code == ERROR_Custom_Error_Message) return get_custom_error_message();
    if(code < 0 || code >= ERROR_COUNT) return error_string_table[ERROR_Custom_Error_Message];
    return error_string_table[code];
}

Error_Code set_custom_error_message(string message) {
    internal_custom_error_message_count = min((s64) ARRAY_COUNT(internal_custom_error_message_buffer), message.count);
    copy_cstring(internal_custom_error_message_buffer, ARRAY_COUNT(internal_custom_error_message_buffer), (const char *) message.data, internal_custom_error_message_count);
    return ERROR_Custom_Error_Message;
}

Error_Code set_custom_error_message(const char *message, s64 count) {
    internal_custom_error_message_count = min((s64) ARRAY_COUNT(internal_custom_error_message_buffer), count);
    copy_cstring(internal_custom_error_message_buffer, ARRAY_COUNT(internal_custom_error_message_buffer), message, internal_custom_error_message_count);
    return ERROR_Custom_Error_Message;
}

Error_Code set_custom_error_message(const char *format, ...) {
    va_list args;
    va_start(args, format);
    internal_custom_error_message_count = vsnprintf(internal_custom_error_message_buffer, ARRAY_COUNT(internal_custom_error_message_buffer), format, args);
    va_end(args);
    return ERROR_Custom_Error_Message;
}

string get_custom_error_message() {
    return string_view((u8 *) internal_custom_error_message_buffer, internal_custom_error_message_count);
}
