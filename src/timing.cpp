#include "timing.h"
#include "memutils.h"
#include "os_specific.h"
#include "threads.h"

/* ----------------------------------------- Internal Implementation ----------------------------------------- */

#define __TM_TRACK_OVERHEAD    true
#define __TM_INDENT_PER_PARENT 2
#define __TM_INITIAL_INDENT    0
#define __TM_PRINT_PROC_OFFSET 0
#define __TM_PRINT_INCL_OFFSET 80
#define __TM_PRINT_CYCL_OFFSET 95
#define __TM_PRINT_EXCL_OFFSET 110
#define __TM_PRINT_COUN_OFFSET 125
#define __TM_PRINT_MTPC_OFFSET 140
#define __TM_PRINT_MCPC_OFFSET 155
#define __TM_PRINT_HEADER_SIZE __TM_PRINT_MCPC_OFFSET + 10
#define __TM_MAX_COLORS        32
#define __TM_MAX_THREADS       16

#define _tmPrintRepeated(char, count) for(s64 __j = 0; __j < count; ++__j) printf("%c", char);
#define _tmPrintHeader() _tmPrintRepeated('-', half_length); printf("%.*s", header_length, header_buffer); _tmPrintRepeated('-', half_length); printf("\n");

struct _tm_Timeline_Entry {
    s64 parent_index = MAX_S64;
    s64 next_index = MAX_S64;
    s64 first_child_index = MAX_S64;
    s64 last_child_index = MAX_S64;

    char const *procedure_name;
    char const *source_string;
    Hardware_Time hwtime_start;
    Hardware_Time hwtime_end;
    s64 cycle_start;
    s64 cycle_end;

    u8 color_index;
};

struct _tm_Summary_Entry {
    _tm_Summary_Entry *next = null;
    
    u64 hash;
    char const *procedure_name;
    char const *source_string;
    Hardware_Time total_inclusive_hwtime;
    Hardware_Time total_exclusive_hwtime;
    s64 total_inclusive_cycles;
    s64 count;
};

struct _tm_Color {
    u8 r = 100, g = 100, b = 200;
};

struct _tm_Thread_State {
    s64 thread_index; // Index into the _tm_State's thread array.
    u32 thread_id;
    
    Resizable_Array<_tm_Timeline_Entry> timeline; // @@Speed: Change this to a bucket list or something, to avoid having to copy everything again...
    s64 head_index = MAX_S64;
    s64 root_index = MAX_S64; // The current root function (the function without a parent in the profiling mechanism).

    Hardware_Time total_overhead_hwtime;
};

struct _tm_State {
    u64 setup = false; // Initializes the mutex on the first call. u64 to support atomic behaviour.
    Mutex thread_array_mutex;

    _tm_Thread_State threads[__TM_MAX_THREADS];
    _tm_Thread_State **thread_local_pointers[__TM_MAX_THREADS]; // Pointers to the thread_local pointers, so that we can clear out these pointers when timing gets reset.
    s64 thread_count = 0;

    _tm_Summary_Entry *summary_table = null;
    s64 summary_table_size = 0;

    Resizable_Array<_tm_Summary_Entry*> sorted_summary;

    _tm_Color colors[__TM_MAX_COLORS];

    Hardware_Time total_hwtime_start = 0;
    Hardware_Time total_hwtime_end = 0;

    s64 total_cycle_start = 0;
    s64 total_cycle_end   = 0;
};

thread_local _tm_Thread_State *_tm_thread = null;

static _tm_State _tm_state;



/* ---------------------------------------- Thread Independent Helpers ---------------------------------------- */

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
Time_Unit _tmInternalGetBestTimeUnit(Hardware_Time hwtime) {
    Time_Unit unit = Nanoseconds;
    while(unit < Minutes && os_convert_hardware_time(hwtime, unit) >= 1000) unit = (Time_Unit) (unit + 1);
    return unit;
}



/* ----------------------------------------- Thread Dependent Helpers ----------------------------------------- */

