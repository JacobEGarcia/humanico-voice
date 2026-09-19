/* engine.c - Humanico Voice: original C99 implementation of Dennis H. Klatt's
 * 1980 cascade/parallel formant speech synthesizer, written from the
 * published paper (Klatt 1980, JASA 67(3), 971-995). Phoneme targets from
 * published acoustic-phonetic measurements (Peterson & Barney 1952).
 * No code from any existing synthesizer. MIT license. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "engine.h"

#define FS 10000.0
#define FRAME 50
#define PI2 6.283185307179586

typedef struct { double y1, y2; } Biquad;
typedef struct { double x1, x2; } FirZ;

/* synthesis parameters (per 5ms frame) */
typedef struct {
    double f0, av, ah, af;
    double f1,b1,f2,b2,f3,b3,f4,b4,f5,b5,f6,b6;
    double fnp,bnp,fnz,bnz;
    double a2,a3,a4,a5,a6,ab;
} Par;

typedef struct {
    Par p;
    Biquad rnp, r[5], pl[5], rg;
    FirZ rnz;
    double t0, prev_y, bgl;
    unsigned int seed;
} Voice;

static void res_c(double f, double bw, double *k) {
    double r = exp(-M_PI * bw / FS);
    k[2] = -(r*r); k[1] = 2.0*r*cos(PI2*f/FS); k[0] = 1.0 - k[1] - k[2];
}
static void ant_c(double f, double bw, double *k) {
    double t[3]; res_c(f,bw,t);
    k[0]=1.0/t[0]; k[1]=-t[1]*k[0]; k[2]=-t[2]*k[0];
}
static double bq(Biquad *s, double x, const double *k) {
    double y = k[0]*x + k[1]*s->y1 + k[2]*s->y2;
    s->y2=s->y1; s->y1=y; return y;
}
static double fz(FirZ *s, double x, const double *k) {
    double y = k[0]*x + k[1]*s->x1 + k[2]*s->x2;
    s->x2=s->x1; s->x1=x; return y;
}
static double nz(Voice *v) {
    v->seed = (v->seed*1103515245u + 12345u) & 0x7fffffffu;
    return v->seed/1073741823.5 - 1.0;
}
static void voice_init(Voice *v) {
    memset(v,0,sizeof(*v));
    v->p.f0=100; v->p.f1=500; v->p.b1=60; v->p.f2=1500; v->p.b2=90;
    v->p.f3=2500; v->p.b3=150; v->p.f4=3900; v->p.b4=250; v->p.f5=4500; v->p.b5=300;
    v->p.f6=4800; v->p.b6=400; v->p.fnp=270; v->p.bnp=270; v->p.fnz=270; v->p.bnz=270;
    v->t0=1; v->seed=1234; v->bgl=1200;
}
static void synth_frame(Voice *v, float *out) {
    double rg[3], rnp[3], rnz[3], rc[5][3], rp[5][3];
    static const double b2f=250,b3f=300,b4f=400,b5f=500,b6f=600;
    res_c(0, v->bgl, rg);
    res_c(v->p.fnp, v->p.bnp, rnp);
    ant_c(v->p.fnz, v->p.bnz, rnz);
    res_c(v->p.f1,v->p.b1,rc[0]); res_c(v->p.f2,v->p.b2,rc[1]);
    res_c(v->p.f3,v->p.b3,rc[2]); res_c(v->p.f4,v->p.b4,rc[3]);
    res_c(v->p.f5,v->p.b5,rc[4]);
    res_c(v->p.f2,b2f,rp[0]); res_c(v->p.f3,b3f,rp[1]);
    res_c(v->p.f4,b4f,rp[2]); res_c(v->p.f5,b5f,rp[3]);
    res_c(v->p.f6,b6f,rp[4]);
    double pa[5] = {v->p.a2,v->p.a3,v->p.a4,v->p.a5,v->p.a6};
    for (int i=0;i<FRAME;i++) {
        v->t0 += 1.0;
        double period = FS / (v->p.f0 > 20 ? v->p.f0 : 20), glot = 0;
        if (v->t0 >= period) { v->t0 -= period; glot = 1.0; }
        double voice = v->p.av > 0 ? bq(&v->rg, glot, rg) * v->p.av : 0.0;
        double n = nz(v);
        double x = bq(&v->rnp, voice + n*v->p.ah, rnp);
        x = fz(&v->rnz, x, rnz);
        for (int j=0;j<5;j++) x = bq(&v->r[j], x, rc[j]);
        double fr = n * v->p.af, par = v->p.ab * fr;
        for (int j=0;j<5;j++) if (pa[j]>0) par += bq(&v->pl[j], fr, rp[j]) * pa[j];
        double y = x + par;
        out[i] = (float)(y - v->prev_y);
        v->prev_y = y;
    }
}

