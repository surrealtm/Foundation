#include "timing.h"
#include "memutils.h"
#include "os_specific.h"



/* ------------------------------------------ Internal Implementation ------------------------------------------ */

#define __TIMING_INDENT_PER_PARENT 2
#define __TIMING_INITIAL_INDENT    0

#define __TIMING_PRINT_PROC_OFFSET 0
#define __TIMING_PRINT_INCL_OFFSET 80
#define __TIMING_PRINT_EXCL_OFFSET 100
#define __TIMING_PRINT_COUN_OFFSET 120

#define __TIMING_MAX_COLORS 32

#define _tmPrintRepeated(char, count) for(s64 i = 0; i < count; ++i) printf("%c", char);

struct _tm_Timeline_Entry {
    s64 parent_index = MAX_S64;
    s64 next_index = MAX_S64;
    s64 first_child_index = MAX_S64;
    s64 last_child_index = MAX_S64;

    char const *procedure_name;
    char const *source_string;
    Hardware_Time hwtime_start;
    Hardware_Time hwtime_end;
    
    u8 color_index;
};

struct _tm_Summary_Entry {
    _tm_Summary_Entry *next = null;
    
    s64 hash;
    char const *procedure_name;
    char const *source_string;
    Hardware_Time total_inclusive_hwtime;
    Hardware_Time total_exclusive_hwtime;
    s64 count;
};

struct _tm_Color {
    u8 r = 100, g = 100, b = 200;
};

struct _tm_State {
    Resizable_Array<_tm_Timeline_Entry> timeline;
    s64 head_index = MAX_S64;
    s64 root_index = MAX_S64; // The current root function (the function without a parent in the profiling mechanism).

    s64 total_hwtime_start;
    s64 total_hwtime_end;
    
    _tm_Summary_Entry *summary_table = null;
    s64 summary_table_size = 0;

    _tm_Color colors[__TIMING_MAX_COLORS];

    Resizable_Array<_tm_Summary_Entry*> sorted_summary;
};

static _tm_State __timing;



static
int _tmPrintPaddingTo(s64 target, s64 current) {
    int total = 0;

    if(target >= current) {
        total = printf("%-*s", (s32) (target - current), "");
    } else {
        for(s64 i = 0; i < (current - target) + 2; ++i) printf("\b");
        printf("  ");
        total = (s32) (target - current);
    }

    return total;
}

static
void _tmInternalDestroySummaryTable(b8 reallocate) {
    //
    // If the summary table is currently empty, then we don't need to clean anything up.
    //
    if(__timing.summary_table_size == 0) {
        if(reallocate) {
            __timing.summary_table_size = (s64) ceil((f64) __timing.timeline.count / (f64) 3);
            __timing.summary_table = (_tm_Summary_Entry *) Default_Allocator->allocate(__timing.summary_table_size * sizeof(_tm_Summary_Entry));
            memset(__timing.summary_table, 0, __timing.summary_table_size * sizeof(_tm_Summary_Entry));
        }

        return;
    }

    //
    // Destroy the single allocated entries inside each bucket.
    //
    for(s64 i = 0; i < __timing.summary_table_size; ++i) {
        auto bucket = __timing.summary_table[i].next; // The first entry is not allocated.
        
        while(bucket) {
            auto next = bucket->next;
            Default_Allocator->deallocate(bucket);
            bucket = next;
        }
    }

    //
    // Destroy (and maybe reallocate) the actual summary table.
    //
    if(reallocate) {        
        s64 size_estimation = (s64) ceil((f64) __timing.timeline.count / (f64) 3);
        if(fabs(1 - (f64) size_estimation / (f64) __timing.summary_table_size) > 0.5) {
            Default_Allocator->deallocate(__timing.summary_table);
            __timing.summary_table_size = size_estimation;
            __timing.summary_table = (_tm_Summary_Entry *) Default_Allocator->allocate(__timing.summary_table_size * sizeof(_tm_Summary_Entry));
        }

        memset(__timing.summary_table, 0, __timing.summary_table_size * sizeof(_tm_Summary_Entry));
    } else {
        Default_Allocator->deallocate(__timing.summary_table);
        __timing.summary_table_size = 0;
    }
}