static
Hardware_Time _tmInternalCalculateHardwareTimeOfChildren(_tm_Thread_State *thread, _tm_Timeline_Entry *entry) {
    if(entry->first_child_index == MAX_S64) return 0;
    
    Hardware_Time result = 0;

    auto *child = &thread->timeline[entry->first_child_index];
    while(true) {
        result += child->hwtime_end - child->hwtime_start;
        if(child->next_index == MAX_S64) break;
        child = &thread->timeline[child->next_index];
    }

    return result;
}

static
s64 _tmInternalCalculateStackDepth(_tm_Thread_State *thread, _tm_Timeline_Entry *entry) {
    s64 depth = 0;

    while(entry->parent_index != MAX_S64) {
        entry = &thread->timeline[entry->parent_index];
        ++depth;
    }
    
    return depth;
}

static
void _tmInternalPrintTimelineEntry(_tm_Thread_State *thread, _tm_Timeline_Entry *entry, u32 indentation) {
    while(true) {
        Hardware_Time inclusive_hwtime = entry->hwtime_end - entry->hwtime_start;
        Hardware_Time exclusive_hwtime = inclusive_hwtime - _tmInternalCalculateHardwareTimeOfChildren(thread, entry);

        Time_Unit inclusive_unit = _tmInternalGetBestTimeUnit(inclusive_hwtime);
        Time_Unit exclusive_unit = _tmInternalGetBestTimeUnit(exclusive_hwtime);
        
        f64 inclusive_time = os_convert_hardware_time(inclusive_hwtime, inclusive_unit);
        f64 exclusive_time = os_convert_hardware_time(exclusive_hwtime, exclusive_unit);

        int line_size = 0;
        line_size += _tmPrintPaddingTo(__TM_PRINT_PROC_OFFSET, line_size);
        line_size += _tmPrintPaddingTo(indentation, 0);
        line_size += printf("%s, %s", entry->procedure_name, entry->source_string);
        line_size += _tmPrintPaddingTo(__TM_PRINT_INCL_OFFSET, line_size);
        line_size += printf("%.2f%s", inclusive_time, time_unit_suffix(inclusive_unit));
        line_size += _tmPrintPaddingTo(__TM_PRINT_CYCL_OFFSET, line_size);
        line_size += printf("%" PRId64, entry->cycle_end - entry->cycle_start);
        line_size += _tmPrintPaddingTo(__TM_PRINT_EXCL_OFFSET, line_size);
        line_size += printf("%.2f%s", exclusive_time, time_unit_suffix(exclusive_unit));
        printf("\n");
        
        if(entry->first_child_index != MAX_S64) {
            _tmInternalPrintTimelineEntry(thread, &thread->timeline[entry->first_child_index], indentation + __TM_INDENT_PER_PARENT);
        }
    
        if(entry->next_index == MAX_S64) break;

        entry = &thread->timeline[entry->next_index];
    }
}

static
void _tmInternalPrintSummaryEntry(_tm_Summary_Entry *entry) {
    f64 mhwtpc = (f64) entry->total_inclusive_hwtime / (f64) entry->count;
    f64 mcpc = (f64) entry->total_inclusive_cycles / (f64) entry->count;

    Time_Unit inclusive_unit = _tmInternalGetBestTimeUnit(entry->total_inclusive_hwtime);
    Time_Unit exclusive_unit = _tmInternalGetBestTimeUnit(entry->total_exclusive_hwtime);
    Time_Unit mtpc_unit      = _tmInternalGetBestTimeUnit((Hardware_Time) mhwtpc);
                
    f64 inclusive_time = os_convert_hardware_time(entry->total_inclusive_hwtime, inclusive_unit);
    f64 exclusive_time = os_convert_hardware_time(entry->total_exclusive_hwtime, exclusive_unit);
    f64 mtpc_time      = os_convert_hardware_time(mhwtpc, mtpc_unit);

    int line_size = 0;
    line_size += _tmPrintPaddingTo(__TM_PRINT_PROC_OFFSET, line_size);
    line_size += printf("%s, %s", entry->procedure_name, entry->source_string);
    line_size += _tmPrintPaddingTo(__TM_PRINT_INCL_OFFSET, line_size);
    line_size += printf("%.2f%s", inclusive_time, time_unit_suffix(inclusive_unit));
    line_size += _tmPrintPaddingTo(__TM_PRINT_CYCL_OFFSET, line_size);
    line_size += printf("%" PRId64, entry->total_inclusive_cycles);
    line_size += _tmPrintPaddingTo(__TM_PRINT_EXCL_OFFSET, line_size);
    line_size += printf("%.2f%s", exclusive_time, time_unit_suffix(exclusive_unit));
    line_size += _tmPrintPaddingTo(__TM_PRINT_COUN_OFFSET, line_size);
    line_size += printf("%" PRId64, entry->count);
    line_size += _tmPrintPaddingTo(__TM_PRINT_MTPC_OFFSET, line_size);
    line_size += printf("%.2f%s", mtpc_time, time_unit_suffix(mtpc_unit));
    line_size += _tmPrintPaddingTo(__TM_PRINT_MCPC_OFFSET, line_size);
    line_size += printf("%.2f", mcpc);
    printf("\n");
}


