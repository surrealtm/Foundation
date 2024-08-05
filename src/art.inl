#include "os_specific.h"
#include <intrin.h>

static inline
u8 art_flip_sign(u8 span) {
    return span ^ 0x80;
}

static inline
u8 art_node4_lower_bound(Art_Node4 *node4, u8 span) {
    u8 child_index;

    for(child_index = 0; child_index < node4->count;) {
        if(span <= node4->keys[child_index]) break;
        ++child_index;
    }

    return child_index;
}

static inline
u8 art_node16_lookup_lower_bound(Art_Node16 *node16, u8 span) {
    __m128i lhs, rhs, result;
    lhs    = _mm_set1_epi8(span);
    rhs    = _mm_loadu_si128((__m128i *) node16->keys);
    result = _mm_cmpeq_epi8(lhs, rhs);

    int bitmask = _mm_movemask_epi8(result) & (0xffff >> (16 - node16->count));
    return bitmask != 0 ? (u8) os_highest_bit_set(bitmask) : node16->count;
}

static inline
u8 art_node16_insert_lower_bound(Art_Node16 *node16, u8 span) {
    __m128i lhs, rhs, result;
    lhs    = _mm_set1_epi8(span);
    rhs    = _mm_loadu_si128((__m128i *) node16->keys);
    result = _mm_cmplt_epi8(lhs, rhs);

    int bitmask = _mm_movemask_epi8(result) & (0xffff >> (16 - node16->count));
    return bitmask != 0 ? (u8) os_highest_bit_set(bitmask) : node16->count;
}

template<typename T>
void Adaptive_Radix_Tree<T>::create() {
    this->root = this->allocator->New<Art_Leaf>();
}

template<typename T>
void Adaptive_Radix_Tree<T>::destroy() {
    this->root = this->destroy_recursive(this->root);
}

template<typename T>
b8 Adaptive_Radix_Tree<T>::add(T const &key) {
    Art_Node **node_ptr = &this->root;
    Art_Node *node;
    u8 span_index = 0;
    u8 span_count = this->span_count(key);
    
    while(span_index < span_count && (node = *node_ptr) != null) {
        u8 span = this->span(key, span_index);
        
        switch(node->kind) {
        case ART_Leaf: {
            // Replace this leaf node with a node4 and then continue inserting into that new node.
            Art_Leaf *leaf = (Art_Leaf *) node;
            Art_Node4 *node4 = this->allocator->New<Art_Node4>();
            this->allocator->deallocate(leaf);
            *node_ptr = node4;
        } break;
            
        case ART_Node4: {
            Art_Node4 *node4 = (Art_Node4 *) node;

            // Find the child index that is the smallest element not smaller than the span
            u8 child_index = art_node4_lower_bound(node4, span);
            
            // If the child already exists for exactly this span, then just continue inserting into there.
            if(child_index < node4->count && node4->keys[child_index] == span) {
                node_ptr = &node4->children[child_index];
                ++span_index;
                break;
            }

            // If the node has no more capacity, resize it to a node16 and continue inserting into that new node.
            if(node4->count == 4) {
                Art_Node16 *node16 = this->allocator->New<Art_Node16>();
                node16->count = node4->count;
                for(u8 i = 0; i < node4->count; ++i) {
                    node16->keys[i]     = art_flip_sign(node4->keys[i]);
                    node16->children[i] = node4->children[i];
                }

                this->allocator->deallocate(node4);
                *node_ptr = node16;
                break;
            }           
            
            // Insert into the sorted node4
            assert(child_index >= 0 && child_index <= 3);
            memmove(&node4->keys[child_index + 1], &node4->keys[child_index], (4 - child_index - 1) * sizeof(u8));
            memmove(&node4->children[child_index + 1], &node4->children[child_index], (4 - child_index - 1) * sizeof(Art_Node *));
            node4->keys[child_index] = span;
            node4->children[child_index] = this->allocator->New<Art_Leaf>(); // @@Speed: If this isn't the last span, already create a Node4 because that will happen later anyway.
            node_ptr = &node4->children[child_index];
            ++node4->count;
            ++span_index;
        } break;

        case ART_Node16: {
            Art_Node16 *node16 = (Art_Node16 *) node;

            span = art_flip_sign(span); // For the SIMD lower_bound implementation to work correctly, we must assume signed (instead of unsigned) integers in this node, therefore flip the signs...
            
            // Find the child index that is the smallest element not smaller than the span
            u8 child_index = art_node16_insert_lower_bound(node16, span);

            // If the child already exists for exactly this span, then just continue inserting into there.
            if(child_index < node16->count && node16->keys[child_index] == span) {
                node_ptr = &node16->children[child_index];
                ++span_index;
                break;
            }

            // If the node has no more capacity, resize it to a node48 and continue inserting into that new node.
            if(node16->count == 16) {
                Art_Node48 *node48 = this->allocator->New<Art_Node48>();
                memset(node48->indirection, 0xff, sizeof(node48->indirection));
                node48->count = node16->count;
                for(u8 i = 0; i < node16->count; ++i) {
                    node48->indirection[art_flip_sign(node16->keys[i])] = i;
                    node48->children[i] = node16->children[i];
                }
                
                this->allocator->deallocate(node16);
                *node_ptr = node48;
                break;
            }

            // Insert into the sorted node16
            assert(child_index >= 0 && child_index <= 15);
            memmove(&node16->keys[child_index + 1], &node16->keys[child_index], (16 - child_index - 1) * sizeof(u8));
            memmove(&node16->children[child_index + 1], &node16->children[child_index], (16 - child_index - 1) * sizeof(Art_Node *));
            node16->keys[child_index] = span;
            node16->children[child_index] = this->allocator->New<Art_Leaf>(); // @@Speed: If this isn't the last span, already create a Node4 because that will happen later anyway.
            node_ptr = &node16->children[child_index];
            ++node16->count;
            ++span_index;
        } break;
            
        case ART_Node48: {
            Art_Node48 *node48 = (Art_Node48 *) node;

            // If the child already exists for exactly this span, then just continue inserting into there.
            if(node48->indirection[span] != 0xff) {
                node_ptr = &node48->children[node48->indirection[span]];
                ++span_index;
                break;
            }

            // If the node has no more capacity, resize it to a node256 and continue inserting into that new node.
            if(node48->count == 48) {
                Art_Node256 *node256 = this->allocator->New<Art_Node256>();
                for(u16 i = 0; i < 256; ++i) {
                    if(node48->indirection[i] != 0xff)
                        node256->children[i] = node48->children[node48->indirection[i]];
                    else
                        node256->children[i] = null;
                }

                this->allocator->deallocate(node48);
                *node_ptr = node256;
                break;
            }

            // Insert into the node48
            node48->indirection[span] = node48->count;
            node48->children[node48->count] = this->allocator->New<Art_Leaf>(); // @@Speed: If this isn't the last span, already create a Node4 because that will happen later anyway.
            node_ptr = &node48->children[node48->count];
            ++node48->count;
            ++span_index;
        } break;
            
        case ART_Node256: {
            Art_Node256 *node256 = (Art_Node256 *) node;
            if(node256->children[span] == null) {
                node256->children[span] = this->allocator->New<Art_Leaf>(); // @@Speed: If this isn't the last span, already create a Node4 because that will happen later anyway.
            }

            node_ptr = &node256->children[span];
            ++span_index;
        } break;
        }
    }

    return false;
}

