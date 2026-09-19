/* tts.js - compact front end for klatt80.js: phones -> frames. MIT. */
"use strict";
const VOWELS = {IY:[270,2290,3010,60,90,150],IH:[390,1990,2550,65,100,160],EH:[530,1840,2480,70,100,160],
AE:[660,1720,2410,80,110,170],AA:[730,1090,2440,85,110,170],AO:[570,840,2410,80,110,170],
UH:[440,1020,2240,70,100,160],UW:[300,870,2240,60,95,150],AH:[640,1190,2390,80,110,170],
AX:[500,1500,2500,75,110,170],ER:[490,1350,1690,70,100,140]};
const DIPH = {EY:['EH','IY'],AY:['AA','IY'],OY:['AO','IY'],OW:['AX','UH'],AW:['AA','UH']};
const STOPS = {P:[70,{af:.75,ab:.45},35,.55,0],T:[65,{af:.85,a4:.55,a5:.45},35,.60,0],K:[70,{af:.80,a3:.65},35,.55,0],
B:[55,{af:.45,ab:.30},12,.20,1],D:[50,{af:.55,a4:.40,a5:.30},12,.20,1],G:[55,{af:.50,a3:.45},12,.20,1]};
const FRICS = {S:[115,{af:.85,a5:.55,a6:.95,f6:4650,b6:380}],Z:[90,{af:.60,a5:.45,a6:.75,f6:4650,b6:380,av:.42}],
SH:[110,{af:.80,a3:.30,a4:.80,a5:.35,f4:2600,b4:350}],ZH:[85,{af:.55,a3:.25,a4:.60,a5:.30,f4:2600,b4:350,av:.42}],
F:[95,{af:.40,ab:.50}],V:[75,{af:.28,ab:.38,av:.48}],TH:[85,{af:.32,ab:.42}],DH:[70,{af:.22,ab:.32,av:.50}],HH:[70,{ah:.60}]};
const NASALS = {M:[95,1000,1000],N:[90,1700,1900],NG:[100,2600,2900]};
const LIQUIDS = {L:[85,380,1350,2600,130,220],R:[85,460,1380,1720,110,180]};
const GLIDES = {W:[65,300,610,2200],Y:[65,270,2290,3010]};
const AFFR = {CH:['T','SH'],JH:['D','ZH']};
const LTS = {a:'AH',b:'B',c:'K',d:'D',e:'EH',f:'F',g:'G',h:'HH',i:'IH',j:'JH',k:'K',l:'L',m:'M',n:'N',
o:'AA',p:'P',q:'K',r:'R',s:'S',t:'T',u:'AH',v:'V',w:'W',x:'K S',y:'Y',z:'Z'};
/* small built-in lexicon (narration vocabulary + common words); ARPAbet with stress */
const DICT = {
it:'IH1 T',begins:'B IH0 G IH1 N Z',with:'W IH1 DH',one:'W AH1 N',of:'AH1 V',you:'Y UW1',us:'AH1 S',
made:'M EY1 D',pictures:'P IH1 K CH ER0 Z',the:'DH AH0',world:'W ER1 L D',all:'AO1 L',looked:'L UH1 K T',
up:'AH1 P',and:'AH0 N D',still:'S T IH1 L',was:'W AH1 Z',humanico:'HH UW0 M AA0 N IY1 K OW0',for:'F AO1 R',
humanity:'HH Y UW0 M AE1 N AH0 T IY0',i:'AY1',did:'D IH1 D',not:'N AA1 T',build:'B IH1 L D',any:'EH1 N IY0',
this:'DH IH1 S',cathedrals:'K AH0 TH IY1 D R AH0 L Z',wire:'W AY1 ER0',raised:'R EY1 Z D',by:'B AY1',hand:'HH AE1 N D',
your:'Y AO1 R',hands:'HH AE1 N D Z',everyone:'EH1 V R IY0 W AH2 N',can:'K AE1 N',do:'D UW1',again:'AH0 G EH1 N',
people:'P IY1 P AH0 L',who:'HH UW1',a:'AH0',voice:'V OY1 S',in:'IH1 N',every:'EH1 V R IY0',room:'R UW1 M',
technology:'T EH0 K N AA1 L AH0 JH IY0',is:'IH1 Z',them:'DH EH1 M',am:'AE1 M',only:'OW1 N L IY0',are:'AA1 R',
always:'AO1 L W EY0 Z',built:'B IH1 L T',bridges:'B R IH1 JH AH0 Z',laid:'L EY1 D',grids:'G R IH1 D Z',
cities:'S IH1 T IY0 Z',touched:'T AH1 CH T',sky:'S K AY1',whatever:'W AH0 T EH1 V ER0',comes:'K AH1 M Z',
next:'N EH1 K S T',make:'M EY1 K',beautiful:'B Y UW1 T AH0 F AH0 L',we:'W IY1',to:'T UW1',too:'T UW1',
love:'L AH1 V',bots:'B AA1 T S',draw:'D R AO1',perfect:'P ER1 F AH0 K T',lines:'L AY1 N Z',wobble:'W AA1 B AH0 L',
beautifully:'B Y UW1 T AH0 F AH0 L IY0',people2:'P IY1 P AH0 L',hello:'HH AH0 L OW1',hi:'HH AY1',
yes:'Y EH1 S',no:'N OW1',speech:'S P IY1 CH',synth:'S IH1 N TH',machine:'M AH0 SH IY1 N',human:'HH Y UW1 M AH0 N'};
function wordPhones(w){
  w = w.toLowerCase().replace(/[^a-z']/g,'');
  if (DICT[w]) return DICT[w].split(' ').map(p=>{const m=p.match(/^([A-Z]+)([012])?$/);return [m[1], m[2]?+m[2]:0];});
  const out=[]; for (const ch of w) if (LTS[ch]) for (const p of LTS[ch].split(' ')) out.push([p,-1]);
  return out;
}
function vowelTgt(v, av){ const [f1,f2,f3,b1,b2,b3]=VOWELS[v]; return {av,f1,f2,f3,b1,b2,b3}; }
function segments(phseq){
  const segs=[]; const n=phseq.length;
  for (let i=0;i<n;i++){
    const [ph,stress]=phseq[i]; const nxt=i+1<n?phseq[i+1][0]:null; const stressed=stress===1;
    if (VOWELS[ph]) segs.push({t:vowelTgt(ph,.92),dur:stressed?150:(stress===2?100:85),tr:50,kind:'vowel',stress});
    else if (DIPH[ph]){ const [a,b]=DIPH[ph]; const d=stressed?165:115;
      segs.push({t:vowelTgt(a,.92),dur:d*.45|0,tr:50,kind:'vowel',stress});
      segs.push({t:vowelTgt(b,.92),dur:d*.55|0,tr:d*.5|0,kind:'vowel',stress}); }
    else if (STOPS[ph]){ const [cl,burst,asp,ahv,voiced]=STOPS[ph];
      const clo = voiced?{av:.18,f1:200,b1:250,f2:1000,f3:2200}:{};
      segs.push({t:clo,dur:cl,tr:3,kind:'sil'}); segs.push({t:Object.assign({},burst),dur:14,tr:3,kind:'burst'});
      if (asp>0){ const ah={ah:ahv}; if (nxt&&VOWELS[nxt]){const v=vowelTgt(nxt);ah.f1=v.f1;ah.f2=v.f2;ah.f3=v.f3;}
        segs.push({t:ah,dur:asp,tr:12,kind:'aspir'}); } }
    else if (AFFR[ph]){ const [st,fr]=AFFR[ph];
      const sub=segments([[st,-1],[fr,-1]]).filter(s=>s.kind!=='aspir'); sub[0].dur=sub[0].dur*.7|0; segs.push(...sub); }
    else if (FRICS[ph]){ const [dur,prm]=FRICS[ph]; const t=Object.assign({},prm);
      if (ph==='HH'&&nxt&&VOWELS[nxt]){const v=vowelTgt(nxt);t.f1=v.f1;t.f2=v.f2;t.f3=v.f3;}
      segs.push({t,dur,tr:20,kind:'fric'}); }
    else if (NASALS[ph]){ const [dur,f2,fnz]=NASALS[ph];
      segs.push({t:{av:.75,f1:270,b1:250,f2,b2:200,f3:2400,b3:250,fnz,bnz:250},dur,tr:35,kind:'nasal'}); }
    else if (LIQUIDS[ph]){ const [dur,f1,f2,f3,b1,b2]=LIQUIDS[ph];
      segs.push({t:{av:.72,f1,f2,f3,b1,b2,b3:200},dur,tr:45,kind:'liquid'}); }
    else if (GLIDES[ph]){ const [dur,f1,f2,f3]=GLIDES[ph];
      segs.push({t:{av:.80,f1,f2,f3,b1:90,b2:130,b3:180},dur,tr:45,kind:'glide'}); }
  }
  return segs;
}
/* calibration: reuse Python-computed source scales per kind via steady-state unit RMS
   (computed once here at load, same algorithm as Python). */
const LEVEL = {vowel:0,nasal:-4,liquid:-4,glide:-2,fric:-7,fric_weak:-10,burst:-5,aspir:-12,voicebar:-20,fric_voice:-16};
const _rc = {};
function unitRMS(params, source){
  const key = source+JSON.stringify(params);
  if (_rc[key]) return _rc[key];
  const v = new Voice(); Object.assign(v, params);
  v.av=0;v.ah=0;v.af=0; v[source]=1; if(!params.f0)v.f0=110;
  let acc=0,cnt=0;
  for(let f=0;f<70;f++){ const out=v.frame(50); if(f>=30) for(let i=0;i<50;i++){acc+=out[i]*out[i];cnt++;} }
  return _rc[key]=Math.sqrt(acc/cnt)+1e-9;
}
let _vref=null;
function vref(){ if(!_vref)_vref=unitRMS({f1:730,b1:85,f2:1090,b2:110,f3:2440,b3:170},'av'); return _vref; }
function balance(t, kind){
  t=Object.assign({},t);
  const set=(src,k)=>{const want=vref()*Math.pow(10,LEVEL[k]/20);t[src]=want/unitRMS(t,src);};
  if(kind==='vowel')set('av','vowel');
  else if(kind==='nasal'||kind==='liquid')set('av',kind);
  else if(kind==='glide')set('av','glide');
  else if(kind==='fric'){set('af',(t.ab&&!t.a4&&!t.a5&&!t.a6)?'fric_weak':'fric');if(t.av)set('av','fric_voice');}
  else if(kind==='burst')set('af','burst');
  else if(kind==='aspir')set('ah','aspir');
  else if(kind==='voicebar')set('av','voicebar');
  return t;
}
function framesForText(text, rate=0.85, pitch=1.0){
  const tokens = text.replace(/[\u2014\u2013]/g,' - ').match(/[A-Za-z']+|[.,!?;:\-]/g)||[];
  const words=[]; 
  for(const tk of tokens){
    if(/^[.,!?;:\-]$/.test(tk)){ if(words.length){words[words.length-1].pause={'.':340,'!':340,'?':340,',':200,';':220,':':220,'-':160}[tk]||200; if('.!?'.includes(tk))words.push({sentenceEnd:true});} }
    else {const ph=wordPhones(tk); if(ph.length)words.push({w:tk,ph,pause:0});}
  }
  const frames=[]; let prev=null;
  const sentences=[]; let cur=[];
  for(const wd of words){ if(wd.sentenceEnd){sentences.push(cur);cur=[];} else cur.push(wd); }
  if(cur.length)sentences.push(cur);
  for(const sent of sentences){
    let total=0;
    for(const wd of sent){ wd.segs=segments(wd.ph); wd.dur=wd.segs.reduce((a,s)=>a+s.dur,0); total+=wd.dur+(wd.pause||0); }
    let t=0; const f0a=118*pitch, f0b=84*pitch;
    for(let wi=0;wi<sent.length;wi++){
      const wd=sent[wi]; const finalW=wi===sent.length-1;
      if(finalW){ for(let i=wd.segs.length-1;i>=0;i--){ if(['vowel','nasal','liquid'].includes(wd.segs[i].kind)){wd.segs[i].dur=wd.segs[i].dur*1.55|0;break;} } wd.dur=wd.segs.reduce((a,s)=>a+s.dur,0); }
      const b0=f0a+(f0b-f0a)*(t/Math.max(1,total)); t+=wd.dur; const b1=f0a+(f0b-f0a)*(t/Math.max(1,total)); t+=(wd.pause||0);
      let acc=0;
      for(const seg of wd.segs){
        const dur=seg.dur/rate, tr=Math.min(seg.tr,dur*.6);
        const nf=Math.max(1,Math.round(dur/5)), nt=Math.max(0,Math.round(tr/5));
        let tp=seg.t;
        if(seg.kind==='vowel')tp=balance(seg.t,'vowel');
        else if(['nasal','liquid','glide','fric','burst','aspir'].includes(seg.kind))tp=balance(seg.t,seg.kind);
        else if(seg.kind==='sil'&&seg.t.av)tp=balance(seg.t,'voicebar');
        for(let k=0;k<nf;k++){
          const fr=Object.assign({},tp);
          if(k<nt&&prev){const mix=(k+1)/(nt+1);for(const key of new Set([...Object.keys(prev),...Object.keys(tp)]))fr[key]=(prev[key]||0)+((tp[key]||0)-(prev[key]||0))*mix;}
          const frac=(acc/5)/Math.max(1,wd.dur);
          let f=b0+(b1-b0)*frac;
          if(seg.stress===1&&seg.kind==='vowel')f*=1+.14*Math.sin(Math.PI*Math.min(1,frac*1.4));
          if(finalW&&seg.kind==='vowel')f*=1-.13*frac*frac;
          fr.f0=f; frames.push(fr); acc+=5;
        }
        prev=tp;
      }
      if(wd.pause){for(let k=0;k<Math.round(wd.pause/rate/5);k++)frames.push({f0:95*pitch});prev=null;}
    }
  }
  return frames;
}
function renderPCM(text, rate, pitch){
  const pcm = synthesize(framesForText(text, rate, pitch));
  let peak=1e-6; for(const x of pcm)peak=Math.max(peak,Math.abs(x));
  const g=.89/peak, nf=80;
  for(let i=0;i<pcm.length;i++)pcm[i]*=g;
  for(let i=0;i<Math.min(nf,pcm.length);i++){pcm[i]*=i/nf;pcm[pcm.length-1-i]*=i/nf;}
  return pcm;
  }
