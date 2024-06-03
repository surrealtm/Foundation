#include "string_type.h"
#include "memutils.h"
#include "os_specific.h"

#if FOUNDATION_WIN32
# include <intrin.h> // For _addcarry_u64...
#elif FOUNDATION_LINUX
# include <x86intrin.h> // For _addcarry_u64...
#endif
#include <fenv.h> // For feclearexcept...
#include <string.h> // For strcmp



/* ------------------------------------------------ Characters ------------------------------------------------ */

b8 is_lower_character(u8 c) {
    return c >= 'a' && c <= 'z';
}

b8 is_upper_character(u8 c) {
    return !is_lower_character(c);
}

u8 to_lower_character(u8 c) {
    if (c >= 'A' && c <= 'Z') return (c - 'A') + 'a';
    return c;
}

u8 to_upper_character(u8 c) {
    if (c >= 'a' && c <= 'z') return (c - 'a') + 'A';
    return c;
}



/* ------------------------------------------------- C String ------------------------------------------------- */

s64 cstring_length(char *cstring) {
    s64 length = 0;
    while(*cstring) {
        ++length;
        ++cstring;
    }
    return length;
}

s64 cstring_length(const char *cstring) {
    s64 length = 0;
    while(*cstring) {
        ++length;
        ++cstring;
    }
    return length;
}

string from_cstring(Allocator *allocator, const char *cstring) {
    string _string;
    _string.count = cstring_length(cstring);
    _string.data  = (u8 *) allocator->allocate(_string.count);
    memcpy(_string.data, cstring, _string.count);
    return _string;
}

string cstring_view(const char *cstring) {
    string _string;
    _string.count = cstring_length(cstring);
    _string.data  = (u8 *) cstring;
    return _string;
}

char *to_cstring(Allocator *allocator, string _string) {
    char *cstring = (char *) allocator->allocate(_string.count + 1);
    memcpy(cstring, _string.data, _string.count);
    cstring[_string.count] = 0;
    return cstring;
}

void free_cstring(Allocator *allocator, char *cstring) {
    allocator->deallocate(cstring);
}

s64 search_cstring(const char *string, u8 _char) {
    for(s64 i = 0; *string != 0; ++string, ++i) {
        if(*string == _char) return i;
    }

    return -1;
}

s64 search_cstring_reverse(const char *string, u8 _char) {
    for(s64 i = strlen(string); *string != 0; ++string, --i) {
        if(*string == _char) return i;
    }

    return -1;
}

b8 cstrings_equal(const char *lhs, const char *rhs) {
    return strcmp(lhs, rhs) == 0;
}

b8 cstrings_equal(const char *lhs, const char *rhs, s64 length) {
    return strncmp(lhs, rhs, length) == 0;
}

b8 cstrings_equal_ignore_case(const char *lhs, const char *rhs) {
    for(;; ++lhs, ++rhs) {
        char d = to_lower_character((unsigned char) *lhs) - to_lower_character((unsigned char) *rhs);
        if(d != 0 || !*lhs) return false;
    }

    return true;
}

b8 cstrings_equal_ignore_case(const char *lhs, const char *rhs, s64 length) {
    for(s64 i = 0; i < length; ++lhs, ++rhs, ++i) {
        char d = to_lower_character((unsigned char) *lhs) - to_lower_character((unsigned char) *rhs);
        if(d != 0 || !*lhs) return false;
    }

    return true;
}

b8 cstring_starts_with(const char *lhs, const char *rhs) {
    return cstrings_equal(lhs, rhs, cstring_length(rhs));
}

b8 cstring_starts_with_ignore_case(const char *lhs, const char *rhs) {
    return cstrings_equal_ignore_case(lhs, rhs, strlen(rhs));
}

b8 cstring_ends_with(const char *lhs, const char *rhs) {
    s64 lhslen = cstring_length(lhs);
    s64 rhslen = cstring_length(rhs);
    if(rhslen > lhslen) return false;

    return cstrings_equal(&lhs[lhslen - rhslen], rhs, rhslen);
}

b8 cstring_ends_with_ignore_case(const char *lhs, const char *rhs) {
    s64 lhslen = cstring_length(lhs);
    s64 rhslen = cstring_length(rhs);
    if(rhslen > lhslen) return false;

    return cstrings_equal_ignore_case(&lhs[lhslen - rhslen], rhs, rhslen);
}

