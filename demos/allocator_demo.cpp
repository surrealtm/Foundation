#include "memutils.h"
#include "string_type.h"
#include "os_specific.h"

#include <random>

#define LOG_FILE "ela.txt"_s
#define LOG_WIDTH 640

struct Debug_Printer {
    Resizable_Array<string> lines;

    void create(s64 line_count, s64 line_length) {
        for(s64 i = 0; i < line_count; ++i) {
            string _string = allocate_string(Default_Allocator, line_length);
            for(s64 i = 0; i < line_length; ++i) {
                _string.data[i] = ' ';            
            }
            
            this->lines.add(_string);
        }
    }

    s64 calculate_nodes_at_depth(s64 branch_factor, s64 depth) {
        if(depth < 1) return 1;

        s64 result = branch_factor;

        for(s64 i = 1; i < depth; ++i) {
            result *= branch_factor;
        }

        return result;
    }

    s64 calculate_width_per_node_at_depth(s64 branch_factor, s64 depth) {
        s64 nodes_at_depth = this->calculate_nodes_at_depth(branch_factor, depth);
        return (this->lines[0].count / nodes_at_depth);
    }

    s64 calculate_offset_for_node_at_depth(s64 branch_factor, s64 depth, s64 index_at_depth) {
        return index_at_depth * this->calculate_width_per_node_at_depth(branch_factor, depth);
    }

    s64 calculate_line_at_depth(s64 depth) {
        return depth * 2;
    }

    void insert(s64 branch_factor, s64 depth, s64 index_at_depth, string value) {
        s64 width = this->calculate_width_per_node_at_depth(branch_factor, depth);
        s64 left  = this->calculate_offset_for_node_at_depth(branch_factor, depth, index_at_depth);

        assert(value.count <= width); // The debug printer is not wide enough for the tree

        s64 line_index = this->calculate_line_at_depth(depth);
        s64 offset_in_line = left + width / 2 - value.count / 2;

        assert(line_index >= 0 && line_index < this->lines.count);
        string &line = this->lines[line_index];
        assert(offset_in_line >= 0 && offset_in_line + value.count <= line.count);

        memcpy(&line.data[offset_in_line], value.data, value.count);
    }

    void flush(string file_path, b8 append) {
        if(append) {
            string header = allocate_string(Default_Allocator, this->lines[0].count + 4);
            for(s64 i = 2; i < header.count - 2; ++i) header[i] = '-';
            header[0] = '\n';
            header[1] = '\n';
            header[header.count - 2] = '\n';
            header[header.count - 1] = '\n';
            os_write_file(file_path, header, true);
        } else {
            os_delete_file(file_path);
        }

        for(string &line : this->lines) {
            os_write_file(file_path, line, true);
            os_write_file(file_path, "\n"_s, true);
        }
    }
};

struct Ela_Block {
    void *user_pointer;
    u32 size_in_bytes;
    b8 used;
};

struct Ela_Tree {
    static const s64 BRANCH_FACTOR = 4;
    
    struct Node {
        Node *children[BRANCH_FACTOR];
        Ela_Block entries[BRANCH_FACTOR - 1];
        u8 child_count;
        u8 entry_count;

        void reset() {
            this->child_count = 0;
            this->entry_count = 0;
        }

        s64 find_lower_bound(void *user_pointer) {
            s64 low = 0, high = this->entry_count - 1;

            while(low <= high) {
                s64 median = (low + high) / 2;
                if(this->entries[median].user_pointer == user_pointer) {
                    low = median;
                    break;
                } else if(this->entries[median].user_pointer < user_pointer) {
                    low = median + 1;
                } else {
                    high = median - 1;
                }
            }

            return low;
        }

        b8 is_leaf() {
            return this->child_count == 0;
        }

        b8 should_be_split_before_insert() {
            return this->entry_count + 1 == BRANCH_FACTOR; // We wouldn't be able to fit one more entry into this node
        }