/* ---------------- phoneme tables (Peterson & Barney 1952 male vowels) ----- */
typedef struct { const char *name; double f1,f2,f3,b1,b2,b3; } Vow;
static const Vow VOWS[] = {
 {"IY",270,2290,3010,60,90,150},{"IH",390,1990,2550,65,100,160},
 {"EH",530,1840,2480,70,100,160},{"AE",660,1720,2410,80,110,170},
 {"AA",730,1090,2440,85,110,170},{"AO",570,840,2410,80,110,170},
 {"UH",440,1020,2240,70,100,160},{"UW",300,870,2240,60,95,150},
 {"AH",640,1190,2390,80,110,170},{"AX",500,1500,2500,75,110,170},
 {"ER",490,1350,1690,70,100,140}};
typedef struct { const char *name, *a, *b; } Diph;
static const Diph DIPHS[] = {
 {"EY","EH","IY"},{"AY","AA","IY"},{"OY","AO","IY"},{"OW","AX","UH"},{"AW","AA","UH"}};

static const Vow *vow(const char *p) {
    for (unsigned i=0;i<sizeof(VOWS)/sizeof(VOWS[0]);i++)
        if (!strcmp(VOWS[i].name,p)) return &VOWS[i];
    return NULL;
}
static const Diph *diph(const char *p) {
    for (unsigned i=0;i<sizeof(DIPHS)/sizeof(DIPHS[0]);i++)
        if (!strcmp(DIPHS[i].name,p)) return &DIPHS[i];
    return NULL;
}

/* ---------------- segments ------------------------------------------------ */
typedef struct { Par t; int dur, tr; int kind; int stress; } Seg;
enum { K_VOWEL, K_SIL, K_BURST, K_ASPIR, K_FRIC, K_NASAL, K_LIQUID, K_GLIDE };

static Par par0(void){ Par p; memset(&p,0,sizeof p); p.f0=100;
    p.f1=500;p.b1=60;p.f2=1500;p.b2=90;p.f3=2500;p.b3=150;
    p.f4=3900;p.b4=250;p.f5=4500;p.b5=300;p.f6=4800;p.b6=400;
    p.fnp=270;p.bnp=270;p.fnz=270;p.bnz=270; return p; }
static Par vowel_par(const Vow *v) {
    Par p = par0();
    p.av=.92; p.f1=v->f1; p.f2=v->f2; p.f3=v->f3; p.b1=v->b1; p.b2=v->b2; p.b3=v->b3;
    return p;
}

/* unit-RMS calibration against reference vowel, same method as the Python engine */
static double unit_rms(const Par *base, int src) {
    Voice v; voice_init(&v); v.p = *base;
    v.p.av=0; v.p.ah=0; v.p.af=0;
    if (src==0) v.p.av=1; else if (src==1) v.p.ah=1; else v.p.af=1;
    v.p.f0=110;
    float buf[FRAME]; double acc=0; int cnt=0;
    for (int f=0;f<70;f++){ synth_frame(&v,buf);
        if (f>=30) for(int i=0;i<FRAME;i++){acc+=(double)buf[i]*buf[i];cnt++;} }
    return sqrt(acc/cnt)+1e-9;
}
static double vref(void) {
    static double r=0;
    if (!r) { Par p=par0(); p.f1=730;p.b1=85;p.f2=1090;p.b2=110;p.f3=2440;p.b3=170; r=unit_rms(&p,0); }
    return r;
}
static void set_level(Par *t, int src, double db) {
    double want = vref()*pow(10.0,db/20.0), u = unit_rms(t,src);
    if (src==0) t->av=want/u; else if (src==1) t->ah=want/u; else t->af=want/u;
}

