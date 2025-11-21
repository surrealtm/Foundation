#pragma once

#define TM_DEFAULT_COLOR (0xffffffff)

#if FOUNDATION_TELEMETRY_BUILTIN

/* ----------------------------------------- Builtin Profiler ---------------------------------------- */

# if FOUNDATION_TELEMETRY_TRACY
#  error "Please specify either FOUNDATION_TELMETRY_TRACY or FOUNDATION_TELEMTRY_BUILTIN, not both"
# endif

# define tmBegin()                    _tmReset()
# define tmFunction(color)            _tmEnter(__FUNCTION__, __FILE__ ":" STRINGIFY(__LINE__), color); defer { _tmExit(); }
# define tmEnter(name, color)         _tmEnter(name, __FILE__ ":" STRINGIFY(__LINE__), color)
# define tmZone(name, color)          _tmEnter(name, __FILE__ ":" STRINGIFY(__LINE__), color); defer { _tmExit(); }
# define tmParameter(parameter, copy) _tmParameter(parameter, copy)
# define tmExit()                     _tmExit()
# define tmFinish()                   _tmFinish()
# define tmDestroy()                  _tmDestroy()

#include "foundation.h"
#include "string_type.h"

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

struct Timing_Timeline_Entry {
    string name;
    s64 start_in_nanoseconds, end_in_nanoseconds;
    s64 depth; // The vertical depth of the entry, representing the call stack depth
    u8 r, g, b;
};

struct Timing_Summary_Entry {
    string name;
    s64 inclusive_time_in_nanoseconds;
    s64 exclusive_time_in_nanoseconds;
    s64 count;
};

struct Timing_Data {
    Timing_Timeline_Entry **timelines;
    s64 *timelines_entry_count;
    s64 timelines_count;
    
    Timing_Summary_Entry *summary;
    s64 summary_count;

    s64 total_time_in_nanoseconds;
    s64 total_overhead_time_in_nanoseconds;
    s64 total_overhead_space_in_bytes;
};

void _tmReset();
void _tmDestroy();
void _tmEnter(const char *procedure_name, const char *source_string, u32 color);
void _tmParameter(string parameter, b8 copy_to_internal);
void _tmExit();
void _tmFinish();

void tmPrintToConsole(Timing_Output_Mode mode = TIMING_OUTPUT_None, Timing_Output_Sorting sorting = TIMING_OUTPUT_Sort_By_Inclusive);
Timing_Data tmData(Timing_Output_Sorting sorting = TIMING_OUTPUT_Sort_By_Inclusive);
void tmFreeData(Timing_Data *data);

#elif FOUNDATION_TELEMETRY_TRACY

/* ------------------------------------------ Tracy Profiler ----------------------------------------- */

# if FOUNDATION_TELEMETRY_BUILTIN
#  error "Please specify either FOUNDATION_TELMETRY_TRACY or FOUNDATION_TELEMTRY_BUILTIN, not both"
# endif

# define TRACY_ENABLE
# define TRACY_NO_EXIT

# include "tracy/tracy/Tracy.hpp"
# include "tracy/tracy/TracyC.h"

# define tmBegin()                    FrameMarkStart()
# define tmFunction(color)            ZoneScopedC(color)
# define tmZone(name, color)          ZoneScopedNC(name, color)
# define tmEnter(name, color)         TracyCZoneC(__tracy__zone, color, true)
# define tmParameter(parameter, copy) ZoneText((const char *) parameter.data, parameter.count)
# define tmExit()                     TracyCZoneEnd(__tracy__zone)
# define tmFinish()                   FrameMarkEnd()
# define tmDestroy()


#else

/* ------------------------------------------- No Profiler ------------------------------------------- */

# define tmBegin()
# define tmFunction(color)
# define tmZone(name, color)
# define tmEnter(name, color)
# define tmParameter(parameter, copy)
# define tmExit()
# define tmFinish()
# define tmDestroy()

#endif
