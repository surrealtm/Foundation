#include "foundation.h"

const char *time_unit_suffix(Time_Unit unit) {
    const char *string = "(UnknownTimeUnit)";

    switch(unit) {
    case Nanoseconds:  string = "ns"; break;
    case Microseconds: string = "us"; break;
    case Milliseconds: string = "ms"; break;
    case Seconds:      string = "s";  break;
    case Minutes:      string = "m";  break;
    }
    
    return string;
}