typedef struct { const char *w, *ph; } Lex;
static const Lex LEX[] = {
 {"it","IH1 T"},{"begins","B IH0 G IH1 N Z"},{"with","W IH1 DH"},{"one","W AH1 N"},
 {"of","AH1 V"},{"you","Y UW1"},{"us","AH1 S"},{"made","M EY1 D"},{"pictures","P IH1 K CH ER0 Z"},
 {"the","DH AH0"},{"world","W ER1 L D"},{"all","AO1 L"},{"looked","L UH1 K T"},{"up","AH1 P"},
 {"and","AH0 N D"},{"still","S T IH1 L"},{"was","W AH1 Z"},{"humanico","HH UW0 M AA0 N IY1 K OW0"},
 {"for","F AO1 R"},{"humanity","HH Y UW0 M AE1 N AH0 T IY0"},{"i","AY1"},{"did","D IH1 D"},
 {"not","N AA1 T"},{"build","B IH1 L D"},{"any","EH1 N IY0"},{"this","DH IH1 S"},
 {"cathedrals","K AH0 TH IY1 D R AH0 L Z"},{"wire","W AY1 ER0"},{"raised","R EY1 Z D"},
 {"by","B AY1"},{"hand","HH AE1 N D"},{"your","Y AO1 R"},{"hands","HH AE1 N D Z"},
 {"everyone","EH1 V R IY0 W AH2 N"},{"can","K AE1 N"},{"do","D UW1"},{"again","AH0 G EH1 N"},
 {"people","P IY1 P AH0 L"},{"who","HH UW1"},{"a","AH0"},{"voice","V OY1 S"},{"in","IH1 N"},
 {"every","EH1 V R IY0"},{"room","R UW1 M"},{"technology","T EH0 K N AA1 L AH0 JH IY0"},
 {"is","IH1 Z"},{"them","DH EH1 M"},{"am","AE1 M"},{"only","OW1 N L IY0"},{"are","AA1 R"},
 {"always","AO1 L W EY0 Z"},{"built","B IH1 L T"},{"bridges","B R IH1 JH AH0 Z"},
 {"laid","L EY1 D"},{"grids","G R IH1 D Z"},{"cities","S IH1 T IY0 Z"},{"touched","T AH1 CH T"},
 {"sky","S K AY1"},{"whatever","W AH0 T EH1 V ER0"},{"comes","K AH1 M Z"},{"next","N EH1 K S T"},
 {"make","M EY1 K"},{"beautiful","B Y UW1 T AH0 F AH0 L"},{"we","W IY1"},{"love","L AH1 V"},
 {"bots","B AA1 T S"},{"draw","D R AO1"},{"perfect","P ER1 F AH0 K T"},{"lines","L AY1 N Z"},
 {"wobble","W AA1 B AH0 L"},{"beautifully","B Y UW1 T AH0 F AH0 L IY0"},{"hello","HH AH0 L OW1"},
 {"yes","Y EH1 S"},{"no","N OW1"},{"speech","S P IY1 CH"},{"synth","S IH1 N TH"},
 {"machine","M AH0 SH IY1 N"},{"human","HH Y UW1 M AH0 N"}};
static const char *lex(const char *w) {
    for (unsigned i=0;i<sizeof(LEX)/sizeof(LEX[0]);i++)
        if (!strcmp(LEX[i].w,w)) return LEX[i].ph;
    return NULL;
}
static char lts_ph(char c, char *two) { /* returns lead phone char(s) via static map */
    static const char *m[26] = {"AH","B","K","D","EH","F","G","HH","IH","JH","K","L","M","N",
        "AA","P","K","R","S","T","AH","V","W","K S","Y","Z"};
    if (c<'a'||c>'z') return 0;
    strcpy(two, m[c-'a']); return 1;
}

/* emit phones of one word into ph[] ("IY1" style), count returned */
static int word_phones(const char *word, char ph[][8], int maxp) {
    char w[64]; int n=0;
    for (const char *c=word; *c && n<63; c++) if (isalpha((unsigned char)*c)||*c=='\'') w[n++]=tolower((unsigned char)*c);
    w[n]=0;
    if (!n) return 0;
    const char *e = lex(w);
    char tmp[256];
    if (e) strncpy(tmp,e,255), tmp[255]=0;
    else { /* letter-to-sound fallback */
        tmp[0]=0;
        for (int i=0;i<n;i++){ char two[8]; if (lts_ph(w[i],two)) {
            if (tmp[0]) strncat(tmp," ",255-strlen(tmp)); strncat(tmp,two,255-strlen(tmp)); } }
    }
    int c=0;
    for (char *tok=strtok(tmp," "); tok && c<maxp; tok=strtok(NULL," ")) {
        strncpy(ph[c],tok,7); ph[c][7]=0; c++;
    }
    return c;
}

