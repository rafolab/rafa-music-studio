/* RAFA MUSIC STUDIO · etapa 1c · obrazovka Model 5.
 * Motor komunikácie vlastní Claude. Tento súbor používa výhradne Rafa.send() a Rafa.on().
 * Žiadne nahrávanie, ukladanie ani spracovanie zvuku sa tu nevykonáva.
 */
(() => {
  'use strict';

  const $ = id => document.getElementById(id);
  const state = { engine: null, tracks: [], viewLength: 30, selectedTrackId: null };
  const elements = {
    environment: $('environment'), song: $('songName'), time: $('transportTime'),
    length: $('screenLength'), screenMode: $('screenMode'), lanes: $('screenLanes'),
    screenPlayhead: $('screenPlayhead'), input: $('inputSelect'), monitor: $('monitorCheck'),
    meter: $('digitalMeter'), meterFill: $('meterFill'), dbValue: $('dbValue'),
    deviceHealth: $('deviceHealth'), deviceSummary: $('deviceSummary'),
    status: $('statusText'), statusLed: $('statusLed'), sessionMode: $('sessionMode'),
    projectMenu: $('projectMenu'), projectButton: $('projectButton'), tracksButton: $('tracksButton'),
    tracksPanel: $('tracksPanel'), trackRows: $('trackRows'),
    vuL: $('vuNeedleL'), vuR: $('vuNeedleR'), record: $('recordButton'), play: $('playButton')
  };

  const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));
  const fmt = v => {
    const s = Math.max(0, Number(v) || 0);
    const minutes = Math.floor(s / 60);
    const seconds = Math.floor(s % 60);
    const millis = Math.floor((s % 1) * 1000);
    return `${minutes}:${String(seconds).padStart(2, '0')}.${String(millis).padStart(3, '0')}`;
  };
  const duration = v => {
    const n = Math.max(0, Number(v) || 0);
    return `${Math.floor(n / 60)}:${String(Math.floor(n % 60)).padStart(2, '0')}`;
  };
  const safeText = value => String(value ?? '');
  const canEdit = () => state.engine?.busy?.editingAllowed !== false;
  const canDiagnose = () => state.engine?.busy?.diagnosticsAllowed !== false;
  const getPosition = () => Number(state.engine?.transport?.position) || 0;

  function send(command, payload) {
    try { Rafa.send(command, payload); }
    catch (error) { elements.status.textContent = `Príkaz sa nepodaril: ${error.message}`; }
  }

  function closeProjectMenu() {
    elements.projectMenu.hidden = true;
    elements.projectButton.setAttribute('aria-expanded', 'false');
  }
  function openTracksPanel(open = true) {
    elements.tracksPanel.hidden = !open;
    elements.tracksButton.setAttribute('aria-expanded', String(open));
    if (open) elements.tracksPanel.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
  }

  // Ovládanie je vždy podľa príkazov, ktoré už existujú v API v1.
  document.querySelectorAll('[data-command]').forEach(button => {
    button.addEventListener('click', () => {
      if ((button.dataset.edit && !canEdit()) || (button.dataset.diagnostics && !canDiagnose())) return;
      send(button.dataset.command);
      closeProjectMenu();
    });
  });
  elements.projectButton.addEventListener('click', () => {
    const open = elements.projectMenu.hidden;
    elements.projectMenu.hidden = !open;
    elements.projectButton.setAttribute('aria-expanded', String(open));
  });
  elements.tracksButton.addEventListener('click', () => openTracksPanel(elements.tracksPanel.hidden));
  document.addEventListener('pointerdown', ev => {
    if (!elements.projectMenu.hidden && !elements.projectMenu.contains(ev.target)
      && ev.target !== elements.projectButton) closeProjectMenu();
  });
  elements.input.addEventListener('change', () => {
    if (canEdit() && elements.input.value) send('selectInput', { id: elements.input.value });
  });
  elements.monitor.addEventListener('change', () => send('setMonitoring', { on: elements.monitor.checked }));

  // Klávesy nepoužívame pri písaní do polí alebo v otvorenom dialógu.
  document.addEventListener('keydown', event => {
    const target = event.target;
    if (event.ctrlKey || event.altKey || event.metaKey || event.repeat) return;
    if (target instanceof Element && (target.closest('input,textarea,select,[contenteditable="true"]'))) return;
    if (event.key === 'Escape') { closeProjectMenu(); return; }
    const key = event.key.toLowerCase();
    if (key === 'r') { event.preventDefault(); send('record'); }
    else if (event.code === 'Space') { event.preventDefault(); send('togglePlay'); }
    else if (event.key === 'Home') { event.preventDefault(); send('rewind'); }
  });

  function updateInputs(s) {
    const inputs = Array.isArray(s.inputs) ? s.inputs : [];
    const ids = inputs.map(i => String(i.id));
    const oldIds = Array.from(elements.input.options, o => o.value);
    if (JSON.stringify(ids) !== JSON.stringify(oldIds)) {
      elements.input.replaceChildren();
      if (!inputs.length) elements.input.add(new Option('Žiadny vstup – otvor Nastavenia zvuku', ''));
      for (const i of inputs) elements.input.add(new Option(safeText(i.name), String(i.id)));
    }
    const selected = safeText(s.selectedInput);
    if (selected && ids.includes(selected) && elements.input.value !== selected)
      elements.input.value = selected;
    elements.input.disabled = !canEdit() || !inputs.length;
  }

  function updateMeter(s) {
    const db = Number(s.meter?.inputDb);
    const level = Number.isFinite(db) ? clamp(db, -100, 0) : -100;
    const pct = level <= -60 ? 0 : (level + 60) / 60 * 100;
    elements.meterFill.style.width = `${pct}%`;
    elements.meterFill.className = 'meter-fill' + (level >= -3 ? ' clip' : level >= -12 ? ' caution' : '');
    elements.meter.setAttribute('aria-valuenow', String(Math.round(level)));
    elements.dbValue.textContent = level <= -90 ? '−∞ dB' : `${level.toFixed(1).replace('-', '−')} dB`;
    // Ručičky sú dynamickým naznačením signálu; číselný merač ukazuje presné API dB.
    const angle = clamp((level + 60) / 60, 0, 1) * 75 - 39;
    elements.vuL.style.transform = `rotate(${angle}deg)`;
    elements.vuR.style.transform = `rotate(${angle * 0.94}deg)`;
  }

  function updateDevice(s) {
    const d = s.device || {};
    const ok = !!d.ok;
    const rate = Number(d.sampleRate) || 0;
    const buffer = Number(d.buffer) || 0;
    const latency = Number(d.latencyMs);
    const cpu = Number(d.cpu);
    elements.deviceHealth.textContent = ok ? `● Zariadenie pripravené · ${rate ? (rate / 1000).toFixed(1) + ' kHz' : 'bez frekvencie'}` :
      '● Nie je pripojené zvukové zariadenie – otvor Nastavenia zvuku';
    elements.deviceHealth.classList.toggle('warning', !ok);
    const bits = [
      `${safeText(d.driver || '—')} · ${safeText(d.input || 'vstup neurčený')}`,
      `Výstup: ${safeText(d.output || '—')}`,
      `${rate ? rate / 1000 + ' kHz' : '— kHz'} · ${buffer || '—'} vzoriek · latencia ${Number.isFinite(latency) ? latency.toFixed(1) : '—'} ms · CPU ${Number.isFinite(cpu) ? cpu.toFixed(0) : '—'} %`
    ];
    elements.deviceSummary.replaceChildren();
    for (const line of bits) { const el = document.createElement('div'); el.textContent = line; elements.deviceSummary.append(el); }
  }

  function renderState(s) {
    if (!s || typeof s !== 'object') return;
    state.engine = s;
    const recording = !!s.transport?.recording;
    const playing = !!s.transport?.playing;
    document.body.classList.toggle('is-recording', recording);
    document.body.classList.toggle('is-playing', playing);
    elements.song.textContent = safeText(s.song?.name || 'Bez názvu');
    elements.time.textContent = fmt(s.transport?.position);
    elements.length.textContent = `/ ${duration(s.song?.length)}`;
    elements.screenMode.textContent = recording ? '● NAHRÁVANIE' : playing ? '▶ PREHRÁVANIE' : '● PRIPRAVENÉ';
    elements.screenMode.classList.toggle('record-mode', recording);
    elements.record.setAttribute('aria-label', recording ? 'Ukončiť nahrávanie' : 'NAHRÁVAŤ');
    elements.play.setAttribute('aria-label', playing ? 'Zastaviť' : 'PREHRAŤ');
    elements.monitor.checked = !!s.monitoring;
    elements.environment.textContent = Rafa.isSimulation ? 'SIMULÁCIA · bez zvuku' : 'PRIPOJENÝ MOTOR';
    elements.environment.classList.toggle('sim', Rafa.isSimulation);
    elements.sessionMode.textContent = Rafa.isSimulation ? 'SIMULAČNÝ REŽIM · API v1' : `API v${s.apiVersion || 1}`;
    elements.status.textContent = safeText(s.status?.text || 'Pripravené.');
    elements.statusLed.dataset.level = safeText(s.status?.level || 'info');
    updateInputs(s);
    updateMeter(s);
    updateDevice(s);
    document.querySelectorAll('[data-edit]').forEach(b => b.disabled = !canEdit());
    document.querySelectorAll('[data-diagnostics]').forEach(b => b.disabled = !canDiagnose());
    document.querySelectorAll('.delete-track,.rename-track').forEach(b => b.disabled = !canEdit());
    updatePlayhead();
  }

  function trackForSlot(slot) { return state.tracks[slot] || null; }
  function updateSlots() {
    document.querySelectorAll('[data-slot]').forEach(button => {
      const track = trackForSlot(Number(button.dataset.slot));
      button.disabled = !track;
      button.classList.toggle('selected', !!track && track.id === state.selectedTrackId);
      button.title = track ? `Detail vrstvy: ${safeText(track.name)}` : 'Táto vrstva ešte nie je nahraná';
    });
    document.querySelectorAll('[data-mute-slot]').forEach(button => {
      const track = trackForSlot(Number(button.dataset.muteSlot));
      button.disabled = !track;
      button.classList.toggle('muted', !!track?.muted);
      button.setAttribute('aria-label', track ? `${track.muted ? 'Zapnúť' : 'Stlmiť'} stopu ${track.name}` : 'Zatiaľ nie je nahraná stopa');
    });
  }
  document.querySelectorAll('[data-slot]').forEach(button => button.addEventListener('click', () => {
    const track = trackForSlot(Number(button.dataset.slot));
    if (!track) return;
    state.selectedTrackId = String(track.id);
    renderTracks();
    openTracksPanel(true);
  }));
  document.querySelectorAll('[data-mute-slot]').forEach(button => button.addEventListener('click', () => {
    const track = trackForSlot(Number(button.dataset.muteSlot));
    if (track) send('trackMute', { trackId: String(track.id), muted: !track.muted });
  }));

  function createWaveCanvas(clip, fullLength, className = '') {
    const canvas = document.createElement('canvas');
    canvas.className = `wave-canvas ${className}`;
    canvas.dataset.viewLength = String(fullLength);
    canvas.setAttribute('aria-hidden', 'true');
    canvas._clips = [clip];
    requestAnimationFrame(() => drawWave(canvas));
    return canvas;
  }
  function drawWave(canvas) {
    if (!canvas.isConnected) return;
    const rect = canvas.getBoundingClientRect();
    if (!rect.width || !rect.height) return;
    const ratio = Math.min(window.devicePixelRatio || 1, 2);
    const w = Math.max(1, Math.round(rect.width * ratio));
    const h = Math.max(1, Math.round(rect.height * ratio));
    if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; }
    const ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, w, h);
    const total = Math.max(1, Number(canvas.dataset.viewLength));
    ctx.fillStyle = canvas.classList.contains('large') ? '#e4a331' : '#f7a83d';
    for (const clip of canvas._clips || []) {
      const peaks = Array.isArray(clip.peaks) ? clip.peaks : [];
      if (!peaks.length) continue;
      const start = Number(clip.start) || 0;
      const len = Math.max(0, Number(clip.length) || 0);
      const startX = start / total * w;
      const width = len / total * w;
      for (let i = 0; i < peaks.length; i++) {
        const x = startX + i / peaks.length * width;
        const barW = Math.max(1, width / peaks.length * 0.75);
        const p = clamp(Number(peaks[i]) || 0, 0, 1);
        const bh = Math.max(1, p * h * 0.86);
        ctx.fillRect(x, (h - bh) / 2, barW, bh);
      }
    }
  }
  function waveFromClips(clips, fullLength, large = false) {
    const canvas = document.createElement('canvas');
    canvas.className = 'wave-canvas' + (large ? ' large' : '');
    canvas._clips = clips;
    canvas.dataset.viewLength = String(fullLength);
    requestAnimationFrame(() => drawWave(canvas));
    return canvas;
  }
  function seekByClick(event, element) {
    if (state.engine?.transport?.recording) return;
    const rect = element.getBoundingClientRect();
    if (rect.width <= 0) return;
    const share = clamp((event.clientX - rect.left) / rect.width, 0, 1);
    send('seek', { seconds: share * state.viewLength });
  }

  function renderTracks() {
    elements.lanes.replaceChildren();
    elements.trackRows.replaceChildren();
    if (!state.tracks.length) {
      const message = document.createElement('div');
      message.className = 'screen-empty';
      message.textContent = 'Zatiaľ žiadna nahrávka. Vyber gitarový vstup a stlač REC.';
      elements.lanes.append(message);
      const text = document.createElement('p'); text.className = 'track-empty';
      text.textContent = 'Prvá nahrávka vytvorí prvú stopu. Ďalšie REC pridá ďalšiu vrstvu.';
      elements.trackRows.append(text);
      updateSlots();
      return;
    }
    const visible = state.tracks.slice(0, 4);
    visible.forEach((track, index) => {
      const row = document.createElement('button');
      row.type = 'button'; row.className = 'screen-track';
      row.title = 'Zobraziť detail stopy';
      row.setAttribute('aria-label', `Zobraziť detaily stopy ${track.name}`);
      if (track.recording) row.classList.add('recording');
      if (track.muted) row.classList.add('is-muted');
      const label = document.createElement('span'); label.className = 'screen-track-label';
      label.textContent = `${index + 1} ${safeText(track.name)}`;
      row.append(label, waveFromClips(track.clips || [], state.viewLength));
      row.addEventListener('click', () => { state.selectedTrackId = String(track.id); renderTracks(); openTracksPanel(true); });
      elements.lanes.append(row);
    });
    if (state.tracks.length > 4) {
      const more = document.createElement('button'); more.type = 'button'; more.className = 'screen-more';
      more.textContent = `+ ${state.tracks.length - 4} ďalších stôp · STOPY`;
      more.onclick = () => openTracksPanel(true);
      elements.lanes.append(more);
    }
    for (const track of state.tracks) {
      const row = document.createElement('article'); row.className = 'track-row';
      if (track.recording) row.classList.add('recording');
      if (track.id === state.selectedTrackId) row.classList.add('selected-track');
      const heading = document.createElement('div'); heading.className = 'track-row-head';
      const name = document.createElement('strong'); name.textContent = safeText(track.name);
      if (track.recording) { const badge = document.createElement('span'); badge.className = 'record-badge'; badge.textContent = '● NAHRÁVA'; heading.append(badge); }
      heading.prepend(name);
      const buttons = document.createElement('div'); buttons.className = 'track-tools';
      const mute = document.createElement('button'); mute.type = 'button'; mute.textContent = track.muted ? 'Zapnúť' : 'Stlmiť';
      mute.className = 'rack-button' + (track.muted ? ' active' : '');
      mute.addEventListener('click', () => send('trackMute', { trackId: String(track.id), muted: !track.muted }));
      const rename = document.createElement('button'); rename.type = 'button'; rename.textContent = 'Premenovať'; rename.className = 'rack-button rename-track'; rename.disabled = !canEdit();
      rename.addEventListener('click', () => renameTrack(track));
      const del = document.createElement('button'); del.type = 'button'; del.textContent = 'Zmazať'; del.className = 'rack-button danger delete-track'; del.disabled = !canEdit();
      del.addEventListener('click', () => send('trackDelete', { trackId: String(track.id) }));
      buttons.append(mute, rename, del);
      const wave = document.createElement('div'); wave.className = 'track-wave';
      wave.title = 'Kliknutím presunieš prehrávaciu hlavu';
      wave.append(waveFromClips(track.clips || [], state.viewLength, true));
      wave.addEventListener('click', e => seekByClick(e, wave));
      const head = document.createElement('div'); head.className = 'track-row-top'; head.append(heading, buttons);
      row.append(head, wave); elements.trackRows.append(row);
    }
    updateSlots();
  }

  function renameTrack(track) {
    if (!canEdit()) return;
    // Zmena názvu je ľudsky čitateľná; motor dostane len výsledný text podľa API.
    const answer = window.prompt('Nový názov stopy:', safeText(track.name));
    if (answer === null) return;
    const value = answer.trim().slice(0, 80);
    if (value && value !== track.name) send('trackRename', { trackId: String(track.id), name: value });
  }

  function updatePlayhead() {
    const ratio = clamp(getPosition() / Math.max(1, state.viewLength), 0, 1);
    elements.screenPlayhead.style.left = `${28 + ratio * 70}%`;
    document.querySelectorAll('.track-wave').forEach(wave => {
      wave.style.setProperty('--playhead', `${ratio * 100}%`);
    });
  }

  function renderTrackData(payload) {
    state.tracks = Array.isArray(payload?.tracks) ? payload.tracks : [];
    const length = Number(payload?.viewLength);
    state.viewLength = Number.isFinite(length) && length > 0 ? length : 30;
    if (state.selectedTrackId && !state.tracks.some(t => String(t.id) === state.selectedTrackId))
      state.selectedTrackId = null;
    renderTracks();
    updatePlayhead();
  }

  window.addEventListener('resize', () => {
    document.querySelectorAll('.wave-canvas').forEach(drawWave);
  });

  // Bridge sa načíta ako prvý skript v HTML; nerozširujeme ho ani neobchádzame.
  if (typeof Rafa === 'undefined') {
    elements.environment.textContent = 'CHÝBA RAFA BRIDGE';
    elements.status.textContent = 'V priečinku ui/ musí byť nezmenený súbor rafa-bridge.js z repozitára.';
    document.querySelectorAll('button,select,input').forEach(el => el.disabled = true);
    return;
  }
  Rafa.on('state', renderState);
  Rafa.on('tracks', renderTrackData);
})();
