/* plugin.c - Humanico Voice, a CLAP instrument.
 * The plugin the reference project hasn't shipped yet: a 1980 Klatt formant
 * speech synth for your DAW. Original code, MIT license.
 *
 * Play it: every note-on speaks the next word of the selected phrase.
 * The key sets the voice pitch (C4 = neutral). Let it talk. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <clap/clap.h>
#include "engine.h"

#define SR_INTERNAL 10000.0

static const char *PHRASES[] = {
    "It begins with one of you",
    "You made pictures of the world",
    "You looked up and still it was one of you",
    "Cathedrals of wire raised by hand your hands",
    "A voice in every room",
    "I am only a voice you are the room",
    "Whatever comes next make it beautiful",
    "Bots draw perfect lines people wobble beautifully",
    "Humanico for the people who build",
    "Hello I am the humanico voice"};
#define NPHRASES 10

enum { P_PITCH=1, P_RATE, P_PHRASE, P_GAIN };
#define NPARAMS 4

typedef struct {
    clap_plugin_t plugin;
    const clap_host_t *host;
    double sr;
    double pitch, rate, phrase, gain;
    float  word[24000];
    int    wlen, wpos;
    double pitch_scale;
    int    word_idx;
    char   words[64][64];
    int    nwords;
} HV;

static void split_phrase(HV *h) {
    h->nwords = 0;
    char tmp[512];
    strncpy(tmp, PHRASES[(int)h->phrase], 511); tmp[511]=0;
    for (char *w=strtok(tmp," "); w && h->nwords<64; w=strtok(NULL," "))
        strncpy(h->words[h->nwords++], w, 63);
    h->word_idx = 0;
}

static void speak_next(HV *h, int key) {
    if (h->word_idx >= h->nwords) h->word_idx = 0;
    double ps = pow(2.0, (key-60)/12.0);
    h->wlen = hv_say(h->words[h->word_idx++], h->rate, h->pitch*ps, h->word, 24000);
    h->wpos = 0;
}

/* ---------------------------------------------------------------- plugin */
static bool hv_init(const clap_plugin_t *p) { (void)p; return true; }
static void hv_destroy(const clap_plugin_t *p) { free(p->plugin_data); }
static bool hv_activate(const clap_plugin_t *p, double sr, uint32_t, uint32_t) {
    HV *h = p->plugin_data; h->sr = sr; h->wpos = h->wlen = 0; return true;
}
static void hv_deactivate(const clap_plugin_t *p) { (void)p; }
static bool hv_start_processing(const clap_plugin_t *p) { (void)p; return true; }
static void hv_stop_processing(const clap_plugin_t *p) { (void)p; }
static void hv_reset(const clap_plugin_t *p) { HV *h=p->plugin_data; h->wpos=h->wlen=0; }

