/* test_host.c - minimal CLAP host: dlopen, instantiate, play notes, write WAV */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <math.h>
#include <clap/clap.h>

#define SR 44100
#define BLOCK 512
#define NNOTES 6
static int keys[NNOTES] = {60, 60, 62, 60, 64, 60};

static clap_event_note_t notes[NNOTES];
static int note_frame[NNOTES];
static int cur_frame;

static uint32_t in_size(const clap_input_events_t *l) {
    int n=0;
    for (int i=0;i<NNOTES;i++) if (note_frame[i] >= cur_frame && note_frame[i] < cur_frame+BLOCK) n++;
    return n;
}
static const clap_event_header_t *in_get(const clap_input_events_t *l, uint32_t idx) {
    int n=0;
    for (int i=0;i<NNOTES;i++) if (note_frame[i] >= cur_frame && note_frame[i] < cur_frame+BLOCK) {
        if ((int)idx==n) return &notes[i].header; n++;
    }
    return NULL;
}

int main(void) {
    void *dl = dlopen("./humanico-voice.clap", RTLD_NOW);
    if (!dl) { fprintf(stderr,"dlopen: %s\n", dlerror()); return 1; }
    const clap_plugin_entry_t *e = dlsym(dl, "clap_entry");
    if (!e) { fprintf(stderr,"no clap_entry\n"); return 1; }
    e->init(".");
    const clap_plugin_factory_t *f = e->get_factory(CLAP_PLUGIN_FACTORY_ID);
    printf("plugins: %u\n", f->get_plugin_count(f));
    const clap_plugin_descriptor_t *d = f->get_plugin_descriptor(f, 0);
    printf("plugin: %s (%s) v%s — %s\n", d->name, d->id, d->version, d->description);
    const clap_plugin_t *pl = f->create_plugin(f, NULL, d->id);
    pl->init(pl);
    const clap_plugin_audio_ports_t *ap = pl->get_extension(pl, CLAP_EXT_AUDIO_PORTS);
    clap_audio_port_info_t api; ap->get(pl,0,false,&api);
    printf("audio out: %s ch=%d type=%s\n", api.name, api.channel_count, api.port_type);
    const clap_plugin_note_ports_t *np = pl->get_extension(pl, CLAP_EXT_NOTE_PORTS);
    clap_note_port_info_t npi; np->get(pl,0,true,&npi);
    printf("note in: %s dialects=%x\n", npi.name, npi.supported_dialects);
    const clap_plugin_params_t *pr = pl->get_extension(pl, CLAP_EXT_PARAMS);
    uint32_t pc = pr->count(pl);
    for (uint32_t i=0;i<pc;i++){ clap_param_info_t pi; pr->get_info(pl,i,&pi); printf("param %d: %s [%.2f..%.2f] def %.2f\n",(int)pi.id,pi.name,pi.min_value,pi.max_value,pi.default_value); }
    pl->activate(pl, SR, BLOCK, BLOCK);
    pl->start_processing(pl);

    for (int i=0;i<NNOTES;i++) {
        memset(&notes[i],0,sizeof notes[i]);
        notes[i].header.size = sizeof notes[i];
        notes[i].header.type = CLAP_EVENT_NOTE_ON;
        notes[i].header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        notes[i].header.flags = 0;
        notes[i].header.time = 0;
        notes[i].key = keys[i];
        notes[i].velocity = 1.0;
        note_frame[i] = i * (int)(0.75 * SR);
    }
    int total = (int)(0.75*SR*NNOTES + SR);
    float *bufL = malloc(BLOCK*sizeof(float)), *bufR = malloc(BLOCK*sizeof(float));
    FILE *wav = fopen("/tmp/plugin_test.wav","wb");
    int nblocks = total/BLOCK + 1, nsamp = nblocks*BLOCK;
    /* WAV header */
    fwrite("RIFF",1,4,wav); unsigned tmp = 36+nsamp*4; fwrite(&tmp,4,1,wav);
    fwrite("WAVEfmt ",1,8,wav); tmp=16; fwrite(&tmp,4,1,wav);
    short fm=3, ch=2, br=16*8; fwrite(&fm,2,1,wav); fwrite(&ch,2,1,wav);
    tmp=SR; fwrite(&tmp,4,1,wav); tmp=SR*8; fwrite(&tmp,4,1,wav);
    short ba=8; fwrite(&ba,2,1,wav); br=32; fwrite(&br,2,1,wav);
    fwrite("data",1,4,wav); tmp=nsamp*4; fwrite(&tmp,4,1,wav);

    clap_audio_buffer_t ab; memset(&ab,0,sizeof ab);
    float *chans[2] = {bufL, bufR};
    ab.channel_count = 2; ab.data32 = chans;
    clap_process_t proc; memset(&proc,0,sizeof proc);
    clap_input_events_t in; in.ctx=NULL; in.size=in_size; in.get=in_get;
    proc.audio_outputs = &ab; proc.audio_outputs_count = 1;
    proc.in_events = &in; proc.frames_count = BLOCK;
    proc.transport = NULL; proc.steady_time = 0;

    double peak=0, rms=0; long n=0;
    for (cur_frame=0; cur_frame<total; cur_frame+=BLOCK) {
        memset(bufL,0,BLOCK*4); memset(bufR,0,BLOCK*4);
        pl->process(pl, &proc);
        for (int i=0;i<BLOCK;i++){
            fwrite(&bufL[i],4,1,wav); fwrite(&bufR[i],4,1,wav);
            double a=fabs(bufL[i]); if(a>peak)peak=a; rms+=bufL[i]*bufL[i]; n++;
        }
    }
    fclose(wav);
    printf("rendered %ld samples, peak=%.3f rms=%.4f -> /tmp/plugin_test.wav\n", n, peak, sqrt(rms/n));
    pl->stop_processing(pl); pl->deactivate(pl); pl->destroy(pl); e->deinit();
    return 0;
}
