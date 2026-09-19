/* klatt80.js - JS port of the Humanico Voice Klatt-1980 formant synth engine.
   Original code from the published algorithm (Klatt 1980, JASA 67(3)). MIT. */
"use strict";
const FS = 10000;
function resCoeffs(f, bw) {
  const r = Math.exp(-Math.PI * bw / FS);
  const c = -(r * r), b = 2 * r * Math.cos(2 * Math.PI * f / FS);
  return [1 - b - c, b, c];
}
function antiCoeffs(f, bw) {
  const [a, b, c] = resCoeffs(f, bw);
  const a0 = 1 / a;
  return [a0, -b * a0, -c * a0];
}
class Res { constructor() { this.y1 = 0; this.y2 = 0; } run(x, k) { const y = k[0]*x + k[1]*this.y1 + k[2]*this.y2; this.y2 = this.y1; this.y1 = y; return y; } }
class Anti { constructor() { this.x1 = 0; this.x2 = 0; } run(x, k) { const y = k[0]*x + k[1]*this.x1 + k[2]*this.x2; this.x2 = this.x1; this.x1 = x; return y; } }
class Voice {
  constructor() {
    this.f0 = 100; this.av = 0; this.ah = 0; this.af = 0;
    this.f1 = 500; this.b1 = 60; this.f2 = 1500; this.b2 = 90; this.f3 = 2500; this.b3 = 150;
    this.f4 = 3900; this.b4 = 250; this.f5 = 4500; this.b5 = 300; this.f6 = 4800; this.b6 = 400;
    this.fnp = 270; this.bnp = 270; this.fnz = 270; this.bnz = 270;
    this.a2 = 0; this.a3 = 0; this.a4 = 0; this.a5 = 0; this.a6 = 0; this.ab = 0;
    this.b2f = 250; this.b3f = 300; this.b4f = 400; this.b5f = 500; this.b6f = 600;
    this.bgl = 1200;
    this._rnp = new Res(); this._rnz = new Anti();
    this._r = [new Res(), new Res(), new Res(), new Res(), new Res()];
    this._p = [new Res(), new Res(), new Res(), new Res(), new Res()];
    this._rg = new Res();
    this._t0 = 1; this._prevY = 0;
    this._seed = 1234;
  }
  _noise() { // deterministic LCG so renders are reproducible
    this._seed = (this._seed * 1103515245 + 12345) & 0x7fffffff;
    return this._seed / 0x3fffffff - 1.0;
  }
  frame(n) {
    const rg = resCoeffs(0, this.bgl);
    const rnp = resCoeffs(this.fnp, this.bnp);
    const rnz = antiCoeffs(this.fnz, this.bnz);
    const rc = [resCoeffs(this.f1, this.b1), resCoeffs(this.f2, this.b2),
                resCoeffs(this.f3, this.b3), resCoeffs(this.f4, this.b4), resCoeffs(this.f5, this.b5)];
    const rp = [resCoeffs(this.f2, this.b2f), resCoeffs(this.f3, this.b3f),
                resCoeffs(this.f4, this.b4f), resCoeffs(this.f5, this.b5f), resCoeffs(this.f6, this.b6f)];
    const pa = [this.a2, this.a3, this.a4, this.a5, this.a6];
    const out = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      this._t0 += 1;
      const period = FS / Math.max(this.f0, 20);
      let glot = 0;
      if (this._t0 >= period) { this._t0 -= period; glot = 1; }
      const voice = this.av > 0 ? this._rg.run(glot, rg) * this.av : 0;
      const noise = this._noise();
      let x = this._rnp.run(voice + noise * this.ah, rnp);
      x = this._rnz.run(x, rnz);
      for (let j = 0; j < 5; j++) x = this._r[j].run(x, rc[j]);
      const fr = noise * this.af;
      let par = this.ab * fr;
      for (let j = 0; j < 5; j++) if (pa[j] > 0) par += this._p[j].run(fr, rp[j]) * pa[j];
      const y = x + par;
      out[i] = y - this._prevY;
      this._prevY = y;
    }
    return out;
  }
}
function synthesize(frames) {
  const v = new Voice();
  const chunks = [];
  for (const fr of frames) { Object.assign(v, fr); chunks.push(v.frame(50)); }
  const total = chunks.reduce((a, c) => a + c.length, 0);
  const pcm = new Float32Array(total);
  let o = 0;
  for (const c of chunks) { pcm.set(c, o); o += c.length; }
  return pcm;
}
