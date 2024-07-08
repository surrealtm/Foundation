//
// This file's sole purpose is supplying an easy way to integrate STB libraries into the code base, as they require
// a very small amount of code to be set up. Users of this code base may however not want to use STB at all, so
// it is an additional file.
//

#include "memutils.h"

#define STBI_MALLOC(size) Default_Allocator->allocate(size)
#define STBI_REALLOC(ptr, new_size) Default_Allocator->reallocate(ptr, new_size)
#define STBI_FREE(ptr) Default_Allocator->deallocate(ptr)
#define STB_IMAGE_IMPLEMENTATION
#include "Dependencies/stb_image.h"

#define DRFLAC_MALLOC(size) Default_Allocator->allocate(size)
#define DRFLAC_REALLOC(ptr, new_size) Default_Allocator->reallocate(ptr, new_size)
#define DRFLAC_FREE(ptr) Default_Allocator->deallocate(ptr)
#define DRFLAC_IMPLEMENTATION
#include "Dependencies/drflac.h"