void copy_cstring(char *dst, const char *src) {
    strcpy(dst, src);
}

void copy_cstring(char *dst, const char *src, s64 src_count) {
    strncpy(dst, src, src_count);
}

void copy_cstring(char *dst, s64 dst_count, const char *src, s64 src_count) {
#if FOUNDATION_WIN32
    strncpy_s(dst, dst_count, src, src_count);
#else
    strncpy(dst, src, min(dst_count, src_count));
#endif
}




/* -------------------------------------------------- String -------------------------------------------------- */

string operator "" _s(const char *literal, size_t size) {
    return { static_cast<s64>(size), (u8 *) literal };
}

string strltr(char *literal) {
    string _string;
    _string.count = cstring_length(literal);
    _string.data  = (u8 *) literal;
    return _string;
}

string strltr(const char *literal) {
    string _string;
    _string.count = cstring_length(literal);
    _string.data  = (u8 *) literal;
    return _string;
}

string string_view(u8 *data, s64 count) {
    string _string;
    _string.count = count;
    _string.data  = data;
    return _string;
}

string make_string(Allocator *allocator, u8 *data, s64 count) {
    string _string;
    _string.count = count;
    _string.data  = (u8 *) allocator->allocate(count);
    memcpy(_string.data, data, _string.count);
    return _string;
}

string allocate_string(Allocator *allocator, s64 count) {
    string _string;
    _string.count = count;
    _string.data  = (u8 *) allocator->allocate(_string.count);
    return _string;
}

void deallocate_string(Allocator *allocator, string *_string) {
    if(_string->count) allocator->deallocate(_string->data);
    _string->count = 0;
    _string->data  = null;
}


string copy_string(Allocator *allocator, string input) {
    string output;
    output.count = input.count;
    output.data  = (u8 *) allocator->allocate(output.count);
    memcpy(output.data, input.data, input.count);
    return output;
}

string substring(Allocator *allocator, string input, s64 start, s64 end) {
    assert(start >= 0 && start < input.count && end >= 0 && end < input.count);
    string output = allocate_string(allocator, end - start);
    memcpy(output.data, input.data + start, output.count);
    return output;
}

string substring_view(string input, s64 start, s64 end) {
    assert(start >= 0 && start < input.count && end >= 0 && end <= input.count);
    string output;
    output.count = end - start;
    output.data  = input.data + start;
    return output;
}


string concatenate_strings(Allocator *allocator, string lhs, string rhs) {
    string output;
    output.count = lhs.count + rhs.count;
    output.data  = (u8 *) allocator->allocate(output.count);
    memcpy(&output.data[0], lhs.data, lhs.count);
    memcpy(&output.data[lhs.count], rhs.data, rhs.count);
    return output;
}


s64 search_string(string _string, u8 _char) {
    for(s64 i = 0; i < _string.count; ++i) {
        if(_string.data[i] == _char) return i;
    }

    return -1;
}

s64 search_string_reverse(string _string, u8 _char) {
    for(s64 i = _string.count - 1; i >= 0; --i) {
        if(_string.data[i] == _char) return i;
    }

    return -1;
}


b8 strings_equal(const string &lhs, const string &rhs) {
    if(lhs.count != rhs.count) return false;

    return memcmp(lhs.data, rhs.data, lhs.count) == 0;
}

b8 string_starts_with(string lhs, string rhs) {
    if(rhs.count > lhs.count) return false;

    return strings_equal(substring_view(lhs, 0, rhs.count), rhs);
}

b8 string_ends_with(string lhs, string rhs) {
    if(rhs.count > lhs.count) return false;

    return strings_equal(substring_view(lhs, lhs.count - rhs.count, lhs.count), rhs);
}


u64 string_hash(const string &input) {
    // fnv1a_64 hash
    u64 prime = 1099511628211;
    u64 offset = 14695981039346656037U;

    u64 hash = offset;

    for(s64 i = 0; i < input.count; ++i) {
        hash ^= input.data[i];
        hash *= prime;
    }

    return hash;
}

u64 string_hash(char const *input) {
    // fnv1a_64 hash
    u64 prime = 1099511628211;
    u64 offset = 14695981039346656037U;

    u64 hash = offset;

    while(*input) {
        hash ^= *input;
        hash *= prime;
        ++input;
    }

    return hash;
}



/* -------------------------------------------- String Conversion -------------------------------------------- */