        Node *split(Memory_Arena *arena, Node *parent_node, b8 parent_is_new_root, void *user_pointer) {
            u8 median = this->entry_count / 2;

            Node *right = (Node *) arena->push(sizeof(Node));
            right->reset();

            // If this isn't a leaf node, split the children between left and right now
            if(this->child_count) {
                assert(this->child_count == this->entry_count + 1);
                right->child_count = this->child_count - median - 1;
                memcpy(right->children, &this->children[median + 1], right->child_count * sizeof(Node * ));
                this->child_count = median + 1;
            }

            // Split the entries between left and right node
            right->entry_count = this->entry_count - median - 1;
            memcpy(right->entries, &this->entries[median + 1], right->entry_count * sizeof(Ela_Block));
            this->entry_count = median;

            // Insert the median into the parent. If the parent is a new root, also insert the node.
            s64 index_in_parent = parent_node->insert_entry(this->entries[median]);
            assert(index_in_parent >= 0 && index_in_parent + 1 < BRANCH_FACTOR);
            if(parent_is_new_root) parent_node->insert_child(index_in_parent + 0, this);
            parent_node->insert_child(index_in_parent + 1, right);

            return user_pointer > this->entries[median].user_pointer ? right : this;
        }

        Node *find_child(void *user_pointer) {
            s64 lower_bound = this->find_lower_bound(user_pointer);
            assert(lower_bound >= 0 && lower_bound < this->child_count);
            return this->children[lower_bound];
        }

        s64 insert_entry(Ela_Block block) {
            assert(this->entry_count + 1 <= BRANCH_FACTOR - 1);
            s64 lower_bound = this->find_lower_bound(block.user_pointer);
            assert(lower_bound >= 0 && lower_bound <= this->entry_count);
            memmove(&this->entries[lower_bound + 1], &this->entries[lower_bound], (this->entry_count - lower_bound) * sizeof(Ela_Block));
            this->entries[lower_bound] = block;
            ++this->entry_count;
            return lower_bound;
        }

        void insert_child(s64 index, Node *node) {
            assert(index >= 0 && index < BRANCH_FACTOR && this->child_count + 1 <= BRANCH_FACTOR);
            memmove(&this->children[index + 1], &this->children[index], (this->child_count - index) * sizeof(Node *));
            this->children[index] = node;
            ++this->child_count;
        }

        void debug_print(Debug_Printer *printer, s64 depth, s64 index_at_depth) {
            // @Incomplete: Maybe implement showing the parent somehow, by drawing flat ---- to that position?
    
            String_Builder builder;
            builder.create(Default_Allocator);
            builder.append_char('[');
            for(s64 i = 0; i < BRANCH_FACTOR - 1; ++i) {
                if(i < this->entry_count) {
                    builder.append_s64((s64) this->entries[i].user_pointer);
                } else {
                    builder.append_char('-');
                }
                
                if(i + 1 < BRANCH_FACTOR - 1) builder.append_string("|");
            }
            builder.append_char(']');
            
            printer->insert(BRANCH_FACTOR, depth, index_at_depth, builder.finish());
            
            for(u8 i = 0; i < this->child_count; ++i) {
                this->children[i]->debug_print(printer, depth + 1, index_at_depth * BRANCH_FACTOR + i);
            }
        }
    };

    Memory_Arena *arena;
    Node *root;

    void reset() {
        this->root = (Node *) this->arena->push(sizeof(Node));
        this->root->reset();
    }

    void add_block(void *user_pointer, u32 size_in_bytes, b8 used) {
        Ela_Block block = { user_pointer, size_in_bytes, used };
        
        Node *parent_node = null;
        Node *node = this->root;

        while(true) {
            if(node->should_be_split_before_insert()) {
                b8 parent_is_new_root = parent_node == null;

                if(parent_is_new_root) { // @Cleanup: Can we maybe make this prettier?
                    parent_node = (Node *) arena->push(sizeof(Node));
                    parent_node->reset();
                }

                node = node->split(this->arena, parent_node, parent_is_new_root, block.user_pointer);

                if(parent_is_new_root) {
                    this->root = parent_node;
                }
            }

            if(node->is_leaf()) {
                node->insert_entry(block);
                break;
            }

            parent_node = node;
            node = node->find_child(block.user_pointer);
        }
    }

    s64 calculate_depth() {
        s64 depth = 0;
        
        Node *node = this->root;
        while(node) {
            depth += 1;
            node = node->is_leaf() ? null : node->children[0];
        }

        return depth;
    }

    void debug_print(string file_path, b8 append) {
        Debug_Printer printer;
        printer.create(printer.calculate_line_at_depth(this->calculate_depth()) + 1, LOG_WIDTH);
        this->root->debug_print(&printer, 0, 0);
        printer.flush(file_path, append);
    }
};

