#include <dlfcn.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plugin_api_v1.h"

static int failures;
static void ok(int yes, const char *name) {
    printf("  %s %s\n", yes ? "ok  " : "FAIL", name);
    if (!yes) ++failures;
}
static int occurrences(const char *s, const char *needle) {
    int n = 0; size_t step = strlen(needle);
    while ((s = strstr(s, needle)) != NULL) { ++n; s += step; }
    return n;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "build-host/bouba-kiki-host.so";
    void *lib = dlopen(path, RTLD_NOW);
    if (!lib) { fprintf(stderr, "%s\n", dlerror()); return 1; }
    move_plugin_init_v2_fn init = (move_plugin_init_v2_fn)dlsym(lib, MOVE_PLUGIN_INIT_V2_SYMBOL);
    ok(init != NULL, "exports v2 entry point");
    host_api_v1_t host = {0}; host.sample_rate = 44100; host.frames_per_block = 128;
    plugin_api_v2_t *api = init(&host);
    ok(api && api->api_version == 2, "reports API v2");
    ok(api && api->create_instance && api->destroy_instance && api->on_midi &&
       api->set_param && api->get_param && api->render_block, "provides required callbacks");
    void *inst = api->create_instance(".", NULL);
    ok(inst != NULL, "creates instance");
    char value[4096];
    ok(api->get_param(inst, "chain_params", value, sizeof(value)) > 0 && strstr(value, "morph"), "serves chain params");
    ok(occurrences(value, "\"step\":0.01") == 8, "runtime contract serves all knob steps");
    ok(occurrences(value, "\"default\":") == 8, "runtime contract serves all knob defaults");
    ok(api->get_param(inst, "ui_hierarchy", value, sizeof(value)) > 0 && strstr(value, "shape"), "serves hierarchy");
    const char *keys[] = {"morph","bulge","pinch","spikes","tilt","wobble","attack","release"};
    const double defaults[] = {0.25,0.35,0,0.25,0.5,0.1,0.05,0.25};
    for (unsigned i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i) {
        ok(api->get_param(inst, keys[i], value, sizeof(value)) > 0 && fabs(atof(value)-defaults[i]) < 0.001, "runtime default matches manifest");
        api->set_param(inst, keys[i], "0.73");
        ok(api->get_param(inst, keys[i], value, sizeof(value)) > 0 && fabs(atof(value)-0.73) < 0.001, keys[i]);
    }
    api->set_param(inst, "morph", "garbage");
    api->get_param(inst, "morph", value, sizeof(value));
    ok(fabs(atof(value)-0.73) < 0.001, "rejects malformed numeric value");
    api->set_param(inst, "morph", "9");
    api->get_param(inst, "morph", value, sizeof(value));
    ok(fabs(atof(value)-1.0) < 0.001, "clamps numeric value");
    api->set_param(inst, "state", "{\"morph\":nan}");
    api->get_param(inst, "morph", value, sizeof(value));
    ok(fabs(atof(value)-1.0) < 0.001, "rejects non-finite state value");
    char state[1024];
    ok(api->get_param(inst, "state", state, sizeof(state)) > 0, "serves state");
    void *copy = api->create_instance(".", NULL);
    api->set_param(copy, "state", state);
    api->get_param(copy, "morph", value, sizeof(value));
    ok(fabs(atof(value)-1.0) < 0.001, "restores state");
    api->destroy_instance(copy);

    int16_t out[256] = {0};
    const uint8_t on[] = {0x90, 60, 110}; api->on_midi(inst, on, 3, 0);
    int peak = 0;
    for (int b = 0; b < 50; ++b) {
        api->render_block(inst, out, 128);
        for (int i = 0; i < 256; ++i) { int x = abs(out[i]); if (x > peak) peak = x; }
    }
    ok(peak > 500 && peak <= 32767, "renders bounded note audio");
    const uint8_t pressure[] = {0xA0, 60, 127}; api->on_midi(inst, pressure, 3, 0);
    api->get_param(inst, "pressure", value, sizeof(value));
    ok(atof(value) > 0.99, "reports pressure for visualizer");
    const uint8_t off[] = {0x80, 60, 0}; api->on_midi(inst, off, 3, 0);
    api->get_param(inst, "pressure", value, sizeof(value));
    ok(atof(value) == 0.0, "clears visual pressure on note off");
    const uint8_t on2[] = {0x90,62,100}, on3[] = {0x90,64,100};
    const uint8_t p2[] = {0xA0,62,100}, p3[] = {0xA0,64,30};
    api->on_midi(inst,on2,3,0); api->on_midi(inst,on3,3,0);
    api->on_midi(inst,p2,3,0); api->on_midi(inst,p3,3,0);
    api->get_param(inst,"pressure",value,sizeof(value));
    ok(atof(value) > 0.75, "reports strongest active poly pressure");
    const uint8_t panic[] = {0xB0, 120, 0}; api->on_midi(inst, panic, 3, 0);
    ok(api->get_param(inst,"active",value,sizeof(value)) > 0 && atoi(value) == 0,
       "CC120 silences immediately");
    ok(api->get_param(inst, "unknown", value, sizeof(value)) < 0, "rejects unknown key");
    api->destroy_instance(inst); dlclose(lib);
    printf("%s (%d failures)\n", failures ? "FAILED" : "PASS: plugin smoke", failures);
    return failures != 0;
}