s64 string_to_int(string input, b8 *success) {
    if(input.count == 0) {
        *success = false;
        return 0;
    }

    s64 sign = 1;
    s64 radix = 10;
    s64 number_start = 0;
    b8 valid = true;

    //
    // Parse a potential sign character.
    //
    if(input[number_start] == '-') {
        sign = -1;
        number_start += 1;
    } else if(input[number_start] == '+') {
        number_start += 1;
    }

    //
    // Parse a potential radix specifier.
    //
    if(input.count >= 2 && input[number_start] == '0' && (input[number_start + 1] == 'x' || input[number_start + 1] == 'X')) {
        radix = 16;
        number_start += 2;
    } else if(input.count >= 2 && input[number_start] == '0' && (input[number_start + 1] == 'b' || input[number_start + 1] == 'B')) {
        radix = 2;
        number_start += 2;
    }

    //
    // Ignore leading zeros.
    //
    while(number_start < input.count && input[number_start] == '0') ++number_start;

    //
    // Parse each digit and sum the result together.
    //
    char character;
    u64 character_decimal_value;
    u64 character_power = 1, result = 0;
    s64 character_index = 0;
    s64 character_count = input.count - number_start;

    while(valid && character_index < character_count) {
        character = input[input.count - character_index - 1];

        switch(radix) {
        case 2:
            if(character >= '0' && character <= '1') {
                character_decimal_value = character - '0';
            } else {
                valid = false;
            }
            break;

        case 10:
            if(character >= '0' && character <= '9') {
                character_decimal_value = character - '0';
            } else {
                valid = false;
            }
            break;

        case 16:
            if(character >= '0' && character <= '9') {
                character_decimal_value = character - '0';
            } else if(character >= 'a' && character <= 'f') {
                character_decimal_value = character - 'a' + 10;
            } else if(character >= 'A' && character <= 'F') {
                character_decimal_value = character - 'A' + 10;
            } else {
                valid = false;
            }
            break;
        }

        if(!valid) break;

        character_decimal_value = character_decimal_value * character_power;

        b8 addcarry_overflow = _addcarry_u64(0, result, character_decimal_value, &result);
        u64 charpower_overflow;
        character_power = _mulx_u64(character_power, radix, &charpower_overflow);
        ++character_index;

        if(addcarry_overflow || charpower_overflow) {
            valid = false;
            break;
        }
    }

    *success = valid;
    return result * sign;
}

f64 string_to_double(string input, b8 *success) {
    const f64 radix = 10;
    f64 sign = 1;
    b8 valid = true;

    s64 number_start = 0;

    //
    // Parse a potential sign character.
    //
    if(input[number_start] == '-') {
        sign = -1;
        number_start += 1;
    } else if(input[number_start] == '+') {
        number_start += 1;
    }

    //
    // Ignore leading zeros.
    //
    while(number_start < input.count && input[number_start] == '0') ++number_start;

    //
    // Find the dot seperating the whole from the fractional part.
    //
    s64 dot_index = search_string(input, '.');

    //
    // Parse the whole part back to front.
    //
    char character;
    s64 character_index = 0;
    f64 character_decimal_value, character_power = 1, result = 0;
    s64 whole_part_size = (dot_index != -1 ? dot_index : input.count);

    while(character_index + number_start < whole_part_size) {
        character = input[whole_part_size - character_index - 1];
        character_decimal_value = (f64) (character - '0') * character_power;

        feclearexcept(FE_ALL_EXCEPT);
        result += character_decimal_value;
        if(fetestexcept(FE_OVERFLOW)) valid = false;

        character_power = character_power * radix;
        ++character_index;
    }

    if(dot_index != -1) {
        //
        // Parse the fractional part front to back.
        //
        character_power = 0.1;
        character_index = dot_index + 1;

        while(character_index < input.count) {
            character = input[character_index];
            character_decimal_value = (f64) (character - '0') * character_power;

            feclearexcept(FE_ALL_EXCEPT);
            result += character_decimal_value;
            if(fetestexcept(FE_OVERFLOW)) valid = false;

            character_power = character_power / radix;
            ++character_index;
        }
    }

    *success = valid;
    return result * sign;
}

f32 string_to_float(string input, b8 *success) {
    f64 _f64 = string_to_double(input, success);
    return (f32) _f64;
}



/* ---------------------------------------------- String Builder ---------------------------------------------- */