template<typename T>
b8 Adaptive_Radix_Tree<T>::query(T const &key) {
    Art_Node *node = this->root;
    u8 span_index = 0;
    u8 span_count = this->span_count(key);
    
    while(span_index < span_count && node != null) {
        u8 span = this->span(key, span_index);

        switch(node->kind) {
        case ART_Leaf:
            node = null;
            break;

        case ART_Node4: {
            Art_Node4 *node4 = (Art_Node4 *) node;
            u8 child_index = art_node4_lower_bound(node4, span);
            if(child_index < node4->count && node4->keys[child_index] == span) {
                node = node4->children[child_index];
                ++span_index;
            } else {
                node = null;
            }
        } break;

        case ART_Node16: {
            Art_Node16 *node16 = (Art_Node16 *) node;
            span = art_flip_sign(span);
            u8 child_index = art_node16_lookup_lower_bound(node16, span);
            if(child_index < node16->count && node16->keys[child_index] == span) {
                node = node16->children[child_index];
                ++span_index;
            } else {
                node = null;
            }
        } break;

        case ART_Node48: {
            Art_Node48 *node48 = (Art_Node48 *) node;
            if(node48->indirection[span] != 0xff) {
                node = node48->children[node48->indirection[span]];
                ++span_index;
            } else {
                node = null;
            }
        } break;

        case ART_Node256: {
            Art_Node256 *node256 = (Art_Node256 *) node;
            node = node256->children[span];
            ++span_index;
        } break;
        }
    }

    return node != null;
}
    
template<typename T>
Art_Node *Adaptive_Radix_Tree<T>::destroy_recursive(Art_Node *node) {
    if(!node) return null;

    switch(node->kind) {
    case ART_Node4: {
        Art_Node4 *node4 = (Art_Node4 *) node;
        for(u8 i = 0; i < node4->count; ++i) {
            node4->children[i] = this->destroy_recursive(node4->children[i]);
        }
    } break;

    case ART_Node16: {
        Art_Node16 *node16 = (Art_Node16 *) node;
        for(u8 i = 0; i < node16->count; ++i) {
            node16->children[i] = this->destroy_recursive(node16->children[i]);
        }
    } break;

    case ART_Node48: {
        Art_Node48 *node48 = (Art_Node48 *) node;
        for(u8 i = 0; i < node48->count; ++i) {
            node48->children[i] = this->destroy_recursive(node48->children[i]);
        }
    } break;

    case ART_Node256: {
        Art_Node256 *node256 = (Art_Node256 *) node;
        for(u16 i = 0; i < 256; ++i) {
            node256->children[i] = this->destroy_recursive(node256->children[i]);
        }
    } break;
    }

    this->allocator->deallocate(node);
    return null;
}

template<typename T>
u8 Adaptive_Radix_Tree<T>::span(T const &key, u8 span_index) {
    return ((u8 *) &key)[span_index];
}

template<typename T>
u8 Adaptive_Radix_Tree<T>::span_count(T const &key) {
    return sizeof(T) / sizeof(u8);
}
