#pragma once

#include "foundation.h"

enum Sort_Comparison_Result {
    SORT_Lhs_Is_Smaller = -1,
    SORT_Lhs_Equals_Rhs =  0,
    SORT_Lhs_Is_Bigger  = +1,

    SORT_Rhs_Is_Bigger = -1,
    SORT_Rhs_Equals_Lhs = 0,
    SORT_Rhs_Is_Smaller = +1,    
};

template<typename T>
void sort(T *array, s64 count, Sort_Comparison_Result(*compare)(T *, T *));

// Because C++ is a terrible language, we need to supply the template definitions in the header file for
// instantiation to work correctly... This feels horrible but still better than just inlining the code I guess.
#include "sort.inl"
