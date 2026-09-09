#include "plugin_api_v1.h"
#include "synth.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct bk_instance {
    bk_synth_t synth;
    float values[8];
    float pressure;
} bk_instance_t;

static const char *const KEYS[8] = {
    "morph", "bulge", "pinch", "spikes", "tilt", "wobble", "attack", "release"
};
static const float DEFAULTS[8] = {0.25f,0.35f,0.0f,0.25f,0.5f,0.1f,0.05f,0.25f};

static const char CHAIN_PARAMS[] =
"[{\"key\":\"morph\",\"name\":\"Morph\",\"type\":\"float\",\"min\":0,\"max\":1},"
"{\"key\":\"bulge\",\"name\":\"Bulge\",\"type\":\"float\",\"min\":0,\"max\":1},"
"{\"key\":\"pinch\",\"name\":\"Pinch\",\"type\":\"float\",\"min\":0,\"max\":1},"
"{\"key\":\"spikes\",\"name\":\"Spikes\",\"type\":\"float\",\"min\":0,\"max\":1},"
"{\"key\":\"tilt\",\"name\":\"Tilt\",\"type\":\"float\",\"min\":0,\"max\":1},"
"{\"key\":\"wobble\",\"name\":\"Wobble\",\"type\":\"float\",\"min\":0,\"max\":1},"
"{\"key\":\"attack\",\"name\":\"Attack\",\"type\":\"float\",\"min\":0,\"max\":1},"
"{\"key\":\"release\",\"name\":\"Release\",\"type\":\"float\",\"min\":0,\"max\":1},"
"{\"key\":\"shape\",\"name\":\"Shape\",\"type\":\"canvas\",\"canvas_script\":\"canvas.js\",\"as_page\":true}]";
static const char UI_HIERARCHY[] =
"{\"pad_layout\":\"chromatic\",\"levels\":{\"root\":{\"label\":\"Bouba-Kiki\","
"\"knobs\":[\"morph\",\"bulge\",\"pinch\",\"spikes\",\"tilt\",\"wobble\",\"attack\",\"release\"],"
"\"params\":[{\"key\":\"shape\"},{\"key\":\"morph\"},{\"key\":\"bulge\"},{\"key\":\"pinch\"},"
"{\"key\":\"spikes\"},{\"key\":\"tilt\"},{\"key\":\"wobble\"},{\"key\":\"attack\"},{\"key\":\"release\"}]}}}";

static int key_index(const char *key) {
    for (int i = 0; i < 8; ++i) if (strcmp(key, KEYS[i]) == 0) return i;
    return -1;
}
static float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
static int copy_string(char *buf, int len, const char *s) {
    const size_t n = strlen(s);
    if (!buf || len <= 0 || n >= (size_t)len) return -1;
    memcpy(buf, s, n + 1); return (int)n;
}
static void apply_values(bk_instance_t *in) {
    bk_shape_params_t p = {in->values[0],in->values[1],in->values[2],in->values[3],in->values[4],in->values[5]};
    bk_synth_set_shape(&in->synth, &p);
    bk_synth_set_attack_release(&in->synth, in->values[6], in->values[7]);
}
static void set_one(bk_instance_t *in, int i, const char *val) {
    if (!val) return;
    char *end = NULL; errno = 0; float x = strtof(val, &end);
    if (errno || end == val || *end != '\0' || !isfinite(x)) return;
    in->values[i] = clamp01(x); apply_values(in);
}
static void set_state(bk_instance_t *in, const char *json) {
    if (!json) return;
    for (int i = 0; i < 8; ++i) {
        char needle[32]; snprintf(needle, sizeof(needle), "\"%s\":", KEYS[i]);
        const char *p = strstr(json, needle);
        if (p) {
            p += strlen(needle);
            char *end = NULL; errno = 0; float x = strtof(p, &end);
            if (!errno && end != p && (*end == ',' || *end == '}'))
                in->values[i] = clamp01(x);
        }
    }
    apply_values(in);
}
static void *create_instance(const char *dir, const char *defaults) {
    (void)dir;
    bk_instance_t *in = (bk_instance_t *)calloc(1, sizeof(*in));
    if (!in) return NULL;
    memcpy(in->values, DEFAULTS, sizeof(DEFAULTS));
    bk_synth_init(&in->synth, 44100.0f); apply_values(in);
    if (defaults) set_state(in, defaults);
    return in;
}
static void destroy_instance(void *ptr) { free(ptr); }
static void on_midi(void *ptr, const uint8_t *msg, int len, int source) {
    (void)source; if (!ptr || !msg || len < 3) return;
    bk_instance_t *in = ptr; const int status = msg[0] & 0xf0;
    if (status == 0x90 && msg[2]) bk_synth_note_on(&in->synth, msg[1], msg[2]);
    else if (status == 0x80 || (status == 0x90 && !msg[2])) bk_synth_note_off(&in->synth, msg[1]);
    else if (status == 0xa0) { bk_synth_pressure(&in->synth, msg[1], msg[2]); in->pressure = msg[2] / 127.0f; }
    else if (status == 0xb0 && (msg[1] == 120 || msg[1] == 123)) bk_synth_all_notes_off(&in->synth);
}
static void set_param(void *ptr, const char *key, const char *val) {
    if (!ptr || !key) return;
    bk_instance_t *in = ptr;
    int i = key_index(key); if (i >= 0) set_one(in, i, val);
    else if (strcmp(key, "state") == 0) set_state(in, val);
}
static int get_param(void *ptr, const char *key, char *buf, int len) {
    if (!ptr || !key) return -1;
    bk_instance_t *in = ptr;
    if (strcmp(key, "chain_params") == 0) return copy_string(buf, len, CHAIN_PARAMS);
    if (strcmp(key, "ui_hierarchy") == 0) return copy_string(buf, len, UI_HIERARCHY);
    if (strcmp(key, "pressure") == 0) { char t[32]; snprintf(t,sizeof(t),"%.6g",in->pressure); return copy_string(buf,len,t); }
    int i = key_index(key);
    if (i >= 0) { char t[32]; snprintf(t,sizeof(t),"%.6g",in->values[i]); return copy_string(buf,len,t); }
    if (strcmp(key, "state") == 0) {
        char t[512]; int n = snprintf(t,sizeof(t),"{\"morph\":%.6g,\"bulge\":%.6g,\"pinch\":%.6g,\"spikes\":%.6g,\"tilt\":%.6g,\"wobble\":%.6g,\"attack\":%.6g,\"release\":%.6g}",in->values[0],in->values[1],in->values[2],in->values[3],in->values[4],in->values[5],in->values[6],in->values[7]);
        if (n < 0 || n >= (int)sizeof(t)) return -1;
        return copy_string(buf,len,t);
    }
    return -1;
}
static int get_error(void *ptr, char *buf, int len) { (void)ptr; if (buf && len) buf[0]='\0'; return 0; }
static void render_block(void *ptr, int16_t *out, int frames) {
    if (!ptr || !out || frames <= 0) return;
    bk_instance_t *in = ptr;
    float temp[256];
    while (frames > 0) {
        int n = frames > 128 ? 128 : frames; bk_synth_render(&in->synth,temp,n);
        for (int i=0;i<n*2;++i) { float x=temp[i]*32767.0f; if(x>32767)x=32767;if(x<-32768)x=-32768;out[i]=(int16_t)x; }
        out += n*2; frames -= n;
    }
}
static plugin_api_v2_t API = {2,create_instance,destroy_instance,on_midi,set_param,get_param,get_error,render_block};
__attribute__((visibility("default"))) plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host) { (void)host; return &API; }
