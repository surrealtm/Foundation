#include "foundation.h"

//
// Timing Macros.
//

#define tmBegin()    _tmBegin()
#define tmFunction() _tmEnter(__FUNCTION__, __FILE__ ":" STRINGIFY(__LINE__));  defer {_tmExit(); }
#define tmFinish()   _tmFinish()
#define tmZone(name) _tmEnter(name, __FILE__ ":" STRINGIFY(__LINE__));  defer {_tmExit(); }


//
// Flags to modify the default output behaviour into the console.
//

enum Timing_Output_Mode {
    TIMING_OUTPUT_None     = 0x0,
    TIMING_OUTPUT_Timeline = 0x1,
    TIMING_OUTPUT_Summary  = 0x2,
};

BITWISE(Timing_Output_Mode);

enum Timing_Output_Sorting {
    TIMING_OUTPUT_Unsorted          = 0x0,
    TIMING_OUTPUT_Sort_By_Count     = 0x1,
    TIMING_OUTPUT_Sort_By_Inclusive = 0x2,
    TIMING_OUTPUT_Sort_By_Exclusive = 0x3,
};


//
// Structs to export timing data into other applications.
//

struct Timing_Timeline_Entry {
    char const *name;
    f64 relative_start, relative_end; // Relative to the entire time span, meaning in the interval [0,1]
    f64 time_in_seconds;
    s64 depth; // The vertical depth of the entry, representing the call stack depth
};

struct Timing_Summary_Entry {
    char const *name;
    f64 inclusive_time_in_seconds;
    f64 exclusive_time_in_seconds;
    s64 count;
};

struct Timing_Data {
    Timing_Timeline_Entry *timeline;
    s64 timeline_count;
    Timing_Summary_Entry *summary;
    s64 summary_count;

    f64 total_time_in_seconds; // To make it easier to port
};


//
// Timing API. You should probably use the macros provided above.
//

void _tmBegin();
void _tmReset();
void _tmDestroy();
void _tmEnter(char const *procedure_name, char const *source_string);
void _tmExit();
void _tmFinish();

void tmPrintToConsole(Timing_Output_Mode mode = TIMING_OUTPUT_None, Timing_Output_Sorting sorting = TIMING_OUTPUT_Sort_By_Inclusive);
Timing_Data tmData(Timing_Output_Sorting sorting = TIMING_OUTPUT_Sort_By_Inclusive);
void tmFreeData(Timing_Data *data);