static
Hardware_Time _tmInternalCalculateHardwareTimeOfChildren(_tm_Timeline_Entry *entry) {
    if(entry->first_child_index == MAX_S64) return 0;
    
    Hardware_Time result = 0;

    auto *child = &__timing.timeline[entry->first_child_index];
    while(true) {
        result += child->hwtime_end - child->hwtime_start;
        if(child->next_index == MAX_S64) break;
        child = &__timing.timeline[child->next_index];
    }

    return result;
}

static
s64 _tmInternalCalculateStackDepth(_tm_Timeline_Entry *entry) {
    s64 depth = 0;

    while(entry->parent_index != MAX_S64) {
        entry = &__timing.timeline[entry->parent_index];
        ++depth;
    }
    
    return depth;
}

static
Time_Unit _tmInternalGetBestTimeUnit(Hardware_Time hwtime) {
    Time_Unit unit = Nanoseconds;
    while(unit < Minutes && os_convert_hardware_time(hwtime, unit) >= 1000) unit = (Time_Unit) (unit + 1);
    return unit;
}

static
void _tmInternalPrintTimelineEntry(_tm_Timeline_Entry *entry, u32 indentation) {
    while(true) {
        Hardware_Time inclusive_hwtime = entry->hwtime_end - entry->hwtime_start;
        Hardware_Time exclusive_hwtime = inclusive_hwtime - _tmInternalCalculateHardwareTimeOfChildren(entry);

        Time_Unit inclusive_unit = _tmInternalGetBestTimeUnit(inclusive_hwtime);
        Time_Unit exclusive_unit = _tmInternalGetBestTimeUnit(exclusive_hwtime);
        
        f64 inclusive_time = os_convert_hardware_time(inclusive_hwtime, inclusive_unit);
        f64 exclusive_time = os_convert_hardware_time(exclusive_hwtime, exclusive_unit);

        int line_size = 0;
        line_size += _tmPrintPaddingTo(__TIMING_PRINT_PROC_OFFSET, line_size);
        line_size += _tmPrintPaddingTo(indentation, 0);
        line_size += printf("%s, %s", entry->procedure_name, entry->source_string);
        line_size += _tmPrintPaddingTo(__TIMING_PRINT_INCL_OFFSET, line_size);
        line_size += printf("%f%s", inclusive_time, time_unit_suffix(inclusive_unit));
        line_size += _tmPrintPaddingTo(__TIMING_PRINT_EXCL_OFFSET, line_size);
        line_size += printf("%f%s", exclusive_time, time_unit_suffix(exclusive_unit));
        printf("\n");
        
        if(entry->first_child_index != MAX_S64) {
            _tmInternalPrintTimelineEntry(&__timing.timeline[entry->first_child_index], indentation + __TIMING_INDENT_PER_PARENT);
        }
    
        if(entry->next_index == MAX_S64) break;

        entry = &__timing.timeline[entry->next_index];
    }
}

static
s64 _tmFindInsertionSortIndex(_tm_Summary_Entry *entry, Timing_Output_Sorting sorting) {
    s64 index = __timing.sorted_summary.count; // Insert at the end of the array by default.

    switch(sorting) {
    case TIMING_OUTPUT_Sort_By_Count:
        for(s64 i = 0; i < __timing.sorted_summary.count; ++i) {
            if(__timing.sorted_summary[i]->count < entry->count) {
                index = i;
                break;
            }
        }
        break;

    case TIMING_OUTPUT_Sort_By_Inclusive:
        for(s64 i = 0; i < __timing.sorted_summary.count; ++i) {
            if(__timing.sorted_summary[i]->total_inclusive_hwtime < entry->total_inclusive_hwtime) {
                index = i;
                break;
            }
        }
        break;

    case TIMING_OUTPUT_Sort_By_Exclusive:
        for(s64 i = 0; i < __timing.sorted_summary.count; ++i) {
            if(__timing.sorted_summary[i]->total_exclusive_hwtime < entry->total_exclusive_hwtime) {
                index = i;
                break;
            }
        }
        break;
    }
    
    return index;
}

