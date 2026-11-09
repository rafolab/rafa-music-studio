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
  let stageX = 0, stageY = 0;
  function fit () {
    const vw = document.documentElement.clientWidth, vh = innerHeight;
    scale = Math.min(vw / W, vh / H);
    stageX = (vw - W * scale) / 2; stageY = Math.max(0, (vh - H * scale) / 2);
    stage.style.transform = `translate(${stageX}px, ${stageY}px) scale(${scale})`;
    if (window.RafaPanel) RafaPanel.layout();
  }
  addEventListener("resize", fit);

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
    const extra = id.startsWith("mute") ? " led-red" : id.startsWith("solo") ? " led-yellow" : "";
    const wrap = el("div", "btn k-" + b.kind + extra, spritesEl, { left: px(b.x), top: px(b.y), width: px(b.w), height: px(b.h) });
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
      fmt: (v) => v >= 0.5 ? "ZAP – gitara aj z počítača (môže byť zdvojená)" : "VYP – gitaru počuješ z komba (odporúčané pre BEAM)" },
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
      if (o.cfg.snap) flashValue(o.cfg.name + " " + o.cfg.fmt(o.local), 2600);
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
    hit.addEventListener("wheel", (ev) => { ev.preventDefault(); const w_ = wheelStep(ev); if (!w_) return; const d = -w_; jogAcc += d * 15; drawKnob(o, jogAcc); hit.classList.add("active"); clearTimeout(wheelT); wheelT = setTimeout(() => hit.classList.remove("active"), 300); jogStep(d, ev.shiftKey); }, { passive: false });
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
  const LANE_H = 22;
  const laneEls = [];
  for (let i = 0; i < 4; i++) {
    const lane = el("div", "lane", lanesEl, { top: px(10 + i * LANE_H) });
    const ln = el("div", "ln", lane); const cv = el("canvas", "", lane); cv.width = 290 * 2; cv.height = 20 * 2;
    laneEls.push({ lane, ln, cv, sig: "" });
  }
  const loopEl = el("div", "d-loop", lanesEl, { display: "none" });
  const headEl = el("div", "d-playhead", lanesEl);
  const emptyEl = el("div", "d-empty", lanesEl); emptyEl.textContent = "Zatiaľ žiadna vrstva. Stlač REC a hraj.";
  const pageEl = el("div", "d-page", lanesEl);
  const laneW = () => lanesEl.clientWidth - LANE_X;
  loopEl.innerHTML = "<i class='in'>IN</i><i class='out'>OUT</i>";
  const gridEl = el("div", "d-grid", lanesEl); let gridSig = "";
  function drawGrid (w) {
    const barLen = st && st.tempo ? 60 / st.tempo.bpm * st.tempo.numerator * (4 / st.tempo.denominator) : 2;
    const sig = w + ":" + barLen + ":" + viewLength; if (sig === gridSig) return; gridSig = sig;
    let step = 1; while (viewLength / (barLen * step) > 16) step *= 2;
    let h = "";
    for (let b = 0; b * barLen <= viewLength; b += step) {
      const x = LANE_X + b * barLen / viewLength * w;
      h += `<span style="left:${x.toFixed(1)}px">${b + 1}</span>`;   // cislo taktu hore, ciara cez vrstvy
    }
    gridEl.innerHTML = h;
  }

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
    const bars = Math.round((lp.lengthBars || 0) * 10) / 10, secs = Math.max(0, (lp.end || 0) - (lp.start || 0));
    const dl = $("#dLoop");
    if (t.recording && lp.on && !t.countingIn) {
      // priebeh nahravania po taktoch: Takt 2 / 4 ■■□□
      const barLen = secs / Math.max(1, bars), done = clamp(Math.floor(((t.position || 0) - lp.start) / barLen) + 1, 1, Math.ceil(bars));
      dl.innerHTML = "TAKT " + done + " / " + bars + " <span class='boxes'>" + Array.from({ length: Math.min(16, Math.ceil(bars)) }, (_, i) => i < done ? "■" : "□").join("") + "</span>";
      dl.className = "d-loopfield rec";
    } else {
      dl.textContent = (lp.on ? "LOOP " : "LOOP VYP · ") + bars + " T · " + secs.toFixed(1).replace(".", ",") + " s";
      dl.className = "d-loopfield" + (lp.on ? " on" : "") + (menu && menu.id === "loopLen" ? " edit" : "");
    }
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
    if (lp.on || (menu && menu.id === "loopLen")) { loopEl.style.display = "block"; loopEl.style.left = px(LANE_X + lp.start / viewLength * w); loopEl.style.width = px(Math.max(1, (lp.end - lp.start) / viewLength * w)); loopEl.classList.toggle("off", !lp.on); }
    else loopEl.style.display = "none";
    drawGrid(w);
  }
  const escapeHtml = (s) => String(s).replace(/[&<>"]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

  // ------------------------------------------------------------------ MENU na displeji
  let menu = null;  // { id, title, items:[{t, act, dis, sub}], sel, parent }
  let lastMenuHtml = "";
  const menuEl = $("#dMenu");
  const LOOP_BARS = [1, 2, 4, 8, 16];
  const isDraftWithTakes = () => st && st.song && st.song.isDraft && st.song.hasRecordings;

  // odchod z nepomenovanej skladby s nahravkami: Uložiť / Zahodiť / Zrušiť
  function leaveSong (thenAct, thenOpenPath) {
    if (!isDraftWithTakes()) { thenAct(); return; }
    openMenu({ id: "leave", title: "SKLADBA NIE JE POMENOVANÁ", items: [
      { t: "Uložiť ako…", act: () => askText("NÁZOV SKLADBY", "", (name) => { send("saveSongAs", { name }); afterSave(thenAct); }) },
      { t: "Zahodiť (do koša Windows)", act: () => thenOpenPath === "new" ? send("discardSong", {}) : send("discardSong", { thenOpen: thenOpenPath || "" }) },
      { t: "Zrušiť", act: () => {} } ] });
  }
  // po "Ulozit ako" pockame, kym motor skladbu premenuje, az potom pokracujeme
  function afterSave (thenAct) {
    const t0 = performance.now();
    const tick = () => {
      if (st && st.song && !st.song.isDraft) { thenAct(); return; }
      if (performance.now() - t0 < 4000) setTimeout(tick, 100);
    };
    setTimeout(tick, 150);
  }
  let pendingSongList = null;
  function withSongList (fn) {
    send("listSongs"); pendingSongList = fn;
  }

  function buildMenu (id) {
    const editing = !(st && st.busy && st.busy.editingAllowed === false) && !rec();
    const t = selectedTrack();
    const h = (st && st.history) || {};
    const draft = st && st.song && st.song.isDraft;
    const M = {
      project: { title: "PROJECT", items: [
        { t: "Nová skladba", act: () => leaveSong(() => send("newSong"), "new"), dis: !editing },
        { t: "Otvoriť skladbu…", act: () => withSongList(showSongList), dis: !editing },
        { t: draft ? "Uložiť skladbu… (pomenovať)" : "Uložiť skladbu", act: () => draft ? askText("NÁZOV SKLADBY", "", (n) => send("saveSongAs", { name: n })) : send("saveSong"), dis: rec() },
        { t: "Uložiť ako…", act: () => askText("NÁZOV SKLADBY", draft ? "" : (st.song.name + " 2"), (n) => send("saveSongAs", { name: n })), dis: rec() },
        { t: "Exportovať mix (WAV)", act: () => send("exportMix"), dis: !editing },
        { t: "Exportovať stopy (WAV pre Sonar)", act: () => send("exportTracks"), dis: !editing || !has(5) },
        { t: "Otvoriť priečinok exportu", act: () => send("openExportFolder"), dis: !has(5) },
        { t: "Otvoriť priečinok skladby", act: () => send("openSongFolder") },
        { t: "Upratať staré skladby…", act: () => withSongList(showCleanup), dis: !editing },
        { t: "Odstrániť túto skladbu…", act: () => confirmBox("ODSTRÁNIŤ SKLADBU „" + st.song.name + "“?", "Presunie sa do koša Windows.", () => send("deleteSong")), dis: !editing } ] },
      tracks: { title: "TRACKS", items: [
        { t: "Vrátiť pokus  (Ctrl+Z)", act: () => send("undo"), dis: !h.canUndo || rec() },
        { t: "Obnoviť  (Ctrl+Y)", act: () => send("redo"), dis: !h.canRedo || rec() },
        { t: "Pridať stopu", act: () => send("trackAdd"), dis: !editing },
        { t: "Vybrať stopu…", sub: () => tracks.map((x, i) => ({ t: (i + 1) + "  " + x.name, act: () => pickTrack(x) })), dis: !tracks.length },
        { t: "Premenovať stopu", act: () => renameTrack(), dis: !t },
        { t: "Odstrániť stopu  (Delete)", act: () => t && send("trackDelete", { trackId: t.id }), dis: !t || !editing },
        { t: "Panel stôp  (T)", act: () => window.RafaPanel && RafaPanel.toggle() },
        ...(tracks.length > 4 ? [{ t: "Ďalšie kanály (stopy " + (((page + 1) % Math.ceil(tracks.length / 4)) * 4 + 1) + "–)", act: () => { page = (page + 1) % Math.ceil(tracks.length / 4); } }] : []) ] },
      effects: { title: "EFFECTS", items: [ { t: "Efekty pribudnú v ďalšej etape.", dis: true } ] },
      settings: { title: "SETTINGS", items: [
        { t: "Nastavenia zvuku", act: () => send("openAudioSettings"), dis: !editing },
        { t: "Vstupné zariadenie…", sub: () => (st && st.inputs || []).map(x => ({ t: (x.id === st.selectedInput ? "● " : "  ") + x.name, act: () => send("selectInput", { id: x.id }) })), dis: !editing },
        { t: "Výstupné zariadenie", act: () => send("openAudioSettings"), dis: !editing },
        { t: "Monitoring: " + (st && st.monitoring ? "ZAP (gitara aj z počítača)" : "VYP (gitara z komba)"), act: () => send("setMonitoring", { on: !(st && st.monitoring) }) },
        { t: "Dĺžka slučky…", act: () => openLoopLen(), dis: rec() },
        { t: "Metrum…", sub: () => [[2, 4], [3, 4], [4, 4], [5, 4], [6, 8], [7, 8]].map(([n, d]) => ({ t: n + "/" + d, act: () => send("setTimeSignature", { numerator: n, denominator: d }) })), dis: rec() },
        { t: "Odpočítanie…", sub: () => [0, 1, 2].map(b => ({ t: b === 0 ? "bez odpočítania" : b + (b === 1 ? " takt" : " takty"), act: () => send("setCountIn", { bars: b }) })) },
        { t: "Diagnostika", act: () => send("runDiagnostics"), dis: !(st && st.busy && st.busy.diagnosticsAllowed !== false) } ] },
    };
    return Object.assign({ id, sel: 0 }, M[id]);
  }
  function pickTrack (x) {
    const i = tracks.indexOf(x); localSel = x.id; page = Math.max(0, Math.floor(i / 4));
    send("selectTrack", { trackId: x.id }); flashValue("Vybraná vrstva: " + x.name);
  }
  function openMenu (m) { menu = Object.assign({ sel: 0 }, m); menu.sel = Math.max(0, menu.items.findIndex(i => !i.dis)); drawMenu(); }
  function toggleMenu (id) {
    if (menu && menu.id === id && !menu.parent) { closeMenu(); return; }
    openMenu(buildMenu(id));
  }
  function openLoopLen () {
    const lp = (st && st.loop) || {}, barSec = Math.max(0.001, (lp.end - lp.start) / Math.max(0.001, lp.lengthBars || 4));
    const cur = Math.round(lp.lengthBars || 4);
    openMenu({ id: "loopLen", title: "DĹŽKA SLUČKY", items: LOOP_BARS.map(b => ({
      t: (b === cur ? "● " : "   ") + b + (b === 1 ? " takt" : b < 5 ? " takty" : " taktov") + "  ·  " + (b * barSec).toFixed(1).replace(".", ",") + " s",
      act: () => { send("setLoopBars", { bars: b }); if (!(st.loop && st.loop.on)) send("setLoop", { on: true }); flashValue("LOOP " + b + " T · " + (b * barSec).toFixed(1).replace(".", ",") + " s – zapnutý"); } })) });
    menu.sel = Math.max(0, LOOP_BARS.indexOf(cur));
    drawMenu();
  }
  $("#dLoop").addEventListener("click", () => { if (!rec()) (menu && menu.id === "loopLen") ? closeMenu() : openLoopLen(); });

  function showSongList (list) {
    const items = [];
    for (const x of list) items.push({ t: (x.current ? "● " : "   ") + (x.draft ? x.name : x.name), act: () => { if (!x.current) leaveSong(() => send("openSongPath", { path: x.path }), x.path); } });
    if (!items.length) items.push({ t: "Zatiaľ žiadne skladby.", dis: true });
    openMenu({ id: "songs", title: "OTVORIŤ SKLADBU", items });
  }
  function showCleanup (list) {
    const old = list.filter(x => !x.draft && !x.current);
    const chosen = new Set();
    const build = () => {
      const items = old.map(x => ({ t: (chosen.has(x.path) ? "☑ " : "☐ ") + x.name + "  ·  " + x.modified, keep: true, act: () => { chosen.has(x.path) ? chosen.delete(x.path) : chosen.add(x.path); const sel = menu.sel; openMenu(build()); menu.sel = sel; drawMenu(); } }));
      items.push({ t: "▶ Presunúť vybrané (" + chosen.size + ") do Rozpracované", dis: !chosen.size, act: () => confirmBox("PRESUNÚŤ " + chosen.size + " SKLADIEB?", "Najprv sa vytvorí záloha v priečinku Záloha.", () => send("moveSongsToDrafts", { paths: [...chosen] })) });
      return { id: "cleanup", title: "UPRATAŤ – vyber skladby", items };
    };
    if (!old.length) { flashValue("Žiadne pomenované skladby na upratanie"); return; }
    openMenu(build());
  }
  function confirmBox (title, text, yes) {
    openMenu({ id: "confirm", title, note: text, items: [ { t: "Áno", act: yes }, { t: "Nie", act: () => {} } ] });
    menu.sel = 1; drawMenu();
  }
  function closeMenu () { menu = null; menuEl.hidden = true; lastMenuHtml = ""; render(); }
  function drawMenuQuiet () { const r = render; render = () => {}; drawMenu(); render = r; }
  const MENU_ROWS = 5;
  function drawMenu () {
    if (!menu) return;
    menuEl.hidden = false;
    const n = menu.items.length;
    // okno sa posunie len ked vyber vyjde mimo neho (klik mysou okno nehybe)
    let first = menu.first || 0;
    if (menu.sel < first) first = menu.sel; else if (menu.sel >= first + MENU_ROWS) first = menu.sel - MENU_ROWS + 1;
    first = clamp(first, 0, Math.max(0, n - MENU_ROWS)); menu.first = first;
    const rows = menu.items.slice(first, first + MENU_ROWS).map((it, k) => { const i = first + k;
      return `<div class="mi${it.dis ? " dis" : ""}" data-i="${i}">${escapeHtml(it.t)}</div>`; }).join("");
    const html = "<h4>" + escapeHtml(menu.title) + (n > MENU_ROWS ? `<span class="pos">${menu.sel + 1}/${n}</span>` : "") + "</h4>" +
      (menu.note ? `<div class="note">${escapeHtml(menu.note)}</div>` : "") + rows +
      `<div class="hint">DATA / koliesko = pohyb · dvojklik, Enter alebo stlačenie DATA = potvrdiť · ${menu.parent ? "Esc = späť" : "tlačidlo = zavrieť"}</div>`;
    // DOM menime len ked sa obsah naozaj zmenil – inak by sa klik / dvojklik "stratil" pri obnove 15× za sekundu
    if (html !== lastMenuHtml || !menuEl.firstChild) { menuEl.innerHTML = html; lastMenuHtml = html; }
    // zvyraznenie vyberu bez prestavby DOM (dvojklik tak trafi tu istu polozku)
    menuEl.querySelectorAll(".mi").forEach(e => e.classList.toggle("sel", +e.dataset.i === menu.sel));
    render();
  }
  // mys: klik = vybrat polozku, dvojklik = potvrdit (ako Enter alebo stlacenie DATA)
  const itemAt = (ev) => { const e = ev.target.closest && ev.target.closest(".mi"); return e ? +e.dataset.i : -1; };
  menuEl.addEventListener("click", (ev) => { const i = itemAt(ev); if (i >= 0 && menu && menu.sel !== i) { menu.sel = i; drawMenu(); } });
  menuEl.addEventListener("dblclick", (ev) => { const i = itemAt(ev); if (i >= 0 && menu) { menu.sel = i; menuEnter(); } });
  // koliesko: vzdy presne o jednu polozku (aj pri touchpade, ktory posiela vela malych udalosti)
  const wheelStep = (() => { let acc = 0, last = 0; return (ev) => {
    const now = performance.now(); if (now - last > 250) acc = 0;
    acc += ev.deltaMode === 1 ? ev.deltaY * 40 : ev.deltaY; last = now;
    if (Math.abs(acc) < 50) return 0;
    const d = acc > 0 ? 1 : -1; acc = 0; return d; }; })();
  menuEl.addEventListener("wheel", (ev) => { if (menu && menu.items.length) { ev.preventDefault(); const d = wheelStep(ev); if (d) menuMove(d); } }, { passive: false });
  function menuMove (dir) { if (!menu || !menu.items.length) return; menu.sel = clamp(menu.sel + dir, 0, menu.items.length - 1); drawMenu(); }
  function menuEnter () {
    if (!menu) return; const it = menu.items[menu.sel]; if (!it || it.dis) return;
    if (it.sub) { const items = it.sub(); const parent = menu; openMenu({ id: menu.id, title: menu.title + " › " + it.t.replace("…", "").trim(), items, parent }); return; }
    const m = menu;
    if (!it.keep) closeMenu();
    if (it.act) it.act();
    if (it.keep && menu === m) drawMenu();
  }
  function menuBack () { if (menu && menu.parent) { menu = menu.parent; drawMenu(); } else closeMenu(); }

  // textove pole na displeji (premenovanie stopy, nazov skladby)
  function askText (title, value, done) {
    menuEl.hidden = false;
    lastMenuHtml = "";
    menuEl.innerHTML = `<h4>${escapeHtml(title)}</h4><input id="dText" maxlength="60" class="d-input" value="${escapeHtml(value)}"><div class="hint">Enter = uložiť · Esc = zrušiť</div>`;
    menu = { id: "text", title: "", items: [], sel: 0 };
    const inp = $("#dText"); setTimeout(() => { inp.focus(); inp.select(); }, 0);
    inp.addEventListener("keydown", (ev) => {
      ev.stopPropagation();
      if (ev.key === "Enter") { const v = inp.value.trim(); closeMenu(); if (v) done(v); }
      if (ev.key === "Escape") closeMenu();
    });
  }
  function renameTrack (tr) {
    const t = tr || selectedTrack(); if (!t) return;
    askText("PREMENOVAŤ STOPU", t.name, (v) => send("trackRename", { trackId: t.id, name: v }));
  }
  function deleteSelected () { const t = selectedTrack(); if (t && !rec()) send("trackDelete", { trackId: t.id }); }

  // ------------------------------------------------------------------ klávesnica
  addEventListener("keydown", (ev) => {
    if (ev.target && ev.target.tagName === "INPUT") return;
    if (menu) {
      if (ev.key === "ArrowDown") { menuMove(1); ev.preventDefault(); return; }
      if (ev.key === "ArrowUp") { menuMove(-1); ev.preventDefault(); return; }
      if (ev.key === "Enter") { menuEnter(); ev.preventDefault(); return; }
      if (ev.key === "Escape" || ev.key === "Backspace") { menuBack(); ev.preventDefault(); return; }
    }
    const k = ev.key.toLowerCase();
    if ((ev.ctrlKey || ev.metaKey) && k === "z" && !ev.shiftKey) { send("undo"); ev.preventDefault(); return; }
    if ((ev.ctrlKey || ev.metaKey) && (k === "y" || (k === "z" && ev.shiftKey))) { send("redo"); ev.preventDefault(); return; }
    if (ev.ctrlKey || ev.metaKey || ev.altKey) return;
    if (ev.repeat) return;
    if (ev.key === "Delete") { deleteSelected(); ev.preventDefault(); }
    else if (k === "r") send("record");
    else if (k === " ") { send("togglePlay"); ev.preventDefault(); }
    else if (ev.key === "Home") send("rewind");
    else if (k === "l") send("toggleLoop");
    else if (k === "t") { if (window.RafaPanel) RafaPanel.toggle(); }
    else if (ev.key === "ArrowRight") jogStep(1, ev.shiftKey);
    else if (ev.key === "ArrowLeft") jogStep(-1, ev.shiftKey);
  });

  // ------------------------------------------------------------------ vykreslenie stavu
  let render = function () {
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
    RafaPanel.update();
  };

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

  // plynula hlava prehravania: medzi spravami z motora dopocitame cas podla hodin
  let posBase = 0, posAt = 0;
  function livePos () {
    if (!st || !st.transport) return 0;
    const t = st.transport;
    if (!t.playing || t.countingIn) return t.position || 0;
    let p = posBase + (performance.now() - posAt) / 1000;
    const lp = st.loop || {};
    if (lp.on && lp.end > lp.start && posBase >= lp.start - 0.05 && p >= lp.end) p = lp.start + ((p - lp.start) % (lp.end - lp.start));
    return p;
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
    if (st && st.transport && st.transport.playing) {
      const p = livePos(), w = laneW();
      headEl.style.left = px(LANE_X + clamp(p / viewLength, 0, 1) * (w - 2));
      RafaPanel.head(p);
    }
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);

  // ------------------------------------------------------------------ PANEL STÔP (pod konzolou, okno sa posúva)
  const RafaPanel = window.RafaPanel = (() => {
    const tab = el("button", "tp-tab", document.body); tab.type = "button";
    const pnl = el("div", "tp", document.body); pnl.hidden = true;
    pnl.innerHTML = `
      <div class="tp-head">
        <b>STOPY</b><span class="tp-song"></span>
        <span class="tp-btns">
          <button data-a="add">+ Pridať</button>
          <button data-a="rename">Premenovať</button>
          <button data-a="delete">Odstrániť</button>
          <span class="tp-sep"></span>
          <button data-a="undo" title="">↶ UNDO</button>
          <button data-a="redo">↷ REDO</button>
          <span class="tp-sep"></span>
          <span class="tp-zoomlbl">ZOOM</span>
          <button data-a="zout" title="Oddialiť (Ctrl + koliesko)">−</button>
          <button data-a="zin" title="Priblížiť (Ctrl + koliesko)">+</button>
          <button data-a="zfit" title="Celá skladba">Celé</button>
        </span>
      </div>
      <div class="tp-ruler"><div class="tp-marks"></div><div class="tp-loop"><i>IN</i><i>OUT</i></div><div class="tp-head-line"></div></div>
      <div class="tp-rows"></div>
      <div class="tp-foot"></div>`;
    const rowsEl = pnl.querySelector(".tp-rows"), ruler = pnl.querySelector(".tp-ruler"), marks = pnl.querySelector(".tp-marks"),
          loopR = pnl.querySelector(".tp-loop"), headR = pnl.querySelector(".tp-head-line"), foot = pnl.querySelector(".tp-foot");
    const NAME_W = 210;
    let open = false, rowEls = new Map(), marksSig = "";
    // ZOOM: viditelne okno casu [vStart, vStart + vLen]
    let zoom = 1, vStart = 0;
    const vLen = () => viewLength / zoom;
    const clampView = () => { vStart = clamp(vStart, 0, Math.max(0, viewLength - vLen())); };
    function setZoom (z, aroundT) {
      const old = vLen(); zoom = clamp(z, 1, 64);
      const at = aroundT === undefined ? vStart + old / 2 : aroundT, frac = (at - vStart) / old;
      vStart = at - frac * vLen(); clampView(); marksSig = ""; for (const r of rowEls.values()) r.sig = ""; update(false);
    }
    const timeAtX = (clientX) => { const r = marks.getBoundingClientRect(); return vStart + clamp((clientX - r.left) / r.width, 0, 1) * vLen(); };

    function layout () {
      const w = W * scale;
      Object.assign(pnl.style, { left: px(stageX), width: px(w), top: px(stageY + H * scale + 6) });
      document.body.style.height = open ? px(stageY + H * scale + 6 + pnl.offsetHeight + 40) : "";
      // otvoreny panel: zalozka sedi nad jeho hornym okrajom (neprekryva text), zatvoreny: dole v okne
      if (open) Object.assign(tab.style, { position: "absolute", bottom: "auto", top: px(stageY + H * scale + 6 - 22) });
      else Object.assign(tab.style, { position: "", bottom: "", top: "" });
      tab.textContent = open ? "▼ STOPY  (T)" : "▲ STOPY  (T)";
      tab.classList.toggle("open", open);
    }
    function toggle (force) {
      open = force === undefined ? !open : !!force; pnl.hidden = !open; update(true); layout();
      if (open) setTimeout(() => pnl.scrollIntoView({ behavior: "smooth", block: "end" }), 30);
      else scrollTo({ top: 0, behavior: "smooth" });
    }
    tab.addEventListener("click", () => toggle());
    pnl.querySelector(".tp-btns").addEventListener("click", (ev) => {
      const a = ev.target.dataset && ev.target.dataset.a; if (!a || ev.target.disabled) return;
      if (a === "add") send("trackAdd");
      else if (a === "rename") renameTrack();
      else if (a === "delete") deleteSelected();
      else if (a === "undo") send("undo");
      else if (a === "redo") send("redo");
      else if (a === "zin") setZoom(zoom * 2, st && st.transport ? livePos() : undefined);
      else if (a === "zout") setZoom(zoom / 2, st && st.transport ? livePos() : undefined);
      else if (a === "zfit") { zoom = 1; vStart = 0; setZoom(1); }
    });
    // Ctrl + koliesko = zoom okolo kurzora, koliesko = posun v case (ked je priblizene)
    pnl.addEventListener("wheel", (ev) => {
      if (!ev.target.closest(".tp-wave, .tp-ruler")) return;
      if (ev.ctrlKey) { ev.preventDefault(); setZoom(ev.deltaY < 0 ? zoom * 1.5 : zoom / 1.5, timeAtX(ev.clientX)); }
      else if (zoom > 1) { ev.preventDefault(); vStart += (ev.deltaY || ev.deltaX) / 400 * vLen(); clampView(); marksSig = ""; for (const r of rowEls.values()) r.sig = ""; update(false); }
    }, { passive: false });
    // klik do casovej osi = presun hlavy
    function seekTo (t) { if (rec()) return; send("seek", { seconds: Math.max(0, t) }); if (st && st.transport) { st.transport.position = t; posBase = t; posAt = performance.now(); } head(t); }
    ruler.addEventListener("click", (ev) => seekTo(timeAtX(ev.clientX)));

    function laneWidth () { return Math.max(100, pnl.clientWidth - NAME_W - 24); }
    function head (p) {
      if (!open) return;
      const w = laneWidth(), x = (p - vStart) / vLen();
      headR.style.display = x < 0 || x > 1 ? "none" : "block";
      headR.style.left = px(NAME_W + clamp(x, 0, 1) * w);
      // pri prehravani s priblizenim drzime hlavu v obraze
      if (zoom > 1 && st && st.transport && st.transport.playing && (x > 0.92 || x < 0)) { vStart = p - vLen() * 0.1; clampView(); marksSig = ""; for (const r of rowEls.values()) r.sig = ""; update(false); }
    }
    function drawRow (r, t, w) {
      const sig = t.id + ":" + t.clips.map(c => c.start + "/" + c.length + "/" + (c.peaks ? c.peaks.length : 0)).join(",") + ":" + viewLength + ":" + w + ":" + vStart.toFixed(3) + ":" + zoom;
      if (r.sig === sig) return; r.sig = sig;
      const cv = r.cv, dpr = 2; cv.width = w * dpr; cv.height = 40 * dpr; cv.style.width = px(w);
      const g = cv.getContext("2d"); g.clearRect(0, 0, cv.width, cv.height);
      const sx = cv.width / vLen(), mid = cv.height / 2;
      for (const c of t.clips) {
        const x0 = (c.start - vStart) * sx, cw = Math.max(2, c.length * sx);
        if (x0 > cv.width || x0 + cw < 0) continue;
        g.fillStyle = "rgba(255,170,60,.10)"; g.fillRect(x0, 2, cw, cv.height - 4);
        g.strokeStyle = "rgba(255,170,60,.45)"; g.lineWidth = 1; g.strokeRect(x0 + .5, 2.5, cw - 1, cv.height - 5);
        const pk = c.peaks || []; if (!pk.length) continue;
        g.fillStyle = "#ffb547"; const step = cw / pk.length;
        for (let i = 0; i < pk.length; i++) { const xi = x0 + i * step; if (xi + step < 0 || xi > cv.width) continue; const h = Math.max(1, pk[i] * (mid - 6)); g.fillRect(xi, mid - h, Math.max(1, step * .85), h * 2); }
      }
    }
    function update (force) {
      if (!open || !st) return;
      const lp = st.loop || {}, t = st.transport || {}, sel = selectedId(), h = st.history || {};
      const w = laneWidth(), anySolo = tracks.some(x => x.solo);
      pnl.querySelector(".tp-song").textContent = (st.song ? st.song.name : "") + (st.song && st.song.isDraft ? "  (nepomenovaná – PROJECT › Uložiť ako…)" : "");
      const b = (n) => pnl.querySelector(`[data-a="${n}"]`);
      b("undo").disabled = !h.canUndo || rec(); b("undo").textContent = "↶ UNDO" + (h.canUndo ? "" : "");
      b("undo").title = h.undoLabel ? "Vrátiť: " + h.undoLabel : "";
      b("redo").disabled = !h.canRedo || rec(); b("redo").title = h.redoLabel ? "Obnoviť: " + h.redoLabel : "";
      b("rename").disabled = !selectedTrack(); b("delete").disabled = !selectedTrack() || rec(); b("add").disabled = rec();
      // casova os (takty)
      const barLen = st.tempo ? 60 / st.tempo.bpm * st.tempo.numerator * (4 / st.tempo.denominator) : 2;
      clampView();
      const msig = w + ":" + barLen + ":" + viewLength + ":" + vStart.toFixed(3) + ":" + zoom;
      if (msig !== marksSig) {
        marksSig = msig; let step = 1; while (vLen() / (barLen * step) > 24) step *= 2; let hh = "";
        const k0 = Math.floor(vStart / barLen / step) * step;
        for (let k = k0; k * barLen <= vStart + vLen() + 1e-6; k += step) { const x = (k * barLen - vStart) / vLen() * w; if (x >= -1) hh += `<span style="left:${x.toFixed(1)}px">${k + 1}</span>`; }
        marks.innerHTML = hh;
      }
      pnl.querySelector('[data-a="zout"]').disabled = zoom <= 1; pnl.querySelector('[data-a="zfit"]').disabled = zoom <= 1;
      marks.style.width = px(w); marks.style.left = px(NAME_W);
      loopR.style.display = lp.end > lp.start ? "block" : "none";
      loopR.classList.toggle("off", !lp.on);
      const lx0 = clamp((lp.start - vStart) / vLen(), 0, 1), lx1 = clamp((lp.end - vStart) / vLen(), 0, 1);
      loopR.style.left = px(NAME_W + lx0 * w); loopR.style.width = px(Math.max(2, (lx1 - lx0) * w));
      if (!t.playing) head(t.position || 0);
      // riadky
      const seen = new Set();
      tracks.forEach((tr, i) => {
        seen.add(tr.id);
        let r = rowEls.get(tr.id);
        if (!r) {
          const row = el("div", "tp-row", rowsEl);
          row.innerHTML = `<div class="tp-name"><span class="n"></span><span class="nm"></span><span class="ms"><button class="m">M</button><button class="s">S</button></span></div><div class="tp-wave"><canvas></canvas></div>`;
          r = { row, cv: row.querySelector("canvas"), nm: row.querySelector(".nm"), n: row.querySelector(".n"), m: row.querySelector(".m"), s: row.querySelector(".s"), sig: "" };
          row.addEventListener("click", (ev) => {
            if (ev.target.tagName === "BUTTON") return;
            pickTrack(tracks.find(x => x.id === tr.id) || tr);
            if (ev.target.closest(".tp-wave")) seekTo(timeAtX(ev.clientX));
          });
          r.nm.addEventListener("dblclick", () => renameTrack(tracks.find(x => x.id === tr.id)));
          r.m.addEventListener("click", () => { const x = tracks.find(y => y.id === tr.id); if (x) send("trackMute", { trackId: x.id, muted: !x.muted }); });
          r.s.addEventListener("click", () => { const x = tracks.find(y => y.id === tr.id); if (x) send("trackSolo", { trackId: x.id, solo: !x.solo }); });
          rowEls.set(tr.id, r);
        }
        if (rowsEl.children[i] !== r.row) rowsEl.insertBefore(r.row, rowsEl.children[i] || null);
        r.n.textContent = i + 1; r.nm.textContent = tr.name;
        r.row.classList.toggle("sel", tr.id === sel); r.row.classList.toggle("rec", !!tr.recording);
        r.row.classList.toggle("quiet", !!tr.muted || (anySolo && !tr.solo));
        r.m.classList.toggle("on", !!tr.muted); r.s.classList.toggle("on", !!tr.solo);
        drawRow(r, tr, w);
      });
      for (const [id, r] of rowEls) if (!seen.has(id)) { r.row.remove(); rowEls.delete(id); }
      foot.textContent = tracks.length ? "Klik do vlny = vybrať stopu a presunúť hlavu · dvojklik na názov = premenovať · Ctrl + koliesko = ZOOM, koliesko = posun · Ctrl+Z / Ctrl+Y = vrátiť / obnoviť"
                                       : "Zatiaľ žiadna stopa – stlač REC na konzole.";
      if (force) layout();
    }
    let lastH = 0;
    return { layout, toggle, head, update: () => { update(false); if (open && pnl.offsetHeight !== lastH) { lastH = pnl.offsetHeight; layout(); } }, get open () { return open; } };
  })();

  // ------------------------------------------------------------------ motor
  let lastStatus = "";
  Rafa.on("state", (s) => {
    const stx = s && s.status ? s.status.text : "";
    if (stx && stx !== lastStatus && lastStatus !== "") flashValue(stx, s.status.level === "error" || s.status.level === "warn" ? 3500 : 2200);
    lastStatus = stx || lastStatus;
    st = s;
    if (s.transport) { posBase = s.transport.position || 0; posAt = performance.now(); }
    if (pendingSongList && s.songList) { const fn = pendingSongList; pendingSongList = null; fn(s.songList); } if (menu && ["project", "tracks", "effects", "settings"].includes(menu.id) && !menu.parent) { const m = buildMenu(menu.id); m.sel = menu.sel; menu = m; drawMenuQuiet(); } render(); });
  Rafa.on("tracks", (t) => {
    tracks = (t && t.tracks) || []; viewLength = (t && t.viewLength) || 30;
    if (page * 4 >= tracks.length) page = Math.max(0, Math.ceil(tracks.length / 4) - 1);
    if (localSel && !tracks.some(x => x.id === localSel)) localSel = null;
    for (const le of laneEls) le.sig = "";
    render();
  });
  fit();
  send("uiReady");
  window.RafaConsole = { render, get state () { return st; }, get tracks () { return tracks; } };
})();
