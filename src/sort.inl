//
// Careful:
// This source file gets #include'd in the header file, because templates are shit!
//

#define sort_swap(lhs, rhs) { auto tmp = *lhs; *lhs = *rhs; *rhs = tmp; }

template<typename T>
static
void internal_quick_sort(T *array, Sort_Comparison_Result(*compare)(T *, T *), s64 low, s64 high) {
    if(low < 0 || low >= high) return;

    //
    // Split the interval into two partitions.
    // Choose the middle element as the pivot.
    //
    T pivot_element = array[(low + high) / 2];

    s64 lt = low;
    s64 eq = low;
    s64 gt = high;

    while(eq <= gt) {
        if(compare(&array[eq], &pivot_element) < 0) {
            // The value at the equal-index isn't actually equal to the pivot, swap it to be at the lesser-index.
            // We now have one more 'lesser' value, and one less 'equal' value.
            sort_swap(&array[eq], &array[lt]);
            ++lt;
            ++eq;
        } else if(compare(&array[eq], &pivot_element) > 0) {
            // The value at the equal-index isn't actually equal to the pivot, swap it to be at the greater-index.
            // We now have one more 'greater' value, and one less 'equal' value.
            sort_swap(&array[eq], &array[gt]);
            --gt;
        } else {
            // The value at the equal-index is equal to the pivot, so no swapping needed.
            ++eq;
        }
    }

    //
    // Recurse the partitions
    //
    internal_quick_sort(array, compare, low, lt - 1);
    internal_quick_sort(array, compare, gt + 1, high);
}

template<typename T>
void sort(T *array, s64 count, Sort_Comparison_Result(*compare)(T *, T *)) {
    internal_quick_sort(array, compare, 0, count - 1);
}