/* build segments for one phone list */
static int build_segs(char ph[][8], int np, Seg *segs, int maxs) {
    int ns=0;
    for (int i=0;i<np && ns<maxs-4;i++) {
        char name[8]; int stress=-1;
        strncpy(name,ph[i],7);
        int L=strlen(name);
        if (L>0 && name[L-1]>='0'&&name[L-1]<='2'){ stress=name[L-1]-'0'; name[L-1]=0; }
        const Vow *v = vow(name); const Diph *d = diph(name);
        const char *nxt = i+1<np ? ph[i+1] : NULL;
        char nxtbase[8]={0};
        if (nxt){ strncpy(nxtbase,nxt,7); int l=strlen(nxtbase);
            if (l&&nxtbase[l-1]>='0'&&nxtbase[l-1]<='2') nxtbase[l-1]=0; }
        if (v) {
            Seg s; s.t=vowel_par(v); s.dur=stress==1?150:(stress==2?100:85); s.tr=50; s.kind=K_VOWEL; s.stress=stress;
            segs[ns++]=s;
        } else if (d) {
            int dur=stress==1?165:115;
            const Vow *a=vow(d->a), *b=vow(d->b);
            Seg s; s.kind=K_VOWEL; s.stress=stress;
            s.t=vowel_par(a); s.dur=dur*45/100; s.tr=50; segs[ns++]=s;
            s.t=vowel_par(b); s.dur=dur*55/100; s.tr=dur/2; segs[ns++]=s;
        } else if (!strcmp(name,"P")||!strcmp(name,"T")||!strcmp(name,"K")||
                   !strcmp(name,"B")||!strcmp(name,"D")||!strcmp(name,"G")) {
            int voiced = name[0]=='B'||name[0]=='D'||name[0]=='G';
            int closure = name[0]=='P'||name[0]=='K'?70:(name[0]=='T'?65:(name[0]=='G'?55:50));
            Seg s; s.t=par0(); s.kind=K_SIL; s.stress=-1; s.tr=3; s.dur=closure;
            if (voiced){ s.t.av=.18; s.t.f1=200; s.t.b1=250; s.t.f2=1000; s.t.f3=2200; }
            segs[ns++]=s;
            s.t=par0(); s.kind=K_BURST; s.dur=14; s.tr=3;
            s.t.af = name[0]=='P'?.75:(name[0]=='T'?.85:(name[0]=='K'?.8:(name[0]=='B'?.45:(name[0]=='D'?.55:.5))));
            if (name[0]=='P') s.t.ab=.45;
            else if (name[0]=='T'){s.t.a4=.55;s.t.a5=.45;}
            else if (name[0]=='K') s.t.a3=.65;
            else if (name[0]=='B') s.t.ab=.30;
            else if (name[0]=='D'){s.t.a4=.40;s.t.a5=.30;}
            else s.t.a3=.45;
            segs[ns++]=s;
            int aspir = voiced?12:35;
            if (aspir>0){ s.t=par0(); s.kind=K_ASPIR; s.dur=aspir; s.tr=12; s.t.ah=voiced?.2:.55;
                const Vow *nv = nxtbase[0]?vow(nxtbase):NULL;
                const Diph *nd = nxtbase[0]?diph(nxtbase):NULL;
                if (nv){s.t.f1=nv->f1;s.t.f2=nv->f2;s.t.f3=nv->f3;}
                else if (nd){const Vow *a=vow(nd->a);s.t.f1=a->f1;s.t.f2=a->f2;s.t.f3=a->f3;}
                segs[ns++]=s; }
        } else if (!strcmp(name,"CH")||!strcmp(name,"JH")) {
            char st[8], fr[8];
            strcpy(st, name[0]=='C'?"T":"D"); strcpy(fr, name[0]=='C'?"SH":"ZH");
            char list[2][8]; strcpy(list[0],st); strcpy(list[1],fr);
            Seg tmp[16]; int m = build_segs(list,2,tmp,16);
            for (int j=0;j<m;j++) if (tmp[j].kind!=K_ASPIR && ns<maxs) { if(j==0)tmp[j].dur=tmp[j].dur*7/10; segs[ns++]=tmp[j]; }
        } else if (!strcmp(name,"S")||!strcmp(name,"Z")||!strcmp(name,"SH")||!strcmp(name,"ZH")||
                   !strcmp(name,"F")||!strcmp(name,"V")||!strcmp(name,"TH")||!strcmp(name,"DH")||!strcmp(name,"HH")) {
            Seg s; s.t=par0(); s.kind=K_FRIC; s.stress=-1; s.tr=20;
            if (!strcmp(name,"S")){s.dur=115;s.t.af=.85;s.t.a5=.55;s.t.a6=.95;s.t.f6=4650;s.t.b6=380;}
            else if (!strcmp(name,"Z")){s.dur=90;s.t.af=.6;s.t.a5=.45;s.t.a6=.75;s.t.f6=4650;s.t.b6=380;s.t.av=.42;}
            else if (!strcmp(name,"SH")){s.dur=110;s.t.af=.8;s.t.a3=.3;s.t.a4=.8;s.t.a5=.35;s.t.f4=2600;s.t.b4=350;}
            else if (!strcmp(name,"ZH")){s.dur=85;s.t.af=.55;s.t.a3=.25;s.t.a4=.6;s.t.a5=.3;s.t.f4=2600;s.t.b4=350;s.t.av=.42;}
            else if (!strcmp(name,"F")){s.dur=95;s.t.af=.4;s.t.ab=.5;}
            else if (!strcmp(name,"V")){s.dur=75;s.t.af=.28;s.t.ab=.38;s.t.av=.48;}
            else if (!strcmp(name,"TH")){s.dur=85;s.t.af=.32;s.t.ab=.42;}
            else if (!strcmp(name,"DH")){s.dur=70;s.t.af=.22;s.t.ab=.32;s.t.av=.5;}
            else {s.dur=70;s.t.ah=.6;
                const Vow *nv = nxtbase[0]?vow(nxtbase):NULL;
                if (nv){s.t.f1=nv->f1;s.t.f2=nv->f2;s.t.f3=nv->f3;}}
            segs[ns++]=s;
        } else if (!strcmp(name,"M")||!strcmp(name,"N")||!strcmp(name,"NG")) {
            Seg s; s.t=par0(); s.kind=K_NASAL; s.stress=-1; s.tr=35;
            s.t.av=.75; s.t.f1=270; s.t.b1=250; s.t.b2=200; s.t.f3=2400; s.t.b3=250; s.t.bnz=250;
            if (!strcmp(name,"M")){s.dur=95;s.t.f2=1000;s.t.fnz=1000;}
            else if (!strcmp(name,"N")){s.dur=90;s.t.f2=1700;s.t.fnz=1900;}
            else {s.dur=100;s.t.f2=2600;s.t.fnz=2900;}
            segs[ns++]=s;
        } else if (!strcmp(name,"L")||!strcmp(name,"R")) {
            Seg s; s.t=par0(); s.kind=K_LIQUID; s.stress=-1; s.tr=45; s.dur=85;
            s.t.av=.72; s.t.b3=200;
            if (!strcmp(name,"L")){s.t.f1=380;s.t.f2=1350;s.t.f3=2600;s.t.b1=130;s.t.b2=220;}
            else {s.t.f1=460;s.t.f2=1380;s.t.f3=1720;s.t.b1=110;s.t.b2=180;}
            segs[ns++]=s;
        } else if (!strcmp(name,"W")||!strcmp(name,"Y")) {
            Seg s; s.t=par0(); s.kind=K_GLIDE; s.stress=-1; s.tr=45; s.dur=65;
            s.t.av=.8; s.t.b1=90; s.t.b2=130; s.t.b3=180;
            if (!strcmp(name,"W")){s.t.f1=300;s.t.f2=610;s.t.f3=2200;}
            else {s.t.f1=270;s.t.f2=2290;s.t.f3=3010;}
            segs[ns++]=s;
        }
    }
    return ns;
}

