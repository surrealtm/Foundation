#pragma once

#include "string_type.h"

enum Error_Code {
    Success = 0,
    
    /* Default Errors */
    ERROR_Custom_Error_Message,
    ERROR_File_Not_Found,
    ERROR_Invalid_Version,

    /* D3D11 Layer Errors */
    ERROR_D3D11_Invalid_Channel_Count,
    ERROR_D3D11_Invalid_Data_Type,
    ERROR_D3D11_Invalid_Dimensions,
    ERROR_D3D11_Erroneous_Shader,
    ERROR_D3D11_Invalid_Shader_Inputs,

    /* Socket Errors */
    ERROR_SOCKET_Address_In_Use,
    ERROR_SOCKET_Address_Not_Available,
    ERROR_SOCKET_Network_Down,
    ERROR_SOCKET_Network_Unreachable,
    ERROR_SOCKET_Network_Reset,
    ERROR_SOCKET_Connection_Aborted,
    ERROR_SOCKET_Connection_Reset,
    ERROR_SOCKET_Connection_Refused,
    ERROR_SOCKET_Host_Down,
    ERROR_SOCKET_Host_Unreachable,
    ERROR_SOCKET_Host_Not_Found,
    
    ERROR_COUNT,
};

string error_string(Error_Code code);
void set_custom_error_message(string message);
void set_custom_error_message(const char *message);
void set_custom_error_message(const char *message, s64 count);
string get_custom_error_message();