static
void _tmInternalBuildSummaryTable() {
    //
    // (Re-)allocate the summary table if necessary.
    //
    _tmInternalDestroySummaryTable(true);

    //
    // Start inserting each entry into the summary table.
    //
    for(_tm_Timeline_Entry *entry : __timing.timeline) {
        u64 hash = string_hash(entry->procedure_name);
        if(hash == 0) hash = 1;

        u64 bucket_index = hash % __timing.summary_table_size;
        
        auto *bucket = &__timing.summary_table[bucket_index];
        if(bucket->hash != 0) {
            _tm_Summary_Entry *previous = null;
            
            while(bucket != null && bucket->hash != hash) {
                previous = bucket;
                bucket = bucket->next;
            }

            if(bucket) {
                //
                // An entry for this procedure already exists, add the stats onto it.
                //
                assert(compare_cstrings(bucket->procedure_name, entry->procedure_name)); // Check for hash collisions
                bucket->total_inclusive_hwtime += entry->hwtime_end - entry->hwtime_start;
                bucket->total_exclusive_hwtime += (entry->hwtime_end - entry->hwtime_start) - _tmInternalCalculateHardwareTimeOfChildren(entry);
                bucket->count                   += 1;
            } else {
                //
                // No entry for this procedure exists yet, create a new one.
                //
                assert(previous != null);
                bucket = (_tm_Summary_Entry *) Default_Allocator->allocate(sizeof(_tm_Summary_Entry));
                bucket->next                    = null;
                bucket->hash                    = hash;
                bucket->procedure_name          = entry->procedure_name;
                bucket->source_string           = entry->source_string;
                bucket->total_inclusive_hwtime  = entry->hwtime_end - entry->hwtime_start;
                bucket->total_exclusive_hwtime  = (entry->hwtime_end - entry->hwtime_start) - _tmInternalCalculateHardwareTimeOfChildren(entry);
                bucket->count                   = 1;
                previous->next                  = bucket;
            }
        } else {
            //
            // No entry for this procedure exists yet, create a new one as the first entry in this bucket.
            //
            bucket->next                    = null;
            bucket->hash                    = hash;
            bucket->procedure_name          = entry->procedure_name;
            bucket->source_string           = entry->source_string;
            bucket->total_inclusive_hwtime  = entry->hwtime_end - entry->hwtime_start;
            bucket->total_exclusive_hwtime  = (entry->hwtime_end - entry->hwtime_start) - _tmInternalCalculateHardwareTimeOfChildren(entry);
            bucket->count                   = 1;
        }
    }
}

void _tmInternalBuildSortedSummary(Timing_Output_Sorting sorting) {
    //
    // Clear out and prepare the summary array.
    //
    __timing.sorted_summary.clear();
    __timing.sorted_summary.reserve(__timing.summary_table_size);

    //
    // Do an insertion sort for all summary entries into the sorted
    // summary list.
    //
    for(s64 i = 0; i < __timing.summary_table_size; ++i) {
        auto *bucket = &__timing.summary_table[i];
        if(bucket->hash == 0) continue;

        while(bucket) {
            s64 insertion_index = _tmFindInsertionSortIndex(bucket, sorting);
            __timing.sorted_summary.insert(insertion_index, bucket);
            bucket = bucket->next;
        }
    }
}



/* -------------------------------------------- API Implementation -------------------------------------------- */

void _tmSetColor(int color_index, u8 r, u8 g, u8 b) {
    __timing.colors[color_index] = { r, g, b };
}

void _tmBegin() {
    _tmReset();
}

void _tmReset() {
    __timing.timeline.clear();
    __timing.head_index = MAX_S64;
    __timing.root_index = MAX_S64;
    __timing.total_hwtime_start = os_get_hardware_time();
}