/* balance segment amplitudes (same LEVELs as the Python engine) */
static void balance(Seg *s) {
    switch (s->kind) {
    case K_VOWEL: set_level(&s->t,0,0.0); break;
    case K_NASAL: case K_LIQUID: set_level(&s->t,0,-4.0); break;
    case K_GLIDE: set_level(&s->t,0,-2.0); break;
    case K_FRIC:
        set_level(&s->t,2,(s->t.ab>0&&s->t.a4==0&&s->t.a5==0&&s->t.a6==0)?-10.0:-7.0);
        if (s->t.av>0) set_level(&s->t,0,-16.0);
        break;
    case K_BURST: set_level(&s->t,2,-5.0); break;
    case K_ASPIR: set_level(&s->t,1,-12.0); break;
    case K_SIL: if (s->t.av>0) set_level(&s->t,0,-20.0); break;
    }
}

/* frame parameter keys for interpolation */
static double par_get(const Par *p, int k) {
    const double *f = (const double *)p; return f[k];
}
#define NPAR ((int)(sizeof(Par)/sizeof(double)))

int hv_say(const char *text, double rate, double pitch, float *output, int max_out) {
    if (!text||!output||max_out<FRAME) return 0;
    Voice v; voice_init(&v);
    /* split into words and sentence boundaries */
    char buf[1024]; strncpy(buf,text,1023); buf[1023]=0;
    int total_frames=0, out_frames=0;
    Par prev; int have_prev=0;
    double sent_f0a=118*pitch, sent_f0b=84*pitch;
    /* first pass: per sentence */
    char *save=NULL;
    for (char *sent=strtok_r(buf,".!?",&save); sent; sent=strtok_r(NULL,".!?",&save)) {
        /* gather words */
        char *words[64]; int nw=0;
        for (char *w=strtok(sent," ,;:-"); w && nw<64; w=strtok(NULL," ,;:-")) words[nw++]=w;
        if (!nw) continue;
        /* measure sentence length for declination */
        Seg allsegs[64][48]; int nseg[64], dur[64]; long total=0;
        for (int i=0;i<nw;i++){
            char ph[24][8]; int np=word_phones(words[i],ph,24);
            nseg[i]=build_segs(ph,np,allsegs[i],48);
            if (i==nw-1 && nseg[i]>0) /* phrase-final lengthening */
                for (int j=nseg[i]-1;j>=0;j--) if (allsegs[i][j].kind==K_VOWEL||allsegs[i][j].kind==K_NASAL||allsegs[i][j].kind==K_LIQUID){
                    allsegs[i][j].dur=allsegs[i][j].dur*155/100; break; }
            dur[i]=0; for (int j=0;j<nseg[i];j++){ balance(&allsegs[i][j]); dur[i]+=allsegs[i][j].dur; }
            total+=dur[i];
        }
        long t=0;
        for (int i=0;i<nw;i++){
            double b0=sent_f0a+(sent_f0b-sent_f0a)*(total?((double)t/total):0);
            t+=dur[i];
            double b1=sent_f0a+(sent_f0b-sent_f0a)*(total?((double)t/total):0);
            int finalW = (i==nw-1);
            long acc=0;
            for (int j=0;j<nseg[i];j++){
                Seg *s=&allsegs[i][j];
                double sdur=s->dur/rate, str=s->tr<sdur*.6?s->tr:sdur*.6;
                int nf=(int)(sdur/5+.5); if(nf<1)nf=1;
                int nt=(int)(str/5+.5);
                for (int k=0;k<nf;k++){
                    Par fr=s->t;
                    if (k<nt&&have_prev){
                        double mix=(k+1.0)/(nt+1.0);
                        for (int q=0;q<NPAR;q++){ double p0=par_get(&prev,q),p1=par_get(&s->t,q);
                            ((double*)&fr)[q]=p0+(p1-p0)*mix; }
                    }
                    double frac=dur[i]?(double)acc/dur[i]:0;
                    double f=b0+(b1-b0)*frac;
                    if (s->stress==1&&s->kind==K_VOWEL) f*=1+.14*sin(M_PI*(frac*1.4<1?frac*1.4:1));
                    if (finalW&&s->kind==K_VOWEL) f*=1-.13*frac*frac;
                    fr.f0=f;
                    if (out_frames*FRAME>=max_out-FRAME) goto done;
                    v.p=fr; synth_frame(&v,output+(long)out_frames*FRAME);
                    out_frames++; total_frames++;
                    acc+=5;
                }
                prev=s->t; have_prev=1;
            }
        }
        /* sentence-end pause */
        int pf=(int)(340/rate/5+.5);
        for (int k=0;k<pf;k++){
            if (out_frames*FRAME>=max_out-FRAME) goto done;
            Par z=par0(); z.f0=95*pitch; v.p=z; synth_frame(&v,output+(long)out_frames*FRAME);
            out_frames++;
        }
        have_prev=0;
    }
done:
    /* normalize + edge fades */
    int n=out_frames*FRAME;
    float peak=1e-6f;
    for (int i=0;i<n;i++){ float a=fabsf(output[i]); if(a>peak)peak=a; }
    float g=.89f/peak;
    for (int i=0;i<n;i++) output[i]*=g;
    int nf2=80<n?80:n;
    for (int i=0;i<nf2;i++){ float w=(float)i/nf2; output[i]*=w; output[n-1-i]*=w; }
    return n;
}