u8 *String_Builder::grow(s64 count) {
    assert(count < BLOCK_SIZE);

    if(!this->current) {
        // This is the first time the string builder has been used. Set up the
        // first node without an allocation to provide a fast code path for small
        // strings.
        this->current  = &this->first;
        *this->current = { 0 };
    } else if(this->current->count + count > BLOCK_SIZE) {
        // The new data doesn't fit into the current block. We therefore need allocate
        // a new block, insert it into the linked list, and start using that.
        Block *block = (Block *) this->allocator->allocate(sizeof(Block));
        this->current->next = block;
        this->current       = block;
        *this->current      = { 0 };
    }

    u8 *pointer = &this->current->data[this->current->count];
    this->current->count += count;
    this->total_count    += count;

    return pointer;
}

u64 String_Builder::radix_value(Radix radix, u64 index) {
    u64 power = 1;

    switch(radix) {
    case RADIX_Binary:      power = 1ULL << index; break;
    case RADIX_Hexadecimal: power = 1ULL << (index * 4); break;
    case RADIX_Decimal:
        for(u64 i = 0; i < index; ++i) {
            power *= 10;
        }
        break;
    default: power = 1; break;
    }

    return power;
}

u64 String_Builder::number_of_required_digits(Radix radix, u64 value) {
    if(value == 0) return 1;

    u64 count = 0;

    while(value != 0) {
        value /= radix;
        ++count;
    }

    return count;
}

u64 String_Builder::number_of_required_digits(Radix radix, s64 value) {
    if(value == 0) return 1;

    u64 count = 0;

    while(value != 0) {
        value /= radix;
        ++count;
    }

    return count;
}

u64 String_Builder::number_of_required_digits(f64 value) {
    if(value == 0) return 1;

    u64 count = 0;

    while(value >= 1 || value <= -1) {
        value /= 10;
        ++count;
    }

    return count;
}

void String_Builder::append_string_builder_format(String_Builder_Format format) {
    if(format.prefix) {
        switch(format.radix) {
        case RADIX_Binary: this->append_string("0b"_s); break;
        case RADIX_Hexadecimal: this->append_string("0x"_s); break;
        default: break; // So that clang doesn't complain
        }
    }

    if(format.radix == RADIX_Binary || format.radix == RADIX_Hexadecimal || (format.radix == RADIX_Decimal && !format.sign)) {
        u64 value = format.value;
        u64 required_digits = this->number_of_required_digits(format.radix, value);

        //
        // Print out leading zeros.
        //
        for(u64 i = required_digits; i < format.digits; ++i) {
            this->append_char('0');
        }

        //
        // Print out the actual value.
        //
        s64 index = required_digits - 1;

        while(index >= 0) {
            u64 power = this->radix_value(format.radix, index);
            u64 digit = value / power;

            this->append_digit(digit);

            value -= digit * power;
            --index;
        }
    } else if(format.radix == RADIX_Decimal && format.sign) {
        u64 value = format.value;
        u64 required_digits = this->number_of_required_digits(format.radix, (s64) value);

        //
        // Handle sign.
        //
        if(value & 0x8000000000000000) { // value < 0
            value = (u64) (- (s64) value);
            this->append_char('-');
        } else if(format.prefix) {
            this->append_char('+');
        }

        //
        // Print out leading zeros.
        //
        for(u64 i = required_digits; i < format.digits; ++i) {
            this->append_char('0');
        }

        //
        // Print out the actual value.
        //
        s64 index = required_digits - 1;

        while(index >= 0) {
            u64 power = this->radix_value(format.radix, index);
            u64 digit = value / power;

            this->append_digit(digit);

            value -= digit * power;
            --index;
        }
    } else if(format.radix == RADIX_Floating_Point) {
        f64 value;
        memcpy(&value, &format.value, sizeof(f64));
        u64 required_digits = this->number_of_required_digits(value);

        //
        // Handle sign.
        //
        if(value < 0) {
            value = -value;
            this->append_char('-');
        } else if(format.prefix) {
            this->append_char('+');
        }

        //
        // Print out leading zeros.
        //
        for(u64 i = required_digits; i < format.digits; ++i) {
            this->append_char('0');
        }

        //
        // Print out the decimal spaces.
        //
        s64 index = required_digits - 1;

        while(index >= 0) {
            f64 power = pow(10, (f64) index);
            f64 digit = floor(value / power);

            this->append_digit((u64) digit);

            value -= digit * power;
            --index;
        }

        //
        // Print the decimal point.
        //

        this->append_char('.');

        //
        // Print the fractional spaces.
        //
        for(u64 i = 0; i < format.fractionals; ++i) {
            value *= 10;

            f64 digit = floor(value);

            this->append_digit((u64) digit);

            value -= digit;
        }
    }
}