void _tmDestroy() {
    _tmReset();
    _tmInternalDestroySummaryTable(false);
    __timing.sorted_summary.clear();
}

void _tmEnter(char const *procedure_name, char const *source_string, int color_index) {    
    s64 parent_index = __timing.head_index;
    __timing.head_index = __timing.timeline.count;

    //
    // Adjust the parent's indices.
    //
    if(parent_index != MAX_S64) {
        auto parent = &__timing.timeline[parent_index];

        if(parent->first_child_index != MAX_S64) {
            auto previous = &__timing.timeline[parent->last_child_index];
            previous->next_index = __timing.head_index;
        } else {
            parent->first_child_index = __timing.head_index;
        }

        parent->last_child_index = __timing.head_index;
    } else {
        if(__timing.root_index != MAX_S64) {
            auto previous = &__timing.timeline[__timing.root_index];
            previous->next_index = __timing.head_index;
        }
        __timing.root_index = __timing.head_index;
    }

    //
    // Set up this procedure entry.
    //
    auto entry = __timing.timeline.push();    
    entry->procedure_name    = procedure_name;
    entry->source_string     = source_string;
    entry->parent_index      = parent_index;
    entry->next_index        = MAX_S64;
    entry->first_child_index = MAX_S64;
    entry->last_child_index  = MAX_S64;
    entry->color_index       = color_index;
    entry->hwtime_end        = 0;
    entry->hwtime_start      = os_get_hardware_time();
}

void _tmExit() {
    Hardware_Time end = os_get_hardware_time();

    assert(__timing.head_index != MAX_S64);
    auto entry = &__timing.timeline[__timing.head_index];
    entry->hwtime_end = end;

    __timing.head_index = entry->parent_index;    
}

void _tmFinish() {
    __timing.total_hwtime_end = os_get_hardware_time();
    _tmInternalBuildSummaryTable();
}



/* ---------------------------------------------- Timing Export ---------------------------------------------- */

void tmPrintToConsole(Timing_Output_Mode mode, Timing_Output_Sorting sorting) {
    if(mode != TIMING_OUTPUT_None) {
        int line_size = 0;
        line_size += _tmPrintPaddingTo(__TIMING_PRINT_PROC_OFFSET, line_size);
        line_size += printf("Procedure");
        line_size += _tmPrintPaddingTo(__TIMING_PRINT_INCL_OFFSET, line_size);
        line_size += printf("Inclusive");
        line_size += _tmPrintPaddingTo(__TIMING_PRINT_EXCL_OFFSET, line_size);
        line_size += printf("Exclusive");
        line_size += _tmPrintPaddingTo(__TIMING_PRINT_COUN_OFFSET, line_size);                        
        line_size += printf("Count");
        printf("\n");
    }

    if(mode & TIMING_OUTPUT_Timeline) {
        s32 half_length = (s32) (__TIMING_PRINT_COUN_OFFSET + cstring_length("Count") - cstring_length(" PROFILING TIMELINE ") + 1) / 2;
        _tmPrintRepeated('-', half_length);
        printf(" PROFILING TIMELINE ");
        _tmPrintRepeated('-', half_length);
        printf("\n");
        
        _tmInternalPrintTimelineEntry(&__timing.timeline[0], __TIMING_INITIAL_INDENT);
    
        _tmPrintRepeated('-', half_length);
        printf(" PROFILING TIMELINE ");
        _tmPrintRepeated('-', half_length);
        printf("\n");
    }

    if(mode & TIMING_OUTPUT_Summary) {
        _tmInternalBuildSortedSummary(sorting);

        s32 half_length = (s32) (__TIMING_PRINT_COUN_OFFSET + cstring_length("Count") - cstring_length(" PROFILING STATISTICS ") + 1) / 2;
        _tmPrintRepeated('-', half_length);
        printf(" PROFILING STATISTICS ");
        _tmPrintRepeated('-', half_length);
        printf("\n");

        for(_tm_Summary_Entry **iterator : __timing.sorted_summary) {
            _tm_Summary_Entry *entry = *iterator;
            Time_Unit inclusive_unit = _tmInternalGetBestTimeUnit(entry->total_inclusive_hwtime);
            Time_Unit exclusive_unit = _tmInternalGetBestTimeUnit(entry->total_exclusive_hwtime);
                
            f64 inclusive_time = os_convert_hardware_time(entry->total_inclusive_hwtime, inclusive_unit);
            f64 exclusive_time = os_convert_hardware_time(entry->total_exclusive_hwtime, exclusive_unit);

            int line_size = 0;
            line_size += _tmPrintPaddingTo(__TIMING_PRINT_PROC_OFFSET, line_size);
            line_size += printf("%s, %s", entry->procedure_name, entry->source_string);
            line_size += _tmPrintPaddingTo(__TIMING_PRINT_INCL_OFFSET, line_size);
            line_size += printf("%f%s", inclusive_time, time_unit_suffix(inclusive_unit));
            line_size += _tmPrintPaddingTo(__TIMING_PRINT_EXCL_OFFSET, line_size);
            line_size += printf("%f%s", exclusive_time, time_unit_suffix(exclusive_unit));
            line_size += _tmPrintPaddingTo(__TIMING_PRINT_COUN_OFFSET, line_size);
            line_size += printf("%" PRId64, entry->count);
            printf("\n");
        }

        _tmPrintRepeated('-', half_length);
        printf(" PROFILING STATISTICS ");
        _tmPrintRepeated('-', half_length);
        printf("\n");    
    }
}