static
void _tmInternalDestroySummaryTable(b8 reallocate) {
    //
    // Calculate the total amount of timeline entries, to get an estimation of
    // how many summary entries we will have.
    //
    s64 total_timeline_count = 0;
    s64 summary_table_size_estimation = 0;

    if(reallocate) {
        for(s64 i = 0; i < _tm_state.thread_count; ++i) {
            total_timeline_count += _tm_state.threads[i].timeline.count;
        }

        summary_table_size_estimation = (s64) ceil((f64) total_timeline_count / 3.0);
    }

    //
    // If the summary table is currently empty, then we don't need to clean anything up.
    //
    if(_tm_state.summary_table_size == 0) {
        if(reallocate) {
            _tm_state.summary_table_size = summary_table_size_estimation;
            _tm_state.summary_table = (_tm_Summary_Entry *) Default_Allocator->allocate(_tm_state.summary_table_size * sizeof(_tm_Summary_Entry));
            memset(_tm_state.summary_table, 0, _tm_state.summary_table_size * sizeof(_tm_Summary_Entry));
        }

        return;
    }

    //
    // Destroy the single allocated entries inside each bucket.
    //
    for(s64 i = 0; i < _tm_state.summary_table_size; ++i) {
        auto bucket = _tm_state.summary_table[i].next; // The first entry is not allocated.
        
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
        if(fabs(1 - (f64) summary_table_size_estimation / (f64) _tm_state.summary_table_size) > 0.5) {
            Default_Allocator->deallocate(_tm_state.summary_table);
            _tm_state.summary_table_size = summary_table_size_estimation;
            _tm_state.summary_table = (_tm_Summary_Entry *) Default_Allocator->allocate(_tm_state.summary_table_size * sizeof(_tm_Summary_Entry));
        }

        memset(_tm_state.summary_table, 0, _tm_state.summary_table_size * sizeof(_tm_Summary_Entry));
    } else {
        Default_Allocator->deallocate(_tm_state.summary_table);
        _tm_state.summary_table_size = 0;
    }
}

