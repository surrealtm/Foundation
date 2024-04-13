#include "foundation.h"

#define tmFunction() _tmEnter(__FUNCTION__, __FILE__ ":" STRINGIFY(__LINE__));  defer {_tmExit(); }
#define tmFinishFrame(mode, ...) _tmFinishFrame((Timing_Output_Mode) (mode), __VA_ARGS__)

enum Timing_Output_Mode {
    TIMING_OUTPUT_None       = 0x0,
    TIMING_OUTPUT_Timeline   = 0x1,
    TIMING_OUTPUT_Statistics = 0x2,
};

enum Timing_Output_Sorting {
    TIMING_OUTPUT_Unsorted          = 0x0,
    TIMING_OUTPUT_Sort_By_Count     = 0x1,
    TIMING_OUTPUT_Sort_By_Inclusive = 0x2,
    TIMING_OUTPUT_Sort_By_Exclusive = 0x3,
};

void _tmReset();
void _tmDestroy();
void _tmEnter(char const *procedure_name, char const *source_string);
void _tmExit();
void _tmFinishFrame(Timing_Output_Mode mode, Timing_Output_Sorting sorting = TIMING_OUTPUT_Sort_By_Inclusive);