void String_Builder::append_digit(u64 value) {
    if(value >= 0 && value <= 9) {
        this->append_char((char) (value + '0'));
    } else if(value >= 10 && value <= 15) {
        this->append_char((char) (value - 10 + 'a'));
    }
}

void String_Builder::create(Allocator *allocator) {
    this->allocator   = allocator;
    this->first       = { 0 };
    this->current     = null;
    this->total_count = 0;
}

void String_Builder::destroy() {
    Block *block = this->first.next;
    while(block) {
        Block *next_block = block->next;
        this->allocator->deallocate(block);
        block = next_block;
    }

    this->first       = { 0 };
    this->total_count = 0;
    this->current     = null;
}

void String_Builder::append_u8(u8 v) {
    this->append_string_builder_format({ RADIX_Decimal, (u64) v, 1, 0, false, false });
}

void String_Builder::append_u16(u16 v) {
    this->append_string_builder_format({ RADIX_Decimal, (u64) v, 1, 0, false, false });
}

void String_Builder::append_u32(u32 v) {
    this->append_string_builder_format({ RADIX_Decimal, (u64) v, 1, 0, false, false });
}

void String_Builder::append_u64(u64 v) {
    this->append_string_builder_format({ RADIX_Decimal, (u64) v, 1, 0, false, false });
}

void String_Builder::append_s8(s8 v) {
    this->append_string_builder_format({ RADIX_Decimal, (u64) (s64) v, 1, 0, true, false });
}

void String_Builder::append_s16(s16 v) {
    this->append_string_builder_format({ RADIX_Decimal, (u64) (s64) v, 1, 0, true, false });
}

void String_Builder::append_s32(s32 v) {
    this->append_string_builder_format({ RADIX_Decimal, (u64) (s64) v, 1, 0, true, false });
}

void String_Builder::append_s64(s64 v) {
    this->append_string_builder_format({ RADIX_Decimal, (u64) (s64) v, 1, 0, true, false });
}

void String_Builder::append_f32(f32 value) {
    this->append_f64((f64) value);
}

void String_Builder::append_f64(f64 value) {
    u64 _u64;
    memcpy(&_u64, &value, sizeof(f64));
    this->append_string_builder_format({ RADIX_Floating_Point, _u64, 1, 5, true, false });
}

void String_Builder::append_char(char c) {
    u8 *pointer = this->grow(1);
    *pointer = c;
}

void String_Builder::append_string(const char *s) {
    s64 length = cstring_length(s);

    while(length > 0) {
        s64 batch = min(BLOCK_SIZE, length);
        u8 *pointer = this->grow(batch);
        memcpy(pointer, s, batch);
        length -= batch;
        s += batch;
    }
}

void String_Builder::append_string(char *s) {
    s64 length = cstring_length(s);

    while(length > 0) {
        s64 batch = min(BLOCK_SIZE, length);
        u8 *pointer = this->grow(batch);
        memcpy(pointer, s, batch);
        length -= batch;
        s += batch;
    }
}

void String_Builder::append_string(string s) {
    while(s.count > 0) {
        s64 batch = min(BLOCK_SIZE, s.count);
        u8 *pointer = this->grow(batch);
        memcpy(pointer, s.data, batch);
        s.count -= batch;
        s.data += batch;
    }
}

string String_Builder::finish() {
    if(this->first.next) {
        // There are at least two different blocks of data which we need to concatenate here.
        string result = allocate_string(this->allocator, this->total_count);
        s64 offset = 0;

        Block *block = &this->first;
        while(block) {
            memcpy(&result.data[offset], block->data, block->count);
            offset += block->count;

            Block *next_block = block->next;
            if(block != &this->first) this->allocator->deallocate(block);
            block = next_block;
        }

        this->first = { 0 };
        this->current = null;
        this->total_count = 0;

        return result;
    } else {
        // There was only ever one block of data, meaning this block is already our complete
        // string. That block however was allocated on the stack, so we need to move it onto
        // the supplied allocator.
        return copy_string(this->allocator, string_view(this->first.data, this->first.count));
    }
}
