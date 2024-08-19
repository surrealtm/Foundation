CC     = clang++
CFLAGS = -Isrc/ -Isrc/Dependencies -DFOUNDATION_LINUX -DFOUNDATION_DEVELOPER -D_DEBUG -O0 -g -rdynamic -march=native -std=c++14 -lstdc++ -LDependencies -lm -lX11 -lfreetype # -rdynamic gives us symbol names for stack traces.
BIN    = x64/linux/

HEADER_FILES = src/art.h src/audio.h src/catalog.h src/concatenator.h src/data_array.h src/error.h src/file_watcher.h src/fileio.h src/font.h src/foundation.h src/hash_table.h src/jobs.h src/memutils.h src/noise.h src/os_specific.h src/package.h src/random.h src/socket.h src/software_renderer.h src/sort.h src/string_type.h src/synth.h src/text_input.h src/threads.h src/timing.h src/tweak_file.h src/ui.h src/window.h
SOURCE_FILES = src/audio.cpp src/concatenator.cpp src/error.cpp src/file_watcher.cpp src/fileio.cpp src/font.cpp src/foundation.cpp src/jobs.cpp src/linux_specific.cpp src/memutils.cpp src/noise.cpp src/package.cpp src/random.cpp src/single_header_libraries.cpp src/socket.cpp src/software_renderer.cpp src/string_type.cpp src/synth.cpp src/text_input.cpp src/threads.cpp src/timing.cpp src/tweak_file.cpp src/ui.cpp src/window.cpp

raytracer: $(HEADER_FILES) $(SOURCE_FILES)
	[ -d $(BIN) ] || mkdir -p $(BIN)
	$(CC) $(SOURCE_FILES) demos/raytracer.cpp $(CFLAGS) -O0 -o $(BIN)raytracer.out

clean:
	rm -f $(BIN)*.out $(BIN)*.o
