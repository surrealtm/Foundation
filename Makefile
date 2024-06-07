CC     = clang++
CFLAGS = -Isrc/ -Isrc/Dependencies -DFOUNDATION_LINUX -march=native -lm -LDependencies -lfreetype
BIN    = x64/linux/

HEADER_FILES = src/concatenator.h src/data_array.h src/error.h src/fileio.h src/font.h src/foundation.h src/hash_table.h src/jobs.h src/memutils.h src/os_specific.h src/random.h src/socket.h src/sort.h src/string_type.h src/text_input.h src/threads.h src/timing.h src/window.h
SOURCE_FILES = src/demo.cpp src/concatenator.cpp src/error.cpp src/fileio.cpp src/font.cpp src/foundation.cpp src/jobs.cpp src/linux_specific.cpp src/memutils.cpp src/random.cpp src/socket.cpp src/string_type.cpp src/text_input.cpp src/threads.cpp src/timing.cpp src/window.cpp

# string.h on linux includes strings.h which then clashes with our own strings.h header... I fucking hate
# my life.

debug: $(HEADER_FILES) $(SOURCE_FILES)
	[ -d $(BIN) ] || mkdir -p $(BIN)
	$(CC) $(SOURCE_FILES) $(CFLAGS) -O0 -o $(BIN)demo.out

release: $(HEADER_FILES) $(SOURCE_FILES)
	[ -d $(BIN) ] || mkdir -p $(BIN)
	$(CC) $(SOURCE_FILES) $(CFLAGS) -O3 -o $(BIN)demo.out

clean:
	rm -f $(BIN)*.out $(BIN)*.o
