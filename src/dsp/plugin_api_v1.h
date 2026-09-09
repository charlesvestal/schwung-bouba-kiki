#ifndef MOVE_PLUGIN_API_V1_H
#define MOVE_PLUGIN_API_V1_H
#include <stdint.h>

#define MOVE_PLUGIN_API_VERSION_2 2
#define MOVE_PLUGIN_INIT_V2_SYMBOL "move_plugin_init_v2"

typedef struct host_api_v1 {
    uint32_t api_version;
    int sample_rate;
    int frames_per_block;
    uint8_t *mapped_memory;
    int audio_out_offset;
    int audio_in_offset;
    void (*log)(const char *msg);
    int (*midi_send_internal)(const uint8_t *, int);
    int (*midi_send_external)(const uint8_t *, int);
    int (*get_clock_status)(void);
    void *mod_emit_value;
    void *mod_clear_source;
    void *mod_host_ctx;
    float (*get_bpm)(void);
    int (*midi_inject_to_move)(const uint8_t *, int);
    int (*slot_recv_channel)(void *instance);
    double (*get_beat_position)(void);
    void *reserved[8];
} host_api_v1_t;

typedef struct plugin_api_v2 {
    uint32_t api_version;
    void *(*create_instance)(const char *, const char *);
    void (*destroy_instance)(void *);
    void (*on_midi)(void *, const uint8_t *, int, int);
    void (*set_param)(void *, const char *, const char *);
    int (*get_param)(void *, const char *, char *, int);
    int (*get_error)(void *, char *, int);
    void (*render_block)(void *, int16_t *, int);
} plugin_api_v2_t;

typedef plugin_api_v2_t *(*move_plugin_init_v2_fn)(const host_api_v1_t *);
#endif