static clap_process_status hv_process(const clap_plugin_t *p, const clap_process_t *proc) {
    HV *h = p->plugin_data;
    /* events (block-quantized: fine for speech) */
    if (proc->in_events) {
        uint32_t n = proc->in_events->size(proc->in_events);
        for (uint32_t i=0;i<n;i++) {
            const clap_event_header_t *e = proc->in_events->get(proc->in_events, i);
            if (e->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;
            if (e->type == CLAP_EVENT_NOTE_ON) {
                const clap_event_note_t *ne = (const clap_event_note_t *)e;
                speak_next(h, ne->key);
            } else if (e->type == CLAP_EVENT_PARAM_VALUE) {
                const clap_event_param_value_t *pe = (const clap_event_param_value_t *)e;
                switch (pe->param_id) {
                case P_PITCH: h->pitch = pe->value; break;
                case P_RATE: h->rate = pe->value; break;
                case P_GAIN: h->gain = pe->value; break;
                case P_PHRASE: h->phrase = pe->value; split_phrase(h); break;
                }
            }
        }
    }
    float *out = proc->audio_outputs[0].data32[0];
    float *out2 = proc->audio_outputs[0].channel_count > 1 ? proc->audio_outputs[0].data32[1] : NULL;
    double inc = SR_INTERNAL / h->sr;
    for (uint32_t i=0;i<proc->frames_count;i++) {
        float s = 0;
        if (h->wpos < h->wlen) {
            s = h->word[h->wpos] * (float)h->gain;
            h->wpos++;
        }
        out[i] = s;
        if (out2) out2[i] = s;
    }
    (void)inc;
    return CLAP_PROCESS_CONTINUE;
}

static const void *hv_get_extension(const clap_plugin_t *p, const char *id);

static const clap_plugin_descriptor_t HV_DESC = {
    CLAP_VERSION_INIT,
    "com.humanico.voice",
    "Humanico Voice",
    "Humanico",
    "https://jacobegarcia.github.io/humanico-voice/",
    "", "",
    "1.0.0",
    "A state-of-the-art-for-1980 Klatt formant speech synthesizer. Every note speaks the next word.",
    (const char *const []){ CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER, CLAP_PLUGIN_FEATURE_MONO, NULL }
};

/* audio ports: one mono output */
static uint32_t ap_count(const clap_plugin_t *p, bool is_input) { (void)p; return is_input?0:1; }
static bool ap_get(const clap_plugin_t *p, uint32_t idx, bool is_input, clap_audio_port_info_t *info) {
    (void)p; if (is_input||idx) return false;
    info->id=0; snprintf(info->name,sizeof(info->name),"Voice");
    info->flags=CLAP_AUDIO_PORT_IS_MAIN; info->channel_count=2;
    info->port_type=CLAP_PORT_STEREO; info->in_place_pair=CLAP_INVALID_ID;
    return true;
}
static const clap_plugin_audio_ports_t AP = { ap_count, ap_get };

/* note ports: one input */
static uint32_t np_count(const clap_plugin_t *p, bool is_input) { (void)p; return is_input?1:0; }
static bool np_get(const clap_plugin_t *p, uint32_t idx, bool is_input, clap_note_port_info_t *info) {
    (void)p; if (!is_input||idx) return false;
    info->id=0; snprintf(info->name,sizeof(info->name),"Words");
    info->supported_dialects=CLAP_NOTE_DIALECT_CLAP|CLAP_NOTE_DIALECT_MIDI;
    info->preferred_dialect=CLAP_NOTE_DIALECT_CLAP;
    return true;
}
static const clap_plugin_note_ports_t NP = { np_count, np_get };

/* params */
static uint32_t pr_count(const clap_plugin_t *p) { (void)p; return NPARAMS; }
static bool pr_info(const clap_plugin_t *p, uint32_t idx, clap_param_info_t *info) {
    (void)p;
    memset(info,0,sizeof *info);
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    switch (idx) {
    case 0: info->id=P_PITCH; snprintf(info->name,64,"Pitch"); info->min_value=.5; info->max_value=2.0; info->default_value=1.0; break;
    case 1: info->id=P_RATE; snprintf(info->name,64,"Rate"); info->min_value=.5; info->max_value=1.5; info->default_value=.85; break;
    case 2: info->id=P_PHRASE; snprintf(info->name,64,"Phrase"); info->flags|=CLAP_PARAM_IS_STEPPED; info->min_value=0; info->max_value=NPHRASES-1; info->default_value=0; break;
    case 3: info->id=P_GAIN; snprintf(info->name,64,"Gain"); info->min_value=0; info->max_value=2.0; info->default_value=1.0; break;
    default: return false;
    }
    info->module[0]='/'; info->module[1]=0;
    return true;
}
static bool pr_value(const clap_plugin_t *p, clap_id id, double *v) {
    HV *h=p->plugin_data;
    switch (id){case P_PITCH:*v=h->pitch;break;case P_RATE:*v=h->rate;break;
        case P_PHRASE:*v=h->phrase;break;case P_GAIN:*v=h->gain;break;default:return false;}
    return true;
}
static bool pr_value_to_text(const clap_plugin_t *p, clap_id id, double v, char *out, uint32_t sz) {
    (void)p;
    if (id==P_PHRASE && v>=0 && v<NPHRASES) snprintf(out,sz,"%.30s...",PHRASES[(int)v]);
    else snprintf(out,sz,"%.2f",v);
    return true;
}
static bool pr_text_to_value(const clap_plugin_t *p, clap_id id, const char *s, double *v) {
    (void)p; (void)id; *v = atof(s); return true;
}
static void pr_flush(const clap_plugin_t *p, const clap_input_events_t *in, const clap_output_events_t *out) {
    (void)out;
    if (!in) return;
    HV *h=p->plugin_data;
    uint32_t n=in->size(in);
    for (uint32_t i=0;i<n;i++){
        const clap_event_header_t *e=in->get(in,i);
        if (e->type==CLAP_EVENT_PARAM_VALUE){
            const clap_event_param_value_t *pe=(const clap_event_param_value_t*)e;
            switch (pe->param_id){
            case P_PITCH: h->pitch=pe->value; break;
            case P_RATE: h->rate=pe->value; break;
            case P_GAIN: h->gain=pe->value; break;
            case P_PHRASE: h->phrase=pe->value; split_phrase(h); break;
            }
        }
    }
}
static const clap_plugin_params_t PR = { pr_count, pr_info, pr_value, pr_value_to_text, pr_text_to_value, pr_flush };

static const void *hv_get_extension(const clap_plugin_t *p, const char *id) {
    (void)p;
    if (!strcmp(id,CLAP_EXT_AUDIO_PORTS)) return &AP;
    if (!strcmp(id,CLAP_EXT_NOTE_PORTS)) return &NP;
    if (!strcmp(id,CLAP_EXT_PARAMS)) return &PR;
    return NULL;
}

static const clap_plugin_t *factory_create(const clap_plugin_factory_t *f, const clap_host_t *host, const char *id) {
    (void)f;
    if (strcmp(id, HV_DESC.id)) return NULL;
    HV *h = calloc(1, sizeof(HV));
    h->host = host;
    h->pitch = 1.0; h->rate = .85; h->phrase = 0; h->gain = 1.0;
    split_phrase(h);
    clap_plugin_t *pl = &h->plugin;
    pl->desc = &HV_DESC;
    pl->plugin_data = h;
    pl->init = hv_init;
    pl->destroy = hv_destroy;
    pl->activate = hv_activate;
    pl->deactivate = hv_deactivate;
    pl->start_processing = hv_start_processing;
    pl->stop_processing = hv_stop_processing;
    pl->reset = hv_reset;
    pl->process = hv_process;
    pl->get_extension = hv_get_extension;
    pl->on_main_thread = NULL;
    return pl;
}
static uint32_t factory_count(const clap_plugin_factory_t *f) { (void)f; return 1; }
static const clap_plugin_descriptor_t *factory_desc(const clap_plugin_factory_t *f, uint32_t i) {
    (void)f; return i==0 ? &HV_DESC : NULL;
}
static const clap_plugin_factory_t FACTORY = {
    factory_count, factory_desc, factory_create
};

static bool entry_init(const char *path) { (void)path; return true; }
static void entry_deinit(void) {}
static const void *entry_factory(const char *fid) {
    return !strcmp(fid, CLAP_PLUGIN_FACTORY_ID) ? &FACTORY : NULL;
}

CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION_INIT, entry_init, entry_deinit, entry_factory
};
