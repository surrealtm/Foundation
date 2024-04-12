#include "foundation.h"
#include "os_specific.h"
#include "fileio.h"

int main() {
    f64 result;
    b8 valid;
    result = string_to_double("+2350.001578"_s, &valid);

    if(valid) {
        printf("Result: %f\n", result);
    } else {
        printf("(Invalid)\n");
    }
    return 0;
}
