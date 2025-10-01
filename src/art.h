#pragma once

#include "foundation.h"

enum Art_Node_Kind {
    ART_Leaf,
    ART_Node4,
    ART_Node16,
    ART_Node48,
    ART_Node256,
};
    
struct Art_Node {
    Art_Node_Kind kind;
};
    
struct Art_Leaf : Art_Node {
    Art_Leaf() { kind = ART_Leaf; };
};

struct Art_Node4 : Art_Node {        
    Art_Node4() { kind = ART_Node4; count = 0; };

    u8 count;
    u8 keys[4];
    Art_Node *children[4];
};

struct Art_Node16 : Art_Node {
    Art_Node16() { kind = ART_Node16; count = 0; };

    u8 count;
    u8 keys[16];
    Art_Node *children[16];
};

struct Art_Node48 : Art_Node {
    Art_Node48() { kind = ART_Node48; count = 0; };

    u8 count;
    u8 indirection[256];
    Art_Node *children[48];
};

struct Art_Node256 : Art_Node {
    Art_Node256() { kind = ART_Node256; };

    Art_Node *children[256];
};


template<typename T>
struct Adaptive_Radix_Tree {   
    Allocator *allocator = Default_Allocator;
    Art_Node *root = null;

    void create();
    void destroy();
    b8 add(T const &key); // Returns false if the key already exists or if the key is a prefix to an existing key
    b8 query(T const &key);

    Art_Node *destroy_recursive(Art_Node *node);
    u8 span(T const &key, u8 span_index);
    u8 span_count();

    template<typename Node>
    Node *allocate_node() {
        Node *pointer = (Node *) this->allocator->allocate(sizeof(Node));
        *pointer = Node();
        return pointer;
    };
};

// Because C++ is a terrible language, we need to supply the template definitions in the header file for
// instantiation to work correctly... This feels horrible but still better than just inlining the code I guess.
#include "art.inl"