static s64 COUNTER = 1;

struct Ela {
    Memory_Arena *arena;
    Ela_Tree tree;

    void create(Memory_Arena *arena) {
        this->arena = arena;
        this->tree.arena = arena;
        this->tree.reset();
    }

    void *acquire(u64 size_in_bytes) {
        assert(size_in_bytes < MAX_U32);

        //void *user_pointer = this->arena->push(size_in_bytes);
        void *user_pointer = (void *) COUNTER;
        
        this->tree.add_block(user_pointer, (u32) size_in_bytes, true);
        this->debug_print(LOG_FILE, true);

        ++COUNTER;

        return user_pointer;
    }

    void release(void *pointer) {
        // @Incomplete    
    }
    
    void *reacquire(void *old_pointer, u64 new_size) {
        // @Incomplete
        return null;
    }

    void destroy() {
        // @Incomplete    
    }
    
    u64 query_size(void *pointer) {
        // @Incomplete
        return 0;
    }

    void debug_print(string file_path, b8 append) {
        this->tree.debug_print(file_path, append);
    }

    Allocator allocator() {
        Allocator allocator = {
            this,
            [](void *data, u64 size)      -> void* { return ((Ela *) data)->acquire(size); },
            [](void *data, void *pointer) -> void  { return ((Ela *) data)->release(pointer); },
            [](void *data, void *old_pointer, u64 new_size) -> void * { return ((Ela *) data)->reacquire(old_pointer, new_size); },
            [](void *data) -> void { ((Ela *) data)->destroy(); },
            [](void *data, void *pointer) -> u64   { return ((Ela *) data)->query_size(pointer); }
        };

        return allocator;
    }
};



enum Action {
    ACTION_Allocation,
    ACTION_Deallocation,
};

enum Pattern {
    PATTERN_Only_Allocations,
    PATTERN_Build_Up_And_Destroy,
    PATTERN_Random,
};

string PATTERN_NAMES[] = { "Build Up and Destroy"_s, "Random"_s };

#define MIN_ALLOCATION_SIZE 16
#define MAX_ALLOCATION_SIZE 8129
#define RANDOM_ALLOCATION_SIZE() (rand() % (MAX_ALLOCATION_SIZE - MIN_ALLOCATION_SIZE) + MIN_ALLOCATION_SIZE)

static
void do_allocations(Allocator *allocator, void **allocations, s64 count, Pattern pattern, string name) {
    Hardware_Time start = os_get_hardware_time();
    
    s64 active_allocations = 0;

    for(s64 i = 0; i < count; ++i) {
        Action action = ACTION_Allocation;
        
        switch(action) {
        case ACTION_Allocation:
            allocations[active_allocations] = allocator->allocate(RANDOM_ALLOCATION_SIZE());
            break;
        }
    }

    Hardware_Time end = os_get_hardware_time();
    printf("%.*s for %.*s took %fms.\n", (u32) PATTERN_NAMES[pattern].count, PATTERN_NAMES[pattern].data, (u32) name.count, name.data, os_convert_hardware_time(end - start, Milliseconds));

    if(allocator->_reset_procedure) {
        allocator->reset();
    } else {
        for(s64 i = 0; i < active_allocations; ++i) {
            allocator->deallocate(allocations[active_allocations]);
        }
    }
}

int main() {
    os_delete_file(LOG_FILE);

    Memory_Arena underlying_arena;
    underlying_arena.create(8 * ONE_GIGABYTE);
    
    Memory_Pool underlying_pool;
    underlying_pool.create(&underlying_arena);

    Ela underlying_ela;
    underlying_ela.create(&underlying_arena);

    Allocator arena = underlying_arena.allocator();
    Allocator heap = heap_allocator;
    Allocator pool = underlying_pool.allocator();
    Allocator ela = underlying_ela.allocator();

    const s64 count = 1000000;
    const Pattern pattern = PATTERN_Only_Allocations;
    
    Resizable_Array<void *> allocations;
    allocations.reserve_exact(count);

    /*
    do_allocations(&arena, allocations.data, count, pattern, "Arena"_s);
    do_allocations(&heap, allocations.data, count, pattern, "Heap"_s);
    do_allocations(&pool, allocations.data, count, pattern, "Pool"_s);
    */
    do_allocations(&ela, allocations.data, count, pattern, "Ela"_s);

    return 0;
}