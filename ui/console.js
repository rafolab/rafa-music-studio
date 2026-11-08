/* RAFA LOOP DECK Model 5 – virtuálna konzola (Claude)
   Kreslí samostatné prvky na pevnú scénu 1672 × 941 a prepája ich s motorom cez rafa-bridge.js.
   Polohy prvkov sú v controls.js (vygenerované z pôvodného obrázka). */
(() => {
  "use strict";
  const C = window.RAFA_CONTROLS;
  const W = C.stage.width, H = C.stage.height;
  const $ = (s) => document.querySelector(s);
  const stage = $("#stage"), spritesEl = $("#sprites"), hitEl = $("#hit");
  const SVGNS = "http://www.w3.org/2000/svg";
  let scale = 1;

  // ------------------------------------------------------------------ mierka scény
  function fit () {
    scale = Math.min(innerWidth / W, innerHeight / H);
    const x = (innerWidth - W * scale) / 2, y = (innerHeight - H * scale) / 2;
    stage.style.transform = `translate(${x}px, ${y}px) scale(${scale})`;
  }
  addEventListener("resize", fit); fit();

  // ------------------------------------------------------------------ stav
  let st = null;                 // posledný stav z motora
  let tracks = [], viewLength = 30;
  let localSel = null;           // vybraná vrstva, kým motor nepozná selectTrack
  let page = 0;                  // stránka kanálov (stopy 1–4, 5–8…)
  const hold = {};               // prvky, ktoré práve ťaháme (ignorujeme ich stav z motora)
  const api = () => (st && st.apiVersion) || Rafa.apiVersion || 3;
  const has = (v) => api() >= v;
  const rec = () => !!(st && st.transport && st.transport.recording);

  function send (cmd, args) {
    if (window.Rafa) Rafa.send(cmd, args || {});
  }
  function needs (v, what) {
    if (has(v)) return true;
    flashValue(what + " – príde s novou verziou motora");
    return false;
  }

  const selectedId = () => (st && st.selectedTrackId) || localSel || (tracks[0] && tracks[0].id) || null;
  const selectedTrack = () => tracks.find(t => t.id === selectedId()) || null;
  const trackAt = (i) => tracks[page * 4 + i] || null;

  // ------------------------------------------------------------------ pomocné kreslenie
  function el (tag, cls, parent, style) {
    const e = document.createElement(tag); if (cls) e.className = cls;
    if (style) Object.assign(e.style, style); if (parent) parent.appendChild(e); return e;
  }
  function svg (tag, attrs, parent) {
    const e = document.createElementNS(SVGNS, tag);
    for (const k in attrs) e.setAttribute(k, attrs[k]); if (parent) parent.appendChild(e); return e;
  }
  const px = (v) => v + "px";
  const clamp = (v, a, b) => Math.min(b, Math.max(a, v));

  // ------------------------------------------------------------------ displej (matrix3d do skla)
  (function placeDisplay () {
    const d = $("#display"), w = d.offsetWidth || 472, h = d.offsetHeight || 220;
    const [p0, p1, p2, p3] = C.display.quad;
    // homografia zo štvorca (0,0)-(1,1) na štvoruholník
    const dx1 = p1[0] - p2[0], dx2 = p3[0] - p2[0], dy1 = p1[1] - p2[1], dy2 = p3[1] - p2[1];
    const sx = p0[0] - p1[0] + p2[0] - p3[0], sy = p0[1] - p1[1] + p2[1] - p3[1];
    const den = dx1 * dy2 - dx2 * dy1;
    const g = (sx * dy2 - dx2 * sy) / den, hh = (dx1 * sy - sx * dy1) / den;
    const a = p1[0] - p0[0] + g * p1[0], b = p3[0] - p0[0] + hh * p3[0], c = p0[0];
    const dd = p1[1] - p0[1] + g * p1[1], e = p3[1] - p0[1] + hh * p3[1], f = p0[1];
    // zo štvorca na obdĺžnik displeja: x/w, y/h
    const m = [a / w, dd / w, 0, g / w,  b / h, e / h, 0, hh / h,  0, 0, 1, 0,  c, f, 0, 1];
    d.style.transform = `matrix3d(${m.join(",")})`;
  })();

  // ------------------------------------------------------------------ hodnota na displeji
  const dValue = el("div", "d-value", $("#display"));
  let valueTimer = 0;
  function flashValue (text, ms) {
    dValue.textContent = text; dValue.classList.add("show");
    clearTimeout(valueTimer); valueTimer = setTimeout(() => dValue.classList.remove("show"), ms || 1100);
  }

  // ------------------------------------------------------------------ TLAČIDLÁ
  const BTN = {};
  for (const [id, b] of Object.entries(C.buttons)) {
    const wrap = el("div", "btn k-" + b.kind, spritesEl, { left: px(b.x), top: px(b.y), width: px(b.w), height: px(b.h) });
    el("div", "glow", wrap);
    const off = el("img", "spr", wrap, { left: 0, top: 0, width: px(b.w), height: px(b.h) }); off.src = b.off;
    const on = el("img", "spr on", wrap, { left: 0, top: 0, width: px(b.w), height: px(b.h) }); on.src = b.on;
    const hit = el("div", "hit rect", hitEl, { left: px(b.x + 4), top: px(b.y + 4), width: px(b.w - 9), height: px(b.h - 13) });
    BTN[id] = { wrap, hit };
    hit.addEventListener("pointerdown", (ev) => {
      if (ev.button !== 0 || hit.classList.contains("disabled")) return;
      wrap.classList.add("pressed"); hit.setPointerCapture(ev.pointerId);
    });
    const up = () => wrap.classList.remove("pressed");
    hit.addEventListener("pointerup", (ev) => { if (wrap.classList.contains("pressed")) { up(); press(id); } });
    hit.addEventListener("pointercancel", up);
  }
  const lit = (id, on) => BTN[id] && BTN[id].wrap.classList.toggle("lit", !!on);
  const armed = (id, on) => BTN[id] && BTN[id].wrap.classList.toggle("armed", !!on);
  const flash = (id) => { lit(id, true); setTimeout(() => render(), 220); flashing[id] = performance.now() + 220; };
  const flashing = {};

  function press (id) {
    if (id.startsWith("layer") && id !== "layerAdd") return selectLayer(+id.slice(5) - 1);
    if (id.startsWith("mute")) { const t = trackAt(+id.slice(4) - 1); if (t) send("trackMute", { trackId: t.id, muted: !t.muted }); return; }
    if (id.startsWith("solo")) { const t = trackAt(+id.slice(4) - 1); if (t) send("trackSolo", { trackId: t.id, solo: !t.solo }); return; }
    switch (id) {
      case "rewind": send("rewind"); flash(id); break;
      case "stop": send("stop"); flash(id); break;
      case "play": send("togglePlay"); break;
      case "rec": send("record"); break;
      case "loopIn": send("loopIn"); flash(id); break;
      case "loopOut": send("loopOut"); flash(id); break;
      case "loop": send("toggleLoop"); break;
      case "metroOn": send("setMetronome", { on: !(st && st.metronome && st.metronome.on) }); break;
      case "layerAdd": if (needs(4, "Pridať stopu")) { send("trackAdd"); flash(id); } break;
      case "project": case "tracks": case "effects": case "settings": toggleMenu(id); break;
    }
  }

  function selectLayer (i) {
    const t = trackAt(i);
    if (!t) { flashValue("Kanál " + (page * 4 + i + 1) + " je prázdny – nahraj vrstvu cez REC"); return; }
    localSel = t.id;
    if (has(4)) send("selectTrack", { trackId: t.id });
    flashValue("Vybraná vrstva: " + t.name);
    render();
  }

  // ------------------------------------------------------------------ LED
  const LED = {};
  for (const [id, l] of Object.entries(C.leds)) {
    const wrap = el("div", "led", spritesEl, { position: "absolute", left: px(l.x), top: px(l.y), width: px(l.w), height: px(l.h) });
    el("div", "halo", wrap, { inset: "8px" });
    const on = el("img", "spr on", wrap, { left: 0, top: 0, width: px(l.w), height: px(l.h) }); on.src = l.on;
    LED[id] = wrap;
  }

  // ------------------------------------------------------------------ GOMBÍKY
  const marks = $("#knobMarks");
  const dbTxt = (v) => v <= -59.9 ? "−∞ dB" : (v > 0 ? "+" : "") + v.toFixed(1) + " dB";
  const KNOB = {
    ch1: chKnob(0), ch2: chKnob(1), ch3: chKnob(2), ch4: chKnob(3),
    metro: { min: 0, max: 1, def: 0.6, step: 0.05, name: "METRONÓM",
      get: () => st && st.metronome ? st.metronome.volume : 0.6, set: (v) => send("setMetronomeVolume", { volume: v }),
      fmt: (v) => Math.round(v * 100) + " %" },
    tempo: { min: 40, max: 240, def: 120, step: 1, fine: 0.1, name: "TEMPO", lockRec: true,
      get: () => st && st.tempo ? st.tempo.bpm : 120, set: (v) => send("setTempo", { bpm: Math.round(v * 10) / 10 }),
      fmt: (v) => v.toFixed(1) + " BPM" },
    input: { min: -24, max: 24, def: 0, step: 1, v: 4, name: "INPUT",
      get: () => st && typeof st.inputGainDb === "number" ? st.inputGainDb : 0, set: (v) => send("setInputGain", { db: Math.round(v * 2) / 2 }),
      fmt: (v) => (v > 0 ? "+" : "") + v.toFixed(1) + " dB" },
    // MONITOR: odposluch gitary cez počítač – zapnutý / vypnutý (dva polohy, hlasitosť je na kombe)
    monitor: { min: 0, max: 1, def: 0, step: 1, name: "MONITOR", snap: true,
      get: () => st && st.monitoring ? 1 : 0,
      set: (v) => send("setMonitoring", { on: v >= 0.5 }),
      fmt: (v) => v >= 0.5 ? "zapnutý" : "vypnutý" },
    volume: { min: -60, max: 6, def: 0, step: 1, name: "VOLUME",
      get: () => st && st.mixer ? Math.max(-60, st.mixer.masterDb) : 0, set: (v) => send("masterVolume", { db: v <= -59.5 ? -100 : Math.round(v * 2) / 2 }),
      fmt: dbTxt },
  };
  function chKnob (i) {
    return { min: -1, max: 1, def: 0, step: 0.05, name: "PAN " + (i + 1), track: i,
      get: () => { const t = trackAt(i); return t ? t.pan || 0 : 0; },
      set: (v) => { const t = trackAt(i); if (t) send("trackPan", { trackId: t.id, pan: Math.round(v * 100) / 100 }); },
      fmt: (v) => Math.abs(v) < 0.02 ? "stred" : (v < 0 ? "L " : "R ") + Math.round(Math.abs(v) * 100) };
  }
  const K = {};
  for (const [id, k] of Object.entries(C.knobs)) {
    const cfg = KNOB[id] || null;
    const g = svg("g", {}, marks);
    // ryska ukazovateľa: tieň + tmavá drážka + svetlá hrana
    const sw = Math.max(1.6, k.rx / 12);
    const sh = svg("line", { stroke: "rgba(255,255,255,.55)", "stroke-width": sw * 0.55, "stroke-linecap": "round" }, g);
    const ln = svg("line", { stroke: "#191512", "stroke-width": sw, "stroke-linecap": "round" }, g);
    K[id] = { k, cfg, ln, sh, angle: 0, local: null, until: 0 };
    const isJog = id === "jog";
    const rx = k.rx * (isJog ? 1.15 : 1.45), ry = k.ry * (isJog ? 1.25 : 1.7);
    const hit = el("div", "hit " + (isJog ? "jog" : "knob"), hitEl, { left: px(k.cx - rx), top: px(k.cy - ry * 0.75), width: px(rx * 2), height: px(ry * 1.75) });
    K[id].hit = hit;
    if (isJog) setupJog(hit, K[id]); else setupKnob(hit, K[id]);
  }
  function drawKnob (o, angleDeg) {
    o.angle = angleDeg;
    const k = o.k, a = angleDeg * Math.PI / 180, s = Math.sin(a), c = Math.cos(a);
    const r0 = o.k === C.knobs.jog ? 0.55 : 0.32, r1 = 0.86;
    const pd = o.pressed ? 1.2 : 0;
    const x0 = k.cx + s * k.rx * r0, y0 = k.cy - c * k.ry * r0 + pd, x1 = k.cx + s * k.rx * r1, y1 = k.cy - c * k.ry * r1 + pd;
    o.ln.setAttribute("x1", x0); o.ln.setAttribute("y1", y0); o.ln.setAttribute("x2", x1); o.ln.setAttribute("y2", y1);
    o.sh.setAttribute("x1", x0 + 0.6); o.sh.setAttribute("y1", y0 + 0.9); o.sh.setAttribute("x2", x1 + 0.6); o.sh.setAttribute("y2", y1 + 0.9);
  }
  const knobAngle = (cfg, v) => cfg.snap ? (v >= 0.5 ? 60 : -135) : -135 + 270 * (clamp(v, cfg.min, cfg.max) - cfg.min) / (cfg.max - cfg.min);
  function knobValue (o) { return o.local !== null && performance.now() < o.until ? o.local : o.cfg.get(); }
  function knobDisabled (o) {
    const c = o.cfg; if (!c) return true;
    if (c.v && !has(c.v)) return true;
    if (c.track !== undefined && !trackAt(c.track)) return true;
    if (c.lockRec && rec()) return true;
    return false;
  }
  function setupKnob (hit, o) {
    let startY = 0, startV = 0, lastSend = 0;
    const apply = (v, final) => {
      const c = o.cfg; v = clamp(v, c.min, c.max); if (c.snap) v = v >= (c.min + c.max) / 2 ? c.max : c.min; o.local = v; o.until = performance.now() + 600;
      drawKnob(o, knobAngle(c, v)); flashValue(c.name + "  " + c.fmt(v));
      const now = performance.now(); if (final || now - lastSend > 45) { c.set(v); lastSend = now; }
    };
    hit.addEventListener("pointerdown", (ev) => {
      if (ev.button !== 0) return;
      if (knobDisabled(o)) { explainDisabled(o); return; }
      hit.setPointerCapture(ev.pointerId); startY = ev.clientY; startV = knobValue(o); hold.knob = o;
    });
    hit.addEventListener("pointermove", (ev) => {
      if (hold.knob !== o) return;
      const c = o.cfg, range = c.max - c.min, dy = (startY - ev.clientY) / scale;
      apply(o.cfg.snap ? (dy > 12 ? 1 : dy < -12 ? 0 : startV) : startV + dy / (ev.shiftKey ? 900 : 180) * range, false);
    });
    const end = () => { if (hold.knob === o) { hold.knob = null; apply(o.local ?? knobValue(o), true); } };
    hit.addEventListener("pointerup", end); hit.addEventListener("pointercancel", end);
    hit.addEventListener("wheel", (ev) => {
      ev.preventDefault(); if (knobDisabled(o)) return explainDisabled(o);
      const c = o.cfg, st_ = ev.shiftKey && c.fine ? c.fine : c.step;
      apply(knobValue(o) + (ev.deltaY < 0 ? st_ : -st_), true);
    }, { passive: false });
    hit.addEventListener("dblclick", () => { if (!knobDisabled(o)) apply(o.cfg.def, true); });
    if (o.cfg && o.cfg.snap) hit.addEventListener("click", () => { if (!knobDisabled(o) && !hold.knob) apply(knobValue(o) >= 0.5 ? 0 : 1, true); });
  }
  function explainDisabled (o) {
    const c = o.cfg; if (!c) return;
    if (c.v && !has(c.v)) flashValue(c.name + " – príde s novou verziou motora");
    else if (c.track !== undefined) flashValue("Kanál je prázdny");
    else if (c.lockRec) flashValue(c.name + " sa počas nahrávania nemení");
  }

  // veľký gombík DATA / SELECT (JOG): bez dorazu, 24 zárezov na otáčku
  let jogAcc = 0;
  function setupJog (hit, o) {
    let lastA = null, rest = 0;
    const ang = (ev) => { const r = hit.getBoundingClientRect(); return Math.atan2(ev.clientX - (r.left + r.width / 2), -(ev.clientY - (r.top + r.height / 2))) * 180 / Math.PI; };
    let moved = 0;
    hit.addEventListener("pointerdown", (ev) => { if (ev.button !== 0) return; hit.setPointerCapture(ev.pointerId); lastA = ang(ev); rest = 0; moved = 0; hold.jog = true; hit.style.cursor = "grabbing"; hit.classList.add("active"); o.pressed = true; drawKnob(o, jogAcc); });
    hit.addEventListener("pointermove", (ev) => {
      if (!hold.jog) return; let a = ang(ev), d = a - lastA; if (d > 180) d -= 360; if (d < -180) d += 360; lastA = a;
      jogAcc += d; moved += Math.abs(d); drawKnob(o, jogAcc); rest += d;
      while (rest >= 15) { rest -= 15; jogStep(+1, ev.shiftKey); }
      while (rest <= -15) { rest += 15; jogStep(-1, ev.shiftKey); }
    });
    const end = (ev) => {
      if (!hold.jog) return; hold.jog = false; hit.style.cursor = ""; o.pressed = false; drawKnob(o, jogAcc);
      setTimeout(() => hit.classList.remove("active"), 250);
      if (ev.type === "pointerup" && moved < 6) jogPress();      // krátke stlačenie bez otočenia = vybrať
    };
    hit.addEventListener("pointerup", end); hit.addEventListener("pointercancel", end);
    let wheelT = 0;
    hit.addEventListener("wheel", (ev) => { ev.preventDefault(); const d = ev.deltaY < 0 ? 1 : -1; jogAcc += d * 15; drawKnob(o, jogAcc); hit.classList.add("active"); clearTimeout(wheelT); wheelT = setTimeout(() => hit.classList.remove("active"), 300); jogStep(d, ev.shiftKey); }, { passive: false });
  }
  // stlačenie DATA: v menu vyberie položku, inak otvorí rýchly výber vrstvy
  function jogPress () {
    if (menu) { menuEnter(); return; }
    if (!tracks.length) { flashValue("Zatiaľ žiadna vrstva"); return; }
    const items = tracks.map((x, i) => ({ t: (i + 1) + "  " + x.name, act: () => { localSel = x.id; page = Math.floor(i / 4); if (has(4)) send("selectTrack", { trackId: x.id }); flashValue("Vybraná vrstva: " + x.name); } }));
    menu = { id: "select", title: "VÝBER VRSTVY", items, sel: Math.max(0, tracks.findIndex(x => x.id === selectedId())) };
    drawMenu();
  }
  function jogStep (dir, big) {
    if (menu) return menuMove(dir);
    if (!st || rec()) { if (rec()) flashValue("Počas nahrávania sa pozícia nemení"); return; }
    const beat = 60 / ((st.tempo && st.tempo.bpm) || 120) * (4 / ((st.tempo && st.tempo.denominator) || 4));
    const len = big ? beat * ((st.tempo && st.tempo.numerator) || 4) : beat;
    const pos = Math.max(0, Math.round((st.transport.position + dir * len) / beat) * beat);
    send("seek", { seconds: pos }); st.transport.position = pos;
    flashValue((big ? "Takt " : "Doba ") + (dir > 0 ? "▶" : "◀") + "  " + fmtTime(pos));
  }

  // ------------------------------------------------------------------ FADERY
  const DB_T = [[6, 0], [0, 1 / 3], [-12, 2 / 3], [-24, 0.79], [-40, 0.9], [-60, 0.97], [-100, 1]];
  const dbToT = (db) => { db = clamp(db, -100, 6); for (let i = 1; i < DB_T.length; i++) { const [d0, t0] = DB_T[i - 1], [d1, t1] = DB_T[i]; if (db >= d1) return t0 + (t1 - t0) * (d0 - db) / (d0 - d1); } return 1; };
  const tToDb = (t) => { t = clamp(t, 0, 1); for (let i = 1; i < DB_T.length; i++) { const [d0, t0] = DB_T[i - 1], [d1, t1] = DB_T[i]; if (t <= t1) return d0 - (d0 - d1) * (t - t0) / (t1 - t0); } return -100; };
  const F = {};
  for (const [id, f] of Object.entries(C.faders)) {
    const img = el("img", "spr", spritesEl, { left: px(f.x), top: px(f.y), width: px(f.w), height: px(f.h) }); img.src = f.sprite;
    const y6 = f.marks["6"], yInf = f.marks["-100"];
    const o = { f, img, y6, yInf, local: null, until: 0, i: f.track };
    const cx = f.x + f.w / 2;
    const hit = el("div", "hit fader rect", hitEl, { left: px(cx - 34), top: px(y6 - 34), width: px(70), height: px(yInf - y6 + 66) });
    o.hit = hit; F[id] = o; setupFader(hit, o);
  }
  function faderDb (o) {
    if (o.local !== null && performance.now() < o.until) return o.local;
    const t = trackAt(o.i); return t ? (typeof t.volumeDb === "number" ? t.volumeDb : 0) : 0;
  }
  function drawFader (o, db) {
    const y = o.y6 + dbToT(db) * (o.yInf - o.y6), dy = y - o.f.grooveY;
    o.img.style.transform = `translate(${(o.f.slant * dy).toFixed(2)}px, ${dy.toFixed(2)}px)`;
  }
  function setupFader (hit, o) {
    let startY = 0, startT = 0, lastSend = 0;
    const apply = (db, final) => {
      const t = trackAt(o.i); db = db >= 5.95 ? 6 : db; o.local = db; o.until = performance.now() + 600; drawFader(o, db);
      flashValue("HLASITOSŤ " + (o.i + page * 4 + 1) + "  " + dbTxt(Math.max(-60, db)));
      const now = performance.now();
      if (t && (final || now - lastSend > 45)) { send("trackVolume", { trackId: t.id, db: db <= -59.5 ? -100 : Math.round(db * 10) / 10 }); lastSend = now; }
    };
    hit.addEventListener("pointerdown", (ev) => {
      if (ev.button !== 0) return; if (!trackAt(o.i)) { flashValue("Kanál je prázdny"); return; }
      hit.setPointerCapture(ev.pointerId); startY = ev.clientY; startT = dbToT(faderDb(o)); hold.fader = o;
    });
    hit.addEventListener("pointermove", (ev) => {
      if (hold.fader !== o) return;
      const dy = (ev.clientY - startY) / scale / (o.yInf - o.y6) * (ev.shiftKey ? 0.2 : 1);
      apply(tToDb(startT + dy), false);
    });
    const end = () => { if (hold.fader === o) { hold.fader = null; apply(o.local ?? faderDb(o), true); } };
    hit.addEventListener("pointerup", end); hit.addEventListener("pointercancel", end);
    hit.addEventListener("wheel", (ev) => { ev.preventDefault(); if (!trackAt(o.i)) return; const cur = Math.max(-60, faderDb(o)); apply(clamp(cur + (ev.deltaY < 0 ? 1 : -1), -60, 6), true); }, { passive: false });
    hit.addEventListener("dblclick", () => { if (trackAt(o.i)) apply(0, true); });
  }

  // ------------------------------------------------------------------ VU METRE
  const VU = C.vu, needles = $("#vuNeedles");
  const vuSide = {};
  for (const side of ["L", "R"]) {
    const v = VU[side], [x0, y0, x1, y1] = v.face;
    const r = $("#clipVu" + side + "r"); r.setAttribute("x", x0); r.setAttribute("y", y0); r.setAttribute("width", x1 - x0); r.setAttribute("height", y1 - y0);
    const g = svg("g", { "clip-path": `url(#clipVu${side})` }, needles);
    const shadow = svg("line", { stroke: "rgba(40,20,0,.22)", "stroke-width": 2.2, "stroke-linecap": "round" }, g);
    const line = svg("line", { stroke: "#17110d", "stroke-width": 1.5, "stroke-linecap": "round" }, g);
    const [dx0, dy0, dx1, dy1] = v.dome;
    const dome = el("img", "spr", spritesEl, { left: px(dx0), top: px(dy0), width: px(dx1 - dx0), height: px(dy1 - dy0) }); dome.src = `assets/vu_dome_${side}.webp`;
    vuSide[side] = { v, line, shadow, val: -22 };
  }
  // uhly stupnice: body na oblúku stupnice ľavého VU (pravý je posunutý)
  const arcC = [281, 384.6], arcR = 136.6;
  const pL = VU.L.pivot;
  const scaleAng = VU.scaleX.map(x => { const y = arcC[1] - Math.sqrt(arcR * arcR - (x - arcC[0]) ** 2); return Math.atan2(x - pL[0], pL[1] - y) * 180 / Math.PI; });
  function vuAngle (vu) {
    const d = VU.scaleDb; vu = clamp(vu, -23, 3.6);
    if (vu <= d[0]) return scaleAng[0] - (d[0] - vu) * 1.2;
    for (let i = 1; i < d.length; i++) if (vu <= d[i]) return scaleAng[i - 1] + (scaleAng[i] - scaleAng[i - 1]) * (vu - d[i - 1]) / (d[i] - d[i - 1]);
    return scaleAng[scaleAng.length - 1] + (vu - 3) * 2;
  }
  function drawNeedle (o, ang) {
    const [px0, py0] = o.v.pivot, a = ang * Math.PI / 180, L = VU.length;
    const x = px0 + Math.sin(a) * L, y = py0 - Math.cos(a) * L;
    for (const [ln, ox, oy] of [[o.shadow, 1.8, 2.6], [o.line, 0, 0]]) { ln.setAttribute("x1", px0 + ox); ln.setAttribute("y1", py0 + oy); ln.setAttribute("x2", x + ox); ln.setAttribute("y2", y + oy); }
  }
  function vuTargets () {
    if (!st) return [-60, -60];
    const playing = st.transport && st.transport.playing;
    const m = st.mixer || {};
    if (playing) {
      const l = typeof m.masterLevelL === "number" ? m.masterLevelL : m.masterLevelDb;
      const r = typeof m.masterLevelR === "number" ? m.masterLevelR : m.masterLevelDb;
      return [l ?? -100, r ?? -100];
    }
    const i = st.meter ? st.meter.inputDb : -100;   // v kľude ukazuje vstup gitary
    return [i, i];
  }

  // ------------------------------------------------------------------ DISPLEJ – obsah
  const lanesEl = $("#dLanes");
  const fmtTime = (s) => { const neg = s < 0; s = Math.abs(s); const m = Math.floor(s / 60), r = s - m * 60; return (neg ? "−" : "") + m + ":" + r.toFixed(3).padStart(6, "0"); };
  const fmtShort = (s) => { const m = Math.floor(s / 60), r = Math.floor(s - m * 60); return m + ":" + String(r).padStart(2, "0"); };
  const LANE_X = 122;
  const LANE_H = 24;
  const laneEls = [];
  for (let i = 0; i < 4; i++) {
    const lane = el("div", "lane", lanesEl, { top: px(i * LANE_H) });
    const ln = el("div", "ln", lane); const cv = el("canvas", "", lane); cv.width = 290 * 2; cv.height = 22 * 2;
    laneEls.push({ lane, ln, cv, sig: "" });
  }
  const loopEl = el("div", "d-loop", lanesEl, { display: "none" });
  const headEl = el("div", "d-playhead", lanesEl);
  const emptyEl = el("div", "d-empty", lanesEl); emptyEl.textContent = "Zatiaľ žiadna vrstva. Stlač REC a hraj.";
  const pageEl = el("div", "d-page", lanesEl);
  const laneW = () => lanesEl.clientWidth - LANE_X;

  function drawWave (le, t) {
    const sig = t ? t.id + ":" + t.clips.length + ":" + viewLength + ":" + (t.clips[0] ? t.clips[0].length + ":" + t.clips[0].start : "") : "";
    if (sig === le.sig) return; le.sig = sig;
    const cv = le.cv, g = cv.getContext("2d"); g.clearRect(0, 0, cv.width, cv.height);
    if (!t) return;
    const sx = cv.width / viewLength, mid = cv.height / 2;
    g.fillStyle = "#ffb547"; g.shadowColor = "rgba(255,150,30,.7)"; g.shadowBlur = 4;
    for (const c of t.clips) {
      const pk = c.peaks || []; if (!pk.length) continue;
      const x0 = c.start * sx, w = c.length * sx, step = w / pk.length;
      for (let i = 0; i < pk.length; i++) { const h = Math.max(1, pk[i] * mid * 0.92); g.fillRect(x0 + i * step, mid - h, Math.max(1, step * 0.8), h * 2); }
    }
    g.shadowBlur = 0; g.fillStyle = "rgba(255,170,60,.35)"; g.fillRect(0, mid - 0.5, cv.width, 1);
  }

  function renderDisplay () {
    if (!st) return;
    $("#dSong").textContent = st.device && st.device.ok === false ? "Zvuk nie je pripravený – SETTINGS › Nastavenia zvuku" : (st.song ? st.song.name : "–");
    const tp = st.tempo || { bpm: 120, numerator: 4, denominator: 4 };
    $("#dTempo").textContent = "♪ " + tp.bpm.toFixed(1);
    $("#dMeter").textContent = tp.numerator + "/" + tp.denominator;
    $("#dMetroIcon").classList.toggle("on", !!(st.metronome && st.metronome.on));
    const t = st.transport || {};
    $("#dTime").textContent = fmtTime(t.position || 0);
    const status = $("#dStatus");
    status.className = "d-status" + (t.recording ? " rec" : "");
    status.textContent = t.countingIn ? "ODPOČÍTAVANIE" : t.recording ? "● REC" : t.playing ? "▶ HRÁ" : "";
    const mp = st.musicalPosition || { bar: 1, beat: 1 };
    $("#dBar").textContent = "T " + (mp.bar || 0) + "." + (mp.beat || 0);
    const lp = st.loop || {};
    $("#dLoop").textContent = lp.on ? "LOOP " + (Math.round((lp.lengthBars || 0) * 10) / 10) + " T" : "LOOP VYP";
    $("#dLength").textContent = "/ " + fmtShort((st.song && st.song.length) || 0);
    // vrstvy
    const sel = selectedId();
    for (let i = 0; i < 4; i++) {
      const tr = trackAt(i), le = laneEls[i];
      le.lane.style.display = tr ? "flex" : "none";
      if (tr) {
        le.ln.innerHTML = "<b>" + (page * 4 + i + 1) + "</b>" + escapeHtml(tr.name);
        le.lane.classList.toggle("sel", tr.id === sel);
        le.lane.classList.toggle("muted", !!tr.muted || (tracks.some(x => x.solo) && !tr.solo));
        le.lane.classList.toggle("rec", !!tr.recording);
      }
      drawWave(le, tr);
    }
    emptyEl.style.display = tracks.length ? "none" : "block";
    pageEl.textContent = tracks.length > 4 ? "STOPY " + (page * 4 + 1) + "–" + Math.min(tracks.length, page * 4 + 4) + " / " + tracks.length : "";
    const w = laneW();
    headEl.style.left = px(LANE_X + clamp((t.position || 0) / viewLength, 0, 1) * (w - 2));
    if (lp.on) { loopEl.style.display = "block"; loopEl.style.left = px(LANE_X + lp.start / viewLength * w); loopEl.style.width = px(Math.max(1, (lp.end - lp.start) / viewLength * w)); }
    else loopEl.style.display = "none";
  }
  const escapeHtml = (s) => String(s).replace(/[&<>"]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

  // ------------------------------------------------------------------ MENU na displeji
  let menu = null;  // { id, title, items:[{t, act, dis}], sel }
  const menuEl = $("#dMenu");
  function buildMenu (id) {
    const editing = !(st && st.busy && st.busy.editingAllowed === false);
    const t = selectedTrack();
    const M = {
      project: { title: "PROJECT", items: [
        { t: "Nová skladba", act: () => send("newSong"), dis: !editing },
        { t: "Otvoriť skladbu", act: () => send("openSong"), dis: !editing },
        { t: "Uložiť skladbu", act: () => needs(4, "Uložiť skladbu") && send("saveSong"), dis: !has(4) },
        { t: "Otvoriť priečinok skladby", act: () => send("openSongFolder") },
        { t: "Exportovať mix", act: () => send("exportMix"), dis: !editing } ] },
      tracks: { title: "TRACKS", items: [
        { t: "Pridať stopu", act: () => needs(4, "Pridať stopu") && send("trackAdd"), dis: !has(4) },
        { t: "Vybrať stopu…", sub: () => tracks.map((x, i) => ({ t: (i + 1) + "  " + x.name, act: () => { localSel = x.id; page = Math.floor(i / 4); if (has(4)) send("selectTrack", { trackId: x.id }); } })), dis: !tracks.length },
        { t: "Premenovať stopu", act: () => renameTrack(), dis: !t },
        { t: t && t.muted ? "Zrušiť stlmenie stopy" : "Stlmiť stopu", act: () => t && send("trackMute", { trackId: t.id, muted: !t.muted }), dis: !t },
        { t: "Odstrániť stopu", act: () => t && send("trackDelete", { trackId: t.id }), dis: !t || !editing },
        ...(tracks.length > 4 ? [{ t: "Ďalšie kanály (stopy " + (((page + 1) % Math.ceil(tracks.length / 4)) * 4 + 1) + "–)", act: () => { page = (page + 1) % Math.ceil(tracks.length / 4); } }] : []) ] },
      effects: { title: "EFFECTS", items: [ { t: "Efekty pribudnú v ďalšej etape.", dis: true } ] },
      settings: { title: "SETTINGS", items: [
        { t: "Nastavenia zvuku", act: () => send("openAudioSettings"), dis: !editing },
        { t: "Vstupné zariadenie…", sub: () => (st && st.inputs || []).map(x => ({ t: (x.id === st.selectedInput ? "● " : "  ") + x.name, act: () => send("selectInput", { id: x.id }) })), dis: !editing },
        { t: "Výstupné zariadenie", act: () => send("openAudioSettings"), dis: !editing },
        { t: "Monitoring: " + (st && st.monitoring ? "zapnutý" : "vypnutý"), act: () => send("setMonitoring", { on: !(st && st.monitoring) }) },
        { t: "Metrum…", sub: () => [[2, 4], [3, 4], [4, 4], [5, 4], [6, 8], [7, 8]].map(([n, d]) => ({ t: n + "/" + d, act: () => send("setTimeSignature", { numerator: n, denominator: d }) })), dis: rec() },
        { t: "Odpočítanie…", sub: () => [0, 1, 2].map(b => ({ t: b === 0 ? "bez odpočítania" : b + (b === 1 ? " takt" : " takty"), act: () => send("setCountIn", { bars: b }) })) },
        { t: "Diagnostika", act: () => send("runDiagnostics"), dis: !(st && st.busy && st.busy.diagnosticsAllowed !== false) } ] },
    };
    return Object.assign({ id, sel: 0 }, M[id]);
  }
  function toggleMenu (id) {
    if (menu && menu.id === id && !menu.parent) { closeMenu(); return; }
    menu = buildMenu(id); menu.sel = Math.max(0, menu.items.findIndex(i => !i.dis)); drawMenu();
  }
  function closeMenu () { menu = null; menuEl.hidden = true; render(); }
  function drawMenu () {
    if (!menu) return;
    menuEl.hidden = false;
    menuEl.innerHTML = "<h4>" + menu.title + "</h4>" + menu.items.map((it, i) =>
      `<div class="mi${i === menu.sel ? " sel" : ""}${it.dis ? " dis" : ""}" data-i="${i}">${escapeHtml(it.t)}</div>`).join("") +
      `<div class="hint">DATA: otočiť = pohyb, klik = vybrať, ${menu.parent ? "Esc = späť" : "tlačidlo = zavrieť"}</div>`;
    render();
  }
  menuEl.addEventListener("pointerover", (ev) => { const i = ev.target.dataset && ev.target.dataset.i; if (i !== undefined && menu) { menu.sel = +i; drawMenu(); } });
  menuEl.addEventListener("click", (ev) => { const i = ev.target.dataset && ev.target.dataset.i; if (i !== undefined && menu) { menu.sel = +i; menuEnter(); } });
  function menuMove (dir) { if (!menu || !menu.items.length) return; menu.sel = (menu.sel + dir + menu.items.length) % menu.items.length; drawMenu(); }
  function menuEnter () {
    if (!menu) return; const it = menu.items[menu.sel]; if (!it || it.dis) return;
    if (it.sub) { const items = it.sub(); menu = { id: menu.id, title: menu.title + " › " + it.t.replace("…", ""), items, sel: 0, parent: menu }; drawMenu(); return; }
    if (it.act) it.act(); closeMenu();
  }
  function menuBack () { if (menu && menu.parent) { menu = menu.parent; drawMenu(); } else closeMenu(); }
  function renameTrack () {
    const t = selectedTrack(); if (!t) return;
    menuEl.hidden = false;
    menuEl.innerHTML = `<h4>PREMENOVAŤ STOPU</h4><input id="dRename" maxlength="40" style="width:100%;box-sizing:border-box;background:#1a0b03;color:#ffd08a;border:1px solid #c47a2c;font:500 14px RafaSans,sans-serif;padding:4px 6px" value="${escapeHtml(t.name)}"><div class="hint">Enter = uložiť · Esc = zrušiť</div>`;
    menu = { id: "rename", title: "", items: [], sel: 0 };
    const inp = $("#dRename"); setTimeout(() => { inp.focus(); inp.select(); }, 0);
    inp.addEventListener("keydown", (ev) => {
      ev.stopPropagation();
      if (ev.key === "Enter") { const v = inp.value.trim(); if (v) send("trackRename", { trackId: t.id, name: v }); closeMenu(); }
      if (ev.key === "Escape") closeMenu();
    });
  }

  // ------------------------------------------------------------------ klávesnica
  addEventListener("keydown", (ev) => {
    if (ev.target && ev.target.tagName === "INPUT") return;
    if (menu) {
      if (ev.key === "ArrowDown") { menuMove(1); ev.preventDefault(); return; }
      if (ev.key === "ArrowUp") { menuMove(-1); ev.preventDefault(); return; }
      if (ev.key === "Enter") { menuEnter(); ev.preventDefault(); return; }
      if (ev.key === "Escape" || ev.key === "Backspace") { menuBack(); ev.preventDefault(); return; }
    }
    if (ev.repeat) return;
    const k = ev.key.toLowerCase();
    if (k === "r") send("record");
    else if (k === " ") { send("togglePlay"); ev.preventDefault(); }
    else if (ev.key === "Home") send("rewind");
    else if (k === "l") send("toggleLoop");
    else if (ev.key === "ArrowRight") jogStep(1, ev.shiftKey);
    else if (ev.key === "ArrowLeft") jogStep(-1, ev.shiftKey);
  });

  // ------------------------------------------------------------------ vykreslenie stavu
  function render () {
    if (!st) return;
    const t = st.transport || {}, now = performance.now();
    const isFlash = (id) => flashing[id] && flashing[id] > now;
    lit("play", t.playing && !t.recording); lit("rec", t.recording && !t.countingIn); armed("rec", t.countingIn);
    lit("stop", isFlash("stop")); lit("rewind", isFlash("rewind")); lit("loopIn", isFlash("loopIn")); lit("loopOut", isFlash("loopOut")); lit("layerAdd", isFlash("layerAdd"));
    lit("loop", st.loop && st.loop.on); lit("metroOn", st.metronome && st.metronome.on);
    for (const m of ["project", "tracks", "effects", "settings"]) lit(m, menu && menu.id === m);
    const sel = selectedId(), anySolo = tracks.some(x => x.solo);
    for (let i = 0; i < 4; i++) {
      const tr = trackAt(i);
      lit("layer" + (i + 1), tr && tr.id === sel);
      lit("mute" + (i + 1), tr && tr.muted); lit("solo" + (i + 1), tr && tr.solo);
      BTN["mute" + (i + 1)].hit.classList.toggle("disabled", !tr);
      BTN["solo" + (i + 1)].hit.classList.toggle("disabled", !tr);
      F["f" + (i + 1)].hit.classList.toggle("disabled", !tr);
      if (hold.fader !== F["f" + (i + 1)]) drawFader(F["f" + (i + 1)], tr ? faderDb(F["f" + (i + 1)]) : 0);
    }
    BTN.layerAdd.hit.classList.toggle("disabled", !has(4));
    for (const [id, o] of Object.entries(K)) {
      if (id === "jog") { drawKnob(o, jogAcc); continue; }
      o.hit.classList.toggle("disabled", knobDisabled(o));
      if (hold.knob !== o) drawKnob(o, knobAngle(o.cfg, knobValue(o)));
    }
    renderDisplay();
  }

  // LED metronómu: bliká na dobu, prvá doba taktu jasnejšie
  let lastBeat = "";
  function beatLed () {
    if (!st) return;
    const t = st.transport || {}, mp = st.musicalPosition || {};
    const running = (t.playing || t.recording) && ((st.metronome && st.metronome.on) || t.countingIn);
    const key = mp.bar + "." + mp.beat;
    if (running && key !== lastBeat) {
      lastBeat = key; LED.metro.classList.add("lit"); LED.metro.classList.toggle("accent", mp.beat === 1);
      setTimeout(() => LED.metro.classList.remove("lit"), 110);
    }
    if (!running) lastBeat = "";
    LED.effects.classList.toggle("lit", !!(menu && menu.id === "effects"));
  }

  // animácia VU (balistika VU: 300 ms)
  let lastT = performance.now();
  function frame (now) {
    const dt = Math.min(0.1, (now - lastT) / 1000); lastT = now;
    const [tl, tr] = vuTargets();
    for (const [side, target] of [["L", tl], ["R", tr]]) {
      const o = vuSide[side], vu = clamp(target + 18, -21.8, 3.3);   // 0 VU = −18 dBFS
      o.val += (vu - o.val) * (1 - Math.exp(-dt / 0.3 * 3));
      drawNeedle(o, vuAngle(o.val));
    }
    beatLed();
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);

  // ------------------------------------------------------------------ motor
  let lastStatus = "";
  Rafa.on("state", (s) => {
    const stx = s && s.status ? s.status.text : "";
    if (stx && stx !== lastStatus && lastStatus !== "") flashValue(stx, s.status.level === "error" || s.status.level === "warn" ? 3500 : 2200);
    lastStatus = stx || lastStatus;
    st = s; if (menu && ["project", "tracks", "effects", "settings"].includes(menu.id) && !menu.parent) { const m = buildMenu(menu.id); m.sel = menu.sel; menu = m; drawMenu(); } else render(); });
  Rafa.on("tracks", (t) => {
    tracks = (t && t.tracks) || []; viewLength = (t && t.viewLength) || 30;
    if (page * 4 >= tracks.length) page = Math.max(0, Math.ceil(tracks.length / 4) - 1);
    if (localSel && !tracks.some(x => x.id === localSel)) localSel = null;
    for (const le of laneEls) le.sig = "";
    render();
  });
  send("uiReady");
  window.RafaConsole = { render, get state () { return st; }, get tracks () { return tracks; } };
})();
