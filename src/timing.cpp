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

#define _tmPrintPaddingTo(target, current) printf("%-*s", ((target) - (current)), "")
#define _tmPrintRepeated(char, count) for(s64 i = 0; i < count; ++i) printf("%c", char);

struct Internal_Timing_Timeline_Entry {
    s64 parent_index;
    s64 next_index;
    s64 first_child_index;
    s64 last_child_index;

    char const *procedure_name;
    char const *source_string;
    Hardware_Time hwtime_start;
    Hardware_Time hwtime_end;
};

struct Internal_Timing_Summary_Entry {
    Internal_Timing_Summary_Entry *next = null;
    
    s64 hash;
    char const *procedure_name;
    char const *source_string;
    Hardware_Time total_hw_inclusive_time;
    Hardware_Time total_hw_exclusive_time;
    s64 count;
};

struct Internal_Timing_Keeper {
    Resizable_Array<Internal_Timing_Timeline_Entry> entries;
    s64 head_index = MAX_S64;

    Internal_Timing_Summary_Entry *summary_table = null;
    s64 summary_table_size = 0;
};

static Internal_Timing_Keeper __timing;



void _tmInternalDestroySummaryTable(b8 reallocate) {
    //
    // If the summary table is currently empty, then we don't need to clean anything up.
    //
    if(__timing.summary_table_size == 0) {
        if(reallocate) {
            __timing.summary_table_size = (s64) ceil((f64) __timing.entries.count / (f64) 3);
            __timing.summary_table = (Internal_Timing_Summary_Entry *) Default_Allocator->allocate(__timing.summary_table_size * sizeof(Internal_Timing_Summary_Entry));
            memset(__timing.summary_table, 0, __timing.summary_table_size * sizeof(Internal_Timing_Summary_Entry));
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
        s64 size_estimation = (s64) ceil((f64) __timing.entries.count / (f64) 3);
        if(fabs(1 - (f64) size_estimation / (f64) __timing.summary_table_size) > 0.5) {
            Default_Allocator->deallocate(__timing.summary_table);
            __timing.summary_table_size = size_estimation;
            __timing.summary_table = (Internal_Timing_Summary_Entry *) Default_Allocator->allocate(__timing.summary_table_size * sizeof(Internal_Timing_Summary_Entry));
        }

        memset(__timing.summary_table, 0, __timing.summary_table_size * sizeof(Internal_Timing_Summary_Entry));
    } else {
        Default_Allocator->deallocate(__timing.summary_table);
        __timing.summary_table_size = 0;
    }
}

Hardware_Time _tmInternalCalculateHardwareTimeOfChildren(Internal_Timing_Timeline_Entry *entry) {
    if(entry->first_child_index == MAX_S64) return 0;
    
    Hardware_Time result = 0;

    auto *child = &__timing.entries[entry->first_child_index];
    while(true) {
        result += child->hwtime_end - child->hwtime_start;
        if(child->next_index == MAX_S64) break;
        child = &__timing.entries[child->next_index];
    }

    return result;
}

Time_Unit _tmInternalGetBestTimeUnit(Hardware_Time hwtime) {
    Time_Unit unit = Nanoseconds;
    while(unit < Minutes && os_convert_hardware_time(hwtime, unit) > 1000) unit = (Time_Unit) (unit + 1);
    return unit;
}

void _tmInternalPrintTimelineEntry(Internal_Timing_Timeline_Entry *entry, u32 indentation) {
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
            _tmInternalPrintTimelineEntry(&__timing.entries[entry->first_child_index], indentation + __TIMING_INDENT_PER_PARENT);
        }
    
        if(entry->next_index == MAX_S64) break;

        entry = &__timing.entries[entry->next_index];
    }
}

