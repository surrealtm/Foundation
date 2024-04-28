#include "../src/hash_table.h"
#include "../src/strings.h"
#include "../src/random.h"
#include "../src/os_specific.h"

#include <typeinfo>

#define RANDOM_HASH false

#if RANDOM_HASH
    auto hash = [](const s64 &k) -> u64 { return get_random_u32(); };
#else
//    auto hash = [](const s64 &k) -> u64 { return murmur_64a(k); };
    auto hash = [](const s64 &k) -> u64 { return fnv1a_64(&k, sizeof(s64)); };
#endif

template<typename T>
void test_hash_table_collisions() {
    s64 table_size = 1048576;
    f64 load_factor = .5;

    printf("===== Testing Hash Table '%s' Collisions ====.\n", typeid(T).name());

    for(s64 i = 0; i < 6; ++i) {
        {
            T table;
            table.create(table_size, hash, [](const s64 &lhs, const s64 &rhs) -> b8 { return lhs == rhs; });

            s64 items = (s64) (table_size * load_factor);
            
            Hardware_Time start = os_get_hardware_time();
            u64 start_cycle = os_get_current_cpu_cycle();
            for(s64 j = 0; j < items; ++j) {
                table.add(j, j);
            }
            u64 end_cycle = os_get_current_cpu_cycle();
            Hardware_Time end = os_get_hardware_time();

            printf("-- Entries: %" PRId64 ", Table size: %" PRId64 ".\n", table.count, table.bucket_count);
            printf("    Collisions: %" PRId64 ", Load Factor: %f.\n", table.stats.collisions, table.stats.load_factor);
            printf("    Collisions Per Entry: %f.\n", (f64) table.stats.collisions / (f64) table.count);
            printf("    Expected Collisions: %f (%f).\n", table.expected_number_of_collisions(), table.stats.collisions / table.expected_number_of_collisions());
            printf("    Time / Add: %f%s, Cycles / Add: %f.\n",os_convert_hardware_time((end - start) / (f64) items, Nanoseconds), time_unit_suffix(Nanoseconds), (end_cycle - start_cycle) / (f64) items);

            table.destroy();
        }

        table_size *= 2;
    }
}

template<typename T>
void test_hash_table_correctness() {
    s64 sizes[] = { 10, 99, 10000, MAX_S16, 16777216 };
    f64 load_factor = .75;

    printf("===== Testing Hash Table '%s' Performance ====.\n", typeid(T).name());

    for(s64 size : sizes) {
        T table;
        table.create(size, hash, [](const s64 &lhs, const s64 &rhs) -> b8 { return lhs == rhs; });
        s64 items = (s64) (size * load_factor);

        Hardware_Time start = os_get_hardware_time();

        for(s64 i = 0; i < items; ++i) {
            table.add(i, i);
        }

        for(s64 i = 0; i < items / 3; ++i) {
            table.remove(items / 3 + i);
        }

        table.resize(size * 2);
        
        for(s64 i = 0; i < items; ++i) {
            table.add(i, i);
        }

        for(s64 i = 0; i < items / 3; ++i) {
            table.remove(items / 3 * 2 + i);
        }
        
        for(s64 i = 0; i < items / 3; ++i) {
            auto *value = table.query(i);
            assert(value != NULL && *value == i);
        }

        printf("-- Entries: %" PRId64 ", Table size: %" PRId64 ".\n", table.count, table.bucket_count);
        printf("    Collisions: %" PRId64 ", Load Factor: %f.\n", table.stats.collisions, table.stats.load_factor);
        
        Hardware_Time end = os_get_hardware_time();
        printf("    Took %.3fms.\n", os_convert_hardware_time(end - start, Milliseconds));
        
        table.destroy();
    }
}

int main() {
    test_hash_table_correctness<Chained_Hash_Table<s64, s64>>();
    test_hash_table_correctness<Probed_Hash_Table<s64, s64>>();
    test_hash_table_collisions<Chained_Hash_Table<s64, s64>>();
    test_hash_table_collisions<Probed_Hash_Table<s64, s64>>();
    assert(Default_Allocator->stats.allocations == Default_Allocator->stats.deallocations);
    return 0;
}
