#pragma once

#include "strings.h"

enum Error_Code {
    Success = 0,
    
    /* Default Errors */
    ERROR_Custom_Error_Message,
    ERROR_File_Not_Found,
    ERROR_Invalid_Version,

    /* D3D11 Layer Errors */
    ERROR_D3D11_Invalid_Channel_Count,
    ERROR_D3D11_Invalid_Data_Type,
    ERROR_D3D11_Erroneous_Shader,
    ERROR_D3D11_Invalid_Shader_Inputs,
    
    ERROR_COUNT,
};

string error_string(Error_Code code);
void set_custom_error_message(string message);
void set_custom_error_message(const char *message);
string get_custom_error_message();
