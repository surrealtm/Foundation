#include "foundation.h"
#include "os_specific.h"
#include "fileio.h"

int main() {
    {
        Binary_Writer writer;
        writer.create("data/test.txt"_s, 1024);
        writer.write_s64(5);
        writer.write_u8(10);
        writer.write_string("Hello World, how are you?"_s);
        writer.destroy();
    }

    {
        Binary_Parser parser;
        parser.create_from_file("data/test.txt"_s);
        s64 _s64 = parser.read_s64();
        u8 _u8 = parser.read_u8();
        string _string = parser.read_string();
        parser.destroy_file_data();

        printf("Data: %lld, %u, %.*s\n", _s64, _u8, _string.count, _string.data);
    }
    return 0;
}