void _tmInternalBuildSummaryTable() {
    //
    // (Re-)allocate the summary table if necessary.
    //
    _tmInternalDestroySummaryTable(true);

    //
    // Start inserting each entry into the summary table.
    //
    for(auto *entry : __timing.entries) {
        u64 hash = string_hash(entry->procedure_name);
        if(hash == 0) hash = 1;

        u64 bucket_index = hash % __timing.summary_table_size;
        
        auto *bucket = &__timing.summary_table[bucket_index];
        if(bucket->hash != 0) {
            Internal_Timing_Summary_Entry *previous = null;
            
            while(bucket != null && bucket->hash != hash) {
                previous = bucket;
                bucket = bucket->next;
            }

            if(bucket) {
                //
                // An entry for this procedure already exists, add the stats onto it.
                //
                assert(compare_cstrings(bucket->procedure_name, entry->procedure_name)); // Check for hash collisions
                bucket->total_hw_inclusive_time += entry->hwtime_end - entry->hwtime_start;
                bucket->total_hw_exclusive_time += (entry->hwtime_end - entry->hwtime_start) - _tmInternalCalculateHardwareTimeOfChildren(entry);
                bucket->count                   += 1;
            } else {
                //
                // No entry for this procedure exists yet, create a new one.
                //
                assert(previous != null);
                bucket = (Internal_Timing_Summary_Entry *) Default_Allocator->allocate(sizeof(Internal_Timing_Summary_Entry));
                bucket->next                    = null;
                bucket->hash                    = hash;
                bucket->procedure_name          = entry->procedure_name;
                bucket->source_string           = entry->source_string;
                bucket->total_hw_inclusive_time = entry->hwtime_end - entry->hwtime_start;
                bucket->total_hw_exclusive_time = (entry->hwtime_end - entry->hwtime_start) - _tmInternalCalculateHardwareTimeOfChildren(entry);
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
            bucket->total_hw_inclusive_time = entry->hwtime_end - entry->hwtime_start;
            bucket->total_hw_exclusive_time = (entry->hwtime_end - entry->hwtime_start) - _tmInternalCalculateHardwareTimeOfChildren(entry);
            bucket->count                   = 1;
        }
    }
}



/* -------------------------------------------- API Implementation -------------------------------------------- */

void _tmReset() {
    __timing.entries.clear();
    __timing.head_index = MAX_S64;
}

void _tmDestroy() {
    _tmReset();
    _tmInternalDestroySummaryTable(false);
}

void _tmEnter(char const *procedure_name, char const *source_string) {    
    s64 parent_index = __timing.head_index;
    __timing.head_index = __timing.entries.count;

    //
    // Adjust the parent's indices.
    //
    if(parent_index != MAX_S64) {
        auto parent = &__timing.entries[parent_index];

        if(parent->first_child_index != MAX_S64) {
            auto previous = &__timing.entries[parent->last_child_index];
            previous->next_index = __timing.head_index;
        } else {
            parent->first_child_index = __timing.head_index;
        }

        parent->last_child_index = __timing.head_index;
    }

    //
    // Set up this procedure entry.
    //
    auto entry  = __timing.entries.push();    
    entry->procedure_name    = procedure_name;
    entry->source_string     = source_string;
    entry->parent_index      = parent_index;
    entry->next_index        = MAX_S64;
    entry->first_child_index = MAX_S64;
    entry->last_child_index  = MAX_S64;
    entry->hwtime_end        = 0;
    entry->hwtime_start      = os_get_hardware_time();
}

void _tmExit() {
    Hardware_Time end = os_get_hardware_time();

    assert(__timing.head_index != MAX_S64);
    auto entry = &__timing.entries[__timing.head_index];
    entry->hwtime_end = end;

    __timing.head_index = entry->parent_index;    
}

void _tmFinishFrame(Timing_Output_Mode mode) {
    if(__timing.entries.count == 0) return;

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
        s32 half_length = (__TIMING_PRINT_COUN_OFFSET + cstring_length("Count") - cstring_length(" PROFILING TIMELINE ") + 1) / 2;
        _tmPrintRepeated('-', half_length);
        printf(" PROFILING TIMELINE ");
        _tmPrintRepeated('-', half_length);
        printf("\n");
        
        _tmInternalPrintTimelineEntry(&__timing.entries[0], __TIMING_INITIAL_INDENT);
    
        _tmPrintRepeated('-', half_length);
        printf(" PROFILING TIMELINE ");
        _tmPrintRepeated('-', half_length);
        printf("\n");
    }

    if(mode & TIMING_OUTPUT_Statistics) {
        _tmInternalBuildSummaryTable();

        s32 half_length = (__TIMING_PRINT_COUN_OFFSET + cstring_length("Count") - cstring_length(" PROFILING STATISTICS ") + 1) / 2;
        _tmPrintRepeated('-', half_length);
        printf(" PROFILING STATISTICS ");
        _tmPrintRepeated('-', half_length);
        printf("\n");

        for(s64 i = 0; i < __timing.summary_table_size; ++i) {
            auto *bucket = &__timing.summary_table[i];
            if(bucket->hash == 0) continue;

            while(bucket) {
                Time_Unit inclusive_unit = _tmInternalGetBestTimeUnit(bucket->total_hw_inclusive_time);
                Time_Unit exclusive_unit = _tmInternalGetBestTimeUnit(bucket->total_hw_exclusive_time);
                
                f64 inclusive_time = os_convert_hardware_time(bucket->total_hw_inclusive_time, inclusive_unit);
                f64 exclusive_time = os_convert_hardware_time(bucket->total_hw_exclusive_time, exclusive_unit);

                int line_size = 0;
                line_size += _tmPrintPaddingTo(__TIMING_PRINT_PROC_OFFSET, line_size);
                line_size += printf("%s, %s", bucket->procedure_name, bucket->source_string);
                line_size += _tmPrintPaddingTo(__TIMING_PRINT_INCL_OFFSET, line_size);
                line_size += printf("%f%s", inclusive_time, time_unit_suffix(inclusive_unit));
                line_size += _tmPrintPaddingTo(__TIMING_PRINT_EXCL_OFFSET, line_size);
                line_size += printf("%f%s", exclusive_time, time_unit_suffix(exclusive_unit));
                line_size += _tmPrintPaddingTo(__TIMING_PRINT_COUN_OFFSET, line_size);
                line_size += printf("%" PRId64, bucket->count);
                printf("\n");
            
                bucket = bucket->next;
            }
        }

        _tmPrintRepeated('-', half_length);
        printf(" PROFILING STATISTICS ");
        _tmPrintRepeated('-', half_length);
        printf("\n");
    }
    
    _tmReset();
}
