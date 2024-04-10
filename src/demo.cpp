#include "foundation.h"
#include "os_specific.h"

int main() {
    printf("Starting...\n");

    auto start = os_get_hardware_time();

    for(int i = 0; i < 10; ++i) {
        os_sleep(0.5);
    }
    
    auto end = os_get_hardware_time();

    Time_Unit unit = Seconds;
    
    printf("Took: %f%s\n", os_convert_hardware_time(end - start, unit), time_unit_suffix(unit));
    
    return 0;
}
