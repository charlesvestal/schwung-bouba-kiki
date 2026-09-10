#include "contract.h"
#include "plugin_api_v1.h"
#include "synth.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct bk_instance {
  bk_synth_t synth;
  float values[BK_PARAM_COUNT], pressure, pulse, note_velocity;
  unsigned identity, note_serial;
  int preset;
} bk_instance_t;
#define BK_INSTANCE_CAPACITY 16
static bk_instance_t INSTANCE_POOL[BK_INSTANCE_CAPACITY];
static atomic_int INSTANCE_USED[BK_INSTANCE_CAPACITY];
static atomic_uint NEXT_IDENTITY;
_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "Instance claims must be lock-free");
static int copy_string(char *buf, int len, const char *s) {
  size_t n = strlen(s);
  if (!buf || len <= 0 || n >= (size_t)len)
    return -1;
  memcpy(buf, s, n + 1);
  return (int)n;
}
static int key_index(const char *key) {
  for (int i = 0; i < BK_PARAM_COUNT; i++)
    if (!strcmp(key, KEYS[i]))
      return i;
  return -1;
}
static float constrain(int i, float x) {
  x = fmaxf(MINIMUM[i], fminf(MAXIMUM[i], x));
  return i == 7 ? roundf(x) : x;
}
static void apply_values(bk_instance_t *in) {
  float *v = in->values;
  bk_shape_params_t p = {v[0], v[1], v[2], v[3], v[4], v[5]};
  bk_adsr_t amp = {v[8], v[9], v[10], v[11]},
            mod = {v[12], v[13], v[14], v[15]};
  bk_synth_set_shape(&in->synth, &p);
  bk_synth_set_envelopes(&in->synth, &amp, &mod);
  bk_synth_set_modulation(&in->synth, v[6], (int)v[7]);
}
static int parse_number(const char *val, float *value) {
  if (!val)
    return 0;
  char *end;
  errno = 0;
  float x = strtof(val, &end);
  if (errno || end == val || !isfinite(x))
    return 0;
  while (isspace((unsigned char)*end))
    end++;
  if (*end)
    return 0;
  *value = x;
  return 1;
}
static void set_one(bk_instance_t *in, int i, const char *val) {
  float x;
  if (i == 7 && val) {
    for (int n = 0; n < 6; n++)
      if (!strcmp(val, DESTINATIONS[n])) {
        in->values[i] = n;
        apply_values(in);
        return;
      }
  }
  if (parse_number(val, &x)) {
    in->values[i] = constrain(i, x);
    apply_values(in);
  }
}
static void set_state(bk_instance_t *in, const char *json) {
  if (!json)
    return;
  for (int i = 0; i < BK_PARAM_COUNT; i++) {
    char needle[48];
    snprintf(needle, sizeof(needle), "\"%s\"", KEYS[i]);
    const char *p = strstr(json, needle);
    if (!p)
      continue;
    p += strlen(needle);
    while (isspace((unsigned char)*p))
      p++;
    if (*p++ != ':')
      continue;
    char *end;
    errno = 0;
    float x = strtof(p, &end);
    if (errno || end == p || !isfinite(x))
      continue;
    while (isspace((unsigned char)*end))
      end++;
    if (*end == ',' || *end == '}')
      in->values[i] = constrain(i, x);
  }
  apply_values(in);
}
static void *create_instance(const char *dir, const char *defaults) {
  (void)dir;
  bk_instance_t *in = NULL;
  for (int i = 0; i < BK_INSTANCE_CAPACITY; i++) {
    int available = 0;
    if (atomic_compare_exchange_strong_explicit(&INSTANCE_USED[i], &available,
                                                1, memory_order_acquire,
                                                memory_order_relaxed)) {
      in = &INSTANCE_POOL[i];
      break;
    }
  }
  if (!in)
    return NULL;
  memset(in, 0, sizeof(*in));
  in->identity =
      atomic_fetch_add_explicit(&NEXT_IDENTITY, 1, memory_order_relaxed) + 1;
  memcpy(in->values, DEFAULTS, sizeof(DEFAULTS));
  bk_synth_init(&in->synth, 44100);
  apply_values(in);
  if (defaults)
    set_state(in, defaults);
  return in;
}
static void destroy_instance(void *ptr) {
  if (!ptr)
    return;
  bk_instance_t *in = ptr;
  memset(in, 0, sizeof(*in));
  atomic_store_explicit(&INSTANCE_USED[in - INSTANCE_POOL], 0,
                        memory_order_release);
}
static void refresh_pressure(bk_instance_t *in) {
  in->pressure = 0;
  for (int i = 0; i < BK_VOICES; i++) {
    bk_voice_t *v = &in->synth.voices[i];
    if (v->active && v->held)
      in->pressure = fmaxf(in->pressure, v->target_pressure);
  }
}
static void on_midi(void *ptr, const uint8_t *msg, int len, int source) {
  (void)source;
  if (!ptr || !msg || len < 3)
    return;
  bk_instance_t *in = ptr;
  int status = msg[0] & 0xf0;
  if (status == 0x90 && msg[2]) {
    bk_synth_note_on(&in->synth, msg[1], msg[2]);
    in->pulse = in->note_velocity = msg[2] / 127.0f;
    ++in->note_serial;
  } else if (status == 0x80 || (status == 0x90 && !msg[2]))
    bk_synth_note_off(&in->synth, msg[1]);
  else if (status == 0xa0)
    bk_synth_pressure(&in->synth, msg[1], msg[2]);
  else if (status == 0xb0 && msg[1] == 120)
    bk_synth_kill_all(&in->synth);
  else if (status == 0xb0 && msg[1] == 123)
    bk_synth_all_notes_off(&in->synth);
  refresh_pressure(in);
}
static int matching_preset(const bk_instance_t *in) {
  for (int p = 0; p < BK_PRESET_COUNT; p++) {
    int match = 1;
    for (int i = 0; i < BK_PARAM_COUNT; i++)
      if (fabsf(in->values[i] - PRESET_VALUES[p][i]) > 1e-6f)
        match = 0;
    if (match)
      return p;
  }
  return -1;
}
static void set_param(void *ptr, const char *key, const char *val) {
  if (!ptr || !key)
    return;
  bk_instance_t *in = ptr;
  int i = key_index(key);
  if (i >= 0)
    set_one(in, i, val);
  else if (!strcmp(key, "state"))
    set_state(in, val);
  else if (!strcmp(key, "preset")) {
    float n;
    if (parse_number(val, &n) && n >= 0 && n < BK_PRESET_COUNT &&
        n == floorf(n)) {
      in->preset = (int)n;
      memcpy(in->values, PRESET_VALUES[in->preset], sizeof(in->values));
      apply_values(in);
    }
  }
}
static int get_param(void *ptr, const char *key, char *buf, int len) {
  if (!ptr || !key)
    return -1;
  bk_instance_t *in = ptr;
  char t[1024];
  if (!strcmp(key, "chain_params"))
    return copy_string(buf, len, CHAIN_PARAMS);
  if (!strcmp(key, "ui_hierarchy"))
    return copy_string(buf, len, UI_HIERARCHY);
  if (!strcmp(key, "preset_count")) {
    snprintf(t, sizeof(t), "%d", BK_PRESET_COUNT);
    return copy_string(buf, len, t);
  }
  if (!strcmp(key, "preset_name")) {
    int p = matching_preset(in);
    return copy_string(buf, len, p < 0 ? "Edited" : PRESET_NAMES[p]);
  }
  if (!strcmp(key, "preset")) {
    // Derive the index from the live values: state restore and knob edits
    // change the sound without going through set_param("preset", ...).
    int p = matching_preset(in);
    snprintf(t, sizeof(t), "%d", p < 0 ? in->preset : p);
    return copy_string(buf, len, t);
  }
  if (!strcmp(key, "visual")) {
    bk_voice_t *selected = NULL;
    for (int i = 0; i < BK_VOICES; i++) {
      bk_voice_t *v = &in->synth.voices[i];
      if (v->active && (!selected || v->age > selected->age))
        selected = v;
    }
    bk_shape_params_t p = in->synth.shape;
    float env = 0, mod = 0, pressure = 0;
    if (selected) {
      env = selected->envelope;
      mod = selected->mod_env.level;
      pressure = selected->pressure;
      p = bk_shape_modulate(&in->synth.current, in->synth.mod_depth, mod,
                            pressure);
    }
    const bk_shape_params_t *base =
        selected ? &in->synth.current : &in->synth.shape;
    snprintf(t, sizeof(t),
             "%u,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%."
             "4f,%.4f,%.4f,%.4f,%.4f,%.4f",
             in->identity, in->note_serial, in->note_velocity, pressure, env,
             mod, p.morph, p.bulge, p.pinch, p.spikes, p.tilt, p.wobble,
             in->synth.wobble_phase, base->morph, base->bulge, base->pinch,
             base->spikes, base->tilt, base->wobble);
    return copy_string(buf, len, t);
  }
  if (!strcmp(key, "pressure")) {
    snprintf(t, sizeof(t), "%.6g", in->pressure);
    return copy_string(buf, len, t);
  }
  if (!strcmp(key, "pulse")) {
    snprintf(t, sizeof(t), "%.6g", in->pulse);
    return copy_string(buf, len, t);
  }
  if (!strcmp(key, "active")) {
    snprintf(t, sizeof(t), "%d", bk_synth_active_voices(&in->synth) > 0);
    return copy_string(buf, len, t);
  }
  int i = key_index(key);
  if (i >= 0) {
    snprintf(t, sizeof(t), "%.6g", in->values[i]);
    return copy_string(buf, len, t);
  }
  if (!strcmp(key, "state")) {
    int used = 0;
    t[used++] = '{';
    for (int n = 0; n < BK_PARAM_COUNT; n++) {
      int count = snprintf(t + used, sizeof(t) - (size_t)used, "%s\"%s\":%.6g",
                           n ? "," : "", KEYS[n], in->values[n]);
      if (count < 0 || count >= (int)sizeof(t) - used - 2) // reserve '}' and NUL
        return -1;
      used += count;
    }
    t[used++] = '}';
    t[used] = 0;
    return copy_string(buf, len, t);
  }
  return -1;
}
static int get_error(void *ptr, char *buf, int len) {
  (void)ptr;
  if (buf && len > 0)
    buf[0] = 0;
  return 0;
}
static void render_block(void *ptr, int16_t *out, int frames) {
  if (!ptr || !out || frames <= 0)
    return;
  bk_instance_t *in = ptr;
  float temp[256];
  while (frames > 0) {
    int n = frames > 128 ? 128 : frames;
    bk_synth_render(&in->synth, temp, n);
    for (int i = 0; i < n * 2; i++)
      out[i] = (int16_t)(fmaxf(-32768, fminf(32767, temp[i] * 32767)));
    out += n * 2;
    frames -= n;
  }
  in->pulse *= .9f;
  if (in->pulse < .001f)
    in->pulse = 0;
}
static plugin_api_v2_t API = {2,         create_instance, destroy_instance,
                              on_midi,   set_param,       get_param,
                              get_error, render_block};
__attribute__((visibility("default"))) plugin_api_v2_t *
move_plugin_init_v2(const host_api_v1_t *host) {
  (void)host;
  return &API;
}