static
s64 _tmFindInsertionSortIndex(_tm_Summary_Entry *entry, Timing_Output_Sorting sorting) {
    s64 index = _tm_state.sorted_summary.count; // Insert at the end of the array by default.

    switch(sorting) {
    case TIMING_OUTPUT_Sort_By_Count:
        for(s64 i = 0; i < _tm_state.sorted_summary.count; ++i) {
            if(_tm_state.sorted_summary[i]->count < entry->count) {
                index = i;
                break;
            }
        }
        break;

    case TIMING_OUTPUT_Sort_By_Inclusive:
        for(s64 i = 0; i < _tm_state.sorted_summary.count; ++i) {
            if(_tm_state.sorted_summary[i]->total_inclusive_hwtime < entry->total_inclusive_hwtime) {
                index = i;
                break;
            }
        }
        break;

    case TIMING_OUTPUT_Sort_By_Exclusive:
        for(s64 i = 0; i < _tm_state.sorted_summary.count; ++i) {
            if(_tm_state.sorted_summary[i]->total_exclusive_hwtime < entry->total_exclusive_hwtime) {
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
    for(s64 i = 0; i < _tm_state.thread_count; ++i) {
        _tm_Thread_State *thread = &_tm_state.threads[i];
        for(_tm_Timeline_Entry &entry : thread->timeline) {
            u64 hash = string_hash(entry.procedure_name);
            if(hash == 0) hash = 1;

            u64 bucket_index = hash % _tm_state.summary_table_size;
        
            auto *bucket = &_tm_state.summary_table[bucket_index];
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
                    assert(compare_cstrings(bucket->procedure_name, entry.procedure_name)); // Check for hash collisions
                    bucket->total_inclusive_hwtime += entry.hwtime_end - entry.hwtime_start;
                    bucket->total_exclusive_hwtime += entry.hwtime_end - entry.hwtime_start - _tmInternalCalculateHardwareTimeOfChildren(thread, &entry);
                    bucket->total_inclusive_cycles += entry.cycle_end - entry.cycle_start;
                    bucket->count                  += 1;
                } else {
                    //
                    // No entry for this procedure exists yet, create a new one.
                    //
                    assert(previous != null);
                    bucket = (_tm_Summary_Entry *) Default_Allocator->allocate(sizeof(_tm_Summary_Entry));
                    bucket->next                    = null;
                    bucket->hash                    = hash;
                    bucket->procedure_name          = entry.procedure_name;
                    bucket->source_string           = entry.source_string;
                    bucket->total_inclusive_hwtime  = entry.hwtime_end - entry.hwtime_start;
                    bucket->total_exclusive_hwtime  = entry.hwtime_end - entry.hwtime_start - _tmInternalCalculateHardwareTimeOfChildren(thread, &entry);
                    bucket->total_inclusive_cycles  = entry.cycle_end - entry.cycle_start;
                    bucket->count                   = 1;
                    previous->next                  = bucket;
                }
            } else {
                //
                // No entry for this procedure exists yet, create a new one as the first entry in this bucket.
                //
                bucket->next                    = null;
                bucket->hash                    = hash;
                bucket->procedure_name          = entry.procedure_name;
                bucket->source_string           = entry.source_string;
                bucket->total_inclusive_hwtime  = entry.hwtime_end - entry.hwtime_start;
                bucket->total_exclusive_hwtime  = entry.hwtime_end - entry.hwtime_start - _tmInternalCalculateHardwareTimeOfChildren(thread, &entry);
                bucket->total_inclusive_cycles  = entry.cycle_end - entry.cycle_start;
                bucket->count                   = 1;
            }
        }
    }
}

static
void _tmInternalBuildSortedSummary(Timing_Output_Sorting sorting) {
    //
    // Actually build the summary table.
    //
    _tmInternalBuildSummaryTable();

    //
    // Clear out and prepare the summary array.
    //
    _tm_state.sorted_summary.clear();
    _tm_state.sorted_summary.reserve(_tm_state.summary_table_size);

    //
    // Do an insertion sort for all summary entries into the sorted
    // summary list.
    //
    for(s64 i = 0; i < _tm_state.summary_table_size; ++i) {
        auto *bucket = &_tm_state.summary_table[i];
        if(bucket->hash == 0) continue;

        while(bucket) {
            s64 insertion_index = _tmFindInsertionSortIndex(bucket, sorting);
            _tm_state.sorted_summary.insert(insertion_index, bucket);
            bucket = bucket->next;
        }
    }
}



/* -------------------------------------------- API Implementation -------------------------------------------- */

void _tmSetColor(int color_index, u8 r, u8 g, u8 b) {
    _tm_state.colors[color_index] = { r, g, b };
}

void _tmReset() {
    if(_tm_state.setup) {
        lock(&_tm_state.thread_array_mutex);

        for(s64 i = 0; i < _tm_state.thread_count; ++i) {
            // Reset all data which is tracked during timing. The summary table remains to maybe avoid a
            // reallocation later on, in case the amount of summaries doesn't change.
            _tm_Thread_State *thread = &_tm_state.threads[i];
            thread->timeline.clear();
            thread->head_index            = MAX_S64;
            thread->root_index            = MAX_S64;
            thread->total_overhead_hwtime = 0;

            *_tm_state.thread_local_pointers[i] = null;
            _tm_state.thread_local_pointers[i]  = null;
        }

        _tm_state.thread_count = 0;
        unlock(&_tm_state.thread_array_mutex);
    }

    _tm_state.total_hwtime_start = os_get_hardware_time();
    _tm_state.total_cycle_start  = os_get_cpu_cycle();
}

void _tmDestroy() {
    if(_tm_state.setup) {
        lock(&_tm_state.thread_array_mutex);
    
        for(s64 i = 0; i < _tm_state.thread_count; ++i) {
            _tm_Thread_State *thread = &_tm_state.threads[i];
            thread->timeline.clear();
            thread->head_index            = MAX_S64;
            thread->root_index            = MAX_S64;
            thread->total_overhead_hwtime = 0;
            
            *_tm_state.thread_local_pointers[i] = null;
            _tm_state.thread_local_pointers[i]  = null;
        }
        
        _tmInternalDestroySummaryTable(false);
        _tm_state.sorted_summary.clear();
            
        _tm_state.thread_count = 0;
        _tm_state.setup = false;
        unlock(&_tm_state.thread_array_mutex);
        destroy_mutex(&_tm_state.thread_array_mutex); // Make sure another thread didn't delete this under our feet.
    }
}

void _tmEnter(char const *procedure_name, char const *source_string, int color_index) {   
#if __TM_TRACK_OVERHEAD
    s64 overhead_start = os_get_hardware_time();
#endif

    //
    // Set up the global timing state.
    //
    u64 setup_value_before = interlocked_compare_exchange(&_tm_state.setup, true, false);
    if(!setup_value_before) {
        create_mutex(&_tm_state.thread_array_mutex);
        _tm_state.setup = true;
    }

    //
    // Register this thread in the global timing state.
    //
    if(!_tm_thread) {
        assert(_tm_state.thread_count < __TM_MAX_THREADS);

        lock(&_tm_state.thread_array_mutex);
        _tm_thread = &_tm_state.threads[_tm_state.thread_count];
        _tm_state.thread_local_pointers[_tm_state.thread_count] = &_tm_thread;
        ++_tm_state.thread_count;
        unlock(&_tm_state.thread_array_mutex);

        _tm_thread->thread_id = thread_get_id();
    }

    s64 parent_index = _tm_thread->head_index;
    _tm_thread->head_index = _tm_thread->timeline.count;

    //
    // Adjust the parent's indices.
    //
    if(parent_index != MAX_S64) {
        auto parent = &_tm_thread->timeline[parent_index];

        if(parent->first_child_index != MAX_S64) {
            auto previous = &_tm_thread->timeline[parent->last_child_index];
            previous->next_index = _tm_thread->head_index;
        } else {
            parent->first_child_index = _tm_thread->head_index;
        }

        parent->last_child_index = _tm_thread->head_index;
    } else {
        if(_tm_thread->root_index != MAX_S64) {
            auto previous = &_tm_thread->timeline[_tm_thread->root_index];
            previous->next_index = _tm_thread->head_index;
        }
        _tm_thread->root_index = _tm_thread->head_index;
    }

    //
    // Set up this procedure entry.
    //
    auto entry = _tm_thread->timeline.push();    
    entry->procedure_name    = procedure_name;
    entry->source_string     = source_string;
    entry->parent_index      = parent_index;
    entry->next_index        = MAX_S64;
    entry->first_child_index = MAX_S64;
    entry->last_child_index  = MAX_S64;
    entry->color_index       = color_index >= 0 ? (u8) color_index : (__TM_MAX_COLORS - 1);
    entry->hwtime_end        = 0;
    entry->hwtime_start      = os_get_hardware_time();
    entry->cycle_start       = os_get_cpu_cycle();

#if __TM_TRACK_OVERHEAD
    _tm_thread->total_overhead_hwtime += entry->hwtime_start - overhead_start;
#endif
}

void _tmExit() {
    s64 cycle_end = os_get_cpu_cycle();
    Hardware_Time hw_end = os_get_hardware_time();

    assert(_tm_thread->head_index != MAX_S64);
    auto entry = &_tm_thread->timeline[_tm_thread->head_index];
    entry->hwtime_end = hw_end;
    entry->cycle_end  = cycle_end;

    _tm_thread->head_index = entry->parent_index;    

#if __TM_TRACK_OVERHEAD
    _tm_thread->total_overhead_hwtime += os_get_hardware_time() - hw_end;
#endif
}

void _tmFinish() {
    _tm_state.total_hwtime_end = os_get_hardware_time();
}



/* ---------------------------------------------- Timing Export ---------------------------------------------- */

void tmPrintToConsole(Timing_Output_Mode mode, Timing_Output_Sorting sorting) {
    if(mode != TIMING_OUTPUT_None) {
        printf("\n\n\n");

        int line_size = 0;
        line_size += _tmPrintPaddingTo(__TM_PRINT_PROC_OFFSET, line_size);
        line_size += printf("Procedure");
        line_size += _tmPrintPaddingTo(__TM_PRINT_INCL_OFFSET, line_size);
        line_size += printf("Inclusive");
        line_size += _tmPrintPaddingTo(__TM_PRINT_CYCL_OFFSET, line_size);
        line_size += printf("(Cycles)");
        line_size += _tmPrintPaddingTo(__TM_PRINT_EXCL_OFFSET, line_size);
        line_size += printf("Exclusive");
        line_size += _tmPrintPaddingTo(__TM_PRINT_COUN_OFFSET, line_size);                        
        line_size += printf("Count");
        line_size += _tmPrintPaddingTo(__TM_PRINT_MTPC_OFFSET, line_size);                        
        line_size += printf("MTPC");
        line_size += _tmPrintPaddingTo(__TM_PRINT_MCPC_OFFSET, line_size);                        
        line_size += printf("(Cycles)");
        printf("\n");
    }

    s64 total_overhead_space  = _tm_state.summary_table_size * sizeof(_tm_Summary_Entry) + _tm_state.sorted_summary.allocated * sizeof(_tm_Summary_Entry*);
    s64 total_overhead_hwtime = 0;
    
    char header_buffer[256];
    
    for(s64 i = 0; i < _tm_state.thread_count; ++i) {
        _tm_Thread_State *thread = &_tm_state.threads[i];
        
        if(thread->timeline.count == 0) continue;

        if(mode & TIMING_OUTPUT_Timeline) {
            s32 header_length = sprintf_s(header_buffer, " THREAD TIMELINE - %d ", thread->thread_id);            
            s32 half_length = (s32) (__TM_PRINT_HEADER_SIZE + 10 - header_length + 1) / 2;

            _tmPrintHeader();
            _tmInternalPrintTimelineEntry(thread, &thread->timeline[0], __TM_INITIAL_INDENT);
            _tmPrintHeader();
            printf("\n\n");
        }

#if __TM_TRACK_OVERHEAD
        total_overhead_hwtime += thread->total_overhead_hwtime;
        total_overhead_space  += thread->timeline.allocated * sizeof(_tm_Timeline_Entry);
#endif        
    }

    if(mode & TIMING_OUTPUT_Summary) {
        _tmInternalBuildSortedSummary(sorting);

        s32 header_length = sprintf_s(header_buffer, " PROFILING SUMMARY ");            
        s32 half_length = (s32) (__TM_PRINT_HEADER_SIZE + 10 - header_length + 1) / 2;
            
        _tmPrintHeader();
        
        for(_tm_Summary_Entry *entry : _tm_state.sorted_summary) {
            _tmInternalPrintSummaryEntry(entry);
        }

        _tmPrintHeader();    
    }

#if __TM_TRACK_OVERHEAD
    if(mode != TIMING_OUTPUT_None) {
        Time_Unit time_unit = _tmInternalGetBestTimeUnit(total_overhead_hwtime);
        f64 time = os_convert_hardware_time(total_overhead_hwtime, time_unit);
        
        f32 space;
        Memory_Unit space_unit = get_best_memory_unit(total_overhead_space, &space);

        s32 header_length = sprintf_s(header_buffer, " PROFILING OVERHEAD ");            
        s32 half_length = (s32) (__TM_PRINT_HEADER_SIZE + 10 - header_length + 1) / 2;
    
        _tmPrintHeader();
        printf("  Time:  %f.2%s\n", time, time_unit_suffix(time_unit));
        printf("  Space: %f.2%s\n", space, memory_unit_suffix(space_unit));
        _tmPrintHeader();
    }
#endif
}

Timing_Data tmData(Timing_Output_Sorting sorting) {
    Timing_Data data = { 0 };

    //
    // Prepare the exported data.
    //
    _tmInternalBuildSortedSummary(sorting);
    data.total_overhead_space_in_bytes += _tm_state.summary_table_size * sizeof(_tm_Summary_Entry) + _tm_state.sorted_summary.allocated * sizeof(_tm_Summary_Entry*);
    data.total_overhead_time_in_nanoseconds = 0;
    data.total_time_in_nanoseconds = (s64) os_convert_hardware_time(_tm_state.total_hwtime_end - _tm_state.total_hwtime_start, Nanoseconds);
    
    //
    // Build the exported summary data.
    //
    data.summary_count = _tm_state.sorted_summary.count;
    data.summary = (Timing_Summary_Entry *) Default_Allocator->allocate(data.summary_count * sizeof(Timing_Summary_Entry));
    
    for(s64 i = 0; i < data.summary_count; ++i) {
        auto *source      = _tm_state.sorted_summary[i];
        auto *destination = &data.summary[i];
        destination->name                          = cstring_view(source->procedure_name);
        destination->inclusive_time_in_nanoseconds = (s64) os_convert_hardware_time(source->total_inclusive_hwtime, Nanoseconds);
        destination->exclusive_time_in_nanoseconds = (s64) os_convert_hardware_time(source->total_exclusive_hwtime, Nanoseconds);
        destination->count                         = source->count;
    }

    //
    // Build the exported timeline for every thread.
    //
    data.timelines_count = _tm_state.thread_count;
    data.timelines_entry_count = (s64 *) Default_Allocator->allocate(data.timelines_count * sizeof(s64));
    data.timelines = (Timing_Timeline_Entry **) Default_Allocator->allocate(data.timelines_count * sizeof(Timing_Timeline_Entry *));

    for(s64 i = 0; i < _tm_state.thread_count; ++i) {
        _tm_Thread_State *thread = &_tm_state.threads[i];

        data.timelines_entry_count[i] = thread->timeline.count;
        data.timelines[i] = (Timing_Timeline_Entry *) Default_Allocator->allocate(data.timelines_entry_count[i] * sizeof(Timing_Timeline_Entry));

        data.total_overhead_time_in_nanoseconds += (s64) os_convert_hardware_time(thread->total_overhead_hwtime, Nanoseconds);
        data.total_overhead_space_in_bytes += thread->timeline.allocated * sizeof(_tm_Timeline_Entry);

        for(s64 j = 0; j < data.timelines_entry_count[i]; ++j) {
            _tm_Timeline_Entry *source = &thread->timeline[j];
            Timing_Timeline_Entry *destination = &data.timelines[i][j];
        
            destination->name                 = cstring_view(source->procedure_name);
            destination->start_in_nanoseconds = (s64) os_convert_hardware_time(source->hwtime_start - _tm_state.total_hwtime_start, Nanoseconds);
            destination->end_in_nanoseconds   = (s64) os_convert_hardware_time(source->hwtime_end   - _tm_state.total_hwtime_start, Nanoseconds);
            destination->depth                = _tmInternalCalculateStackDepth(thread, source);
            destination->r = _tm_state.colors[source->color_index].r;
            destination->g = _tm_state.colors[source->color_index].g;
            destination->b = _tm_state.colors[source->color_index].b;
        }
    }

    _tmDestroy();

    return data;
}

void tmFreeData(Timing_Data *data) {
    for(s64 i = 0; i < data->timelines_count; ++i) {
        Default_Allocator->deallocate(data->timelines[i]);
    }
    
    Default_Allocator->deallocate(data->timelines_entry_count);
    data->timelines_entry_count = null;

    Default_Allocator->deallocate(data->timelines);
    data->timelines = null;
    data->timelines_count = 0;
    
    Default_Allocator->deallocate(data->summary);
    data->summary = null;
    data->summary_count = 0;
}
