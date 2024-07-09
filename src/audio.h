#pragma once

#include "foundation.h"
#include "string_type.h"
#include "error.h"
#include "memutils.h"

// @Incomplete: Support streaming in large audio buffers from disk.
// @Cleanup: Handle switching audio engines during play, e.g. when connecting headphones on laptop. Might only
// happen if connecting them while muted, and then unmuting?

#define AUDIO_PLATFORM_STATE_SIZE 32

#ifndef AUDIO_SAMPLE_RATE
# define AUDIO_SAMPLE_RATE        48000
#endif

#ifndef AUDIO_CHANNELS
# define AUDIO_CHANNELS               2
#endif

#ifndef AUDIO_UPDATES_PER_SECOND
# define AUDIO_UPDATES_PER_SECOND    20
# define AUDIO_SAMPLES_PER_UPDATE (AUDIO_SAMPLE_RATE / AUDIO_UPDATES_PER_SECOND)
#endif

enum Audio_Volume_Type {
    AUDIO_VOLUME_Master,
    AUDIO_VOLUME_Interface,
    AUDIO_VOLUME_Music,
    AUDIO_VOLUME_Sound_Effect,
    AUDIO_VOLUME_Ambient,
    AUDIO_VOLUME_COUNT,
};

enum Audio_Source_State {
    AUDIO_SOURCE_Paused    = 0x0,
    AUDIO_SOURCE_Playing   = 0x1,
    AUDIO_SOURCE_Completed = 0x2,
};

enum Audio_Buffer_Format {
    AUDIO_BUFFER_FORMAT_Unknown,
    AUDIO_BUFFER_FORMAT_Pcm16,
    AUDIO_BUFFER_FORMAT_Float32,
};

struct Audio_Arc {
    s8 left_channel, right_channel;
    f32 left_angle, right_angle;
    f32 covered_angle;
};

struct Audio_Listener {
    f32 x, y, z;    // World position of the listener
    f32 fx, fy, fz; // Forward vector of the listener
    f32 ux, uy, uz; // Up vector of the listener
    f32 rx, ry, rz; // Right vector of the listener
    
    Audio_Arc arcs[AUDIO_CHANNELS];
};

struct Audio_Buffer {
    string name; // Usually the file path, but can be set manually.

    u8 channels;
    u32 sample_rate;
    Audio_Buffer_Format format;

    u8 *data;
    u64 size_in_bytes;
    
    u64 frame_count; // Valid number of frames in the data buffer

    b8 __drflac_cleanup; // Depending on how an audio buffer was loaded, we may require different cleanups.
};

struct Audio_Source {
    Audio_Source_State state;
    Audio_Volume_Type volume_type;
    b8 remove_on_completion; // Externally managed audio sources will not be automatically destroyed by the audio system.

    Audio_Buffer *playing_buffer;
    u64 frame_offset_in_buffer;

    f32 volume;
    b8 loop;
    b8 spatialized;

    f32 x, y, z;
    f32 falloff_start; // Spatialized sound is played at full volume until the min radius, and then blent to zero using 1/distance
};

typedef f32 *(*Audio_Stream_Callback)(void *user_pointer, u64 requested_frames);

struct Audio_Stream {
    void *user_pointer;
    Audio_Stream_Callback callback;
    Audio_Buffer buffer;
    Audio_Source *source;
    u64 frames_played_last_update;
};

struct Audio_Mixer {
    u8 platform_data[AUDIO_PLATFORM_STATE_SIZE];

    Allocator *allocator;

    Audio_Listener listener;
    f32 volumes[AUDIO_VOLUME_COUNT];
    
    Linked_List<Audio_Source> sources;
    Linked_List<Audio_Stream> streams;    
};

Error_Code create_audio_mixer(Audio_Mixer *mixer);
void destroy_audio_mixer(Audio_Mixer *mixer);
void update_audio_mixer(Audio_Mixer *mixer);

// This can be useful to "catch up" the audio system after a long time without any updates,
// for example after loading something from disk or program startup. In that case, the audio
// mixer may have to "catch up" a large number of audio frames, which may not be available
// at the time for seamless play (e.g. when streaming into an audio buffer which is not large
// enough for the time gap). Instead of playing the too-small available part of the audio
// stream, skip this update entirely by just playing silence, and then resume normal playback.
// This avoids bad audio artifacts at the cost of a little latency.
void update_audio_mixer_with_silence(Audio_Mixer *mixer);

void update_audio_listener(Audio_Mixer *mixer, f32 x, f32 y, f32 z, f32 fx, f32 fy, f32 fz, f32 ux, f32 uy, f32 uz, f32 rx, f32 ry, f32 rz);

Error_Code create_audio_buffer_from_wav_memory(Audio_Buffer *buffer, string file_content, string buffer_name, Allocator *allocator);
Error_Code create_audio_buffer_from_wav_file(Audio_Buffer *buffer, string file_path, Allocator *allocator);
Error_Code create_audio_buffer_from_flac_memory(Audio_Buffer *buffer, string file_content, string buffer_name, Allocator *allocator);
Error_Code create_audio_buffer_from_flac_file(Audio_Buffer *buffer, string file_path, Allocator *allocator);
void create_audio_buffer(Audio_Buffer *buffer, Audio_Buffer_Format format, u8 channels, u32 sample_rate, u64 frame_count, string buffer_name, Allocator *allocator);
void destroy_audio_buffer(Audio_Buffer *buffer, Allocator *allocator);

Audio_Source *acquire_audio_source(Audio_Mixer *mixer, Audio_Volume_Type type, b8 spatialized);
void release_audio_source(Audio_Mixer *mixer, Audio_Source *source);
void stop_audio_source(Audio_Source *source);
void pause_audio_source(Audio_Source *source);
void resume_audio_source(Audio_Source *source);
void set_audio_source_looping(Audio_Source *source, b8 looping);
void set_audio_source_transformation(Audio_Source *source, f32 x, f32 y, f32 z, f32 falloff_start);
void play_audio_buffer(Audio_Mixer *mixer, Audio_Buffer *buffer, Audio_Volume_Type type, b8 spatialized);
void play_audio_buffer(Audio_Source *source, Audio_Buffer *buffer);

Audio_Stream *create_audio_stream(Audio_Mixer *mixer, void *user_pointer, Audio_Stream_Callback callback, Audio_Volume_Type type, b8 spatialized, string buffer_name, Allocator *allocator);
void destroy_audio_stream(Audio_Mixer *mixer, Audio_Stream *stream, Allocator *allocator);
void pause_audio_stream(Audio_Stream *stream);
void resume_audio_stream(Audio_Stream *stream);