Timing_Data tmData(Timing_Output_Sorting sorting) {
    _tmInternalBuildSortedSummary(sorting);

    Timing_Data data;

    //
    // Set up the data to return.
    //
    
    data.timeline_count = __timing.timeline.count;
    data.summary_count  = __timing.sorted_summary.count;
    
    f64 total_time = (f64) (__timing.total_hwtime_end - __timing.total_hwtime_start);
    
    data.timeline = (Timing_Timeline_Entry *) Default_Allocator->allocate(data.timeline_count * sizeof(Timing_Timeline_Entry));
    data.summary  = (Timing_Summary_Entry *)  Default_Allocator->allocate(data.summary_count  * sizeof(Timing_Summary_Entry));

    data.total_time_in_nanoseconds = (s64) os_convert_hardware_time(__timing.total_hwtime_end - __timing.total_hwtime_start, Nanoseconds);
    
    //
    // Set up the timeline entries.
    //
    
    for(s64 i = 0; i < data.timeline_count; ++i) {
        auto *source = &__timing.timeline[i];
        data.timeline[i].name                 = cstring_view(source->procedure_name);
        data.timeline[i].start_in_nanoseconds = (s64) os_convert_hardware_time(source->hwtime_start - __timing.total_hwtime_start, Nanoseconds);
        data.timeline[i].end_in_nanoseconds   = (s64) os_convert_hardware_time(source->hwtime_end   - __timing.total_hwtime_start, Nanoseconds);
        data.timeline[i].depth                = _tmInternalCalculateStackDepth(source);
        data.timeline[i].r = __timing.colors[source->color_index].r;
        data.timeline[i].g = __timing.colors[source->color_index].g;
        data.timeline[i].b = __timing.colors[source->color_index].b;
    }

    //
    // Set up the summary entries.
    //

    for(s64 i = 0; i < data.summary_count; ++i) {
        auto *source = __timing.sorted_summary[i];
        data.summary[i].name                          = cstring_view(source->procedure_name);
        data.summary[i].inclusive_time_in_nanoseconds = (s64) os_convert_hardware_time(source->total_inclusive_hwtime, Nanoseconds);
        data.summary[i].exclusive_time_in_nanoseconds = (s64) os_convert_hardware_time(source->total_exclusive_hwtime, Nanoseconds);
        data.summary[i].count                         = source->count;
    }
    
    return data;
}

void tmFreeData(Timing_Data *data) {
    Default_Allocator->deallocate(data->timeline);
    data->timeline_count = 0;
    Default_Allocator->deallocate(data->summary);
    data->summary_count = 0;
}
