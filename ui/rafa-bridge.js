/*
    RAFA MUSIC STUDIO – most medzi obrazovkou (HTML) a motorom (C++).
    Vlastnik: Claude. Obrazovka (ui/app.js) tento subor iba POUZIVA, nemeni.
    Popis prikazov a dat: ui/API.md

    Pouzitie:
        Rafa.send("play");
        Rafa.send("selectInput", { id: "in-1" });
        Rafa.on("state",  s => ...);
        Rafa.on("tracks", t => ...);
        Rafa.isSimulation   // true = otvorene v prehliadaci bez programu
*/
(function () {
    "use strict";

    const API_VERSION = 4;
    const listeners = { state: [], tracks: [] };
    const hasEngine = typeof window.__JUCE__ !== "undefined"
                   && window.__JUCE__.backend
                   && typeof window.__JUCE__.backend.emitEvent === "function";

    function dispatch (eventName, data) {
        (listeners[eventName] || []).forEach(fn => {
            try { fn(data); } catch (e) { console.error("[Rafa] chyba v obsluhe '" + eventName + "':", e); }
        });
    }

    const Rafa = {
        apiVersion: API_VERSION,
        isSimulation: ! hasEngine,

        send (command, args) {
            if (typeof command !== "string" || command.length === 0)
                throw new Error("Rafa.send: chyba nazov prikazu");

            if (hasEngine)
                window.__JUCE__.backend.emitEvent("rafaCommand", { command: command, args: args || {} });
            else
                simulation.handle(command, args || {});
        },

        on (eventName, fn) {
            if (! listeners[eventName])
                throw new Error("Rafa.on: neznama udalost '" + eventName + "' (povolene: state, tracks)");

            listeners[eventName].push(fn);

            // ak uz mame posledny stav, posleme ho hned
            if (lastData[eventName] !== undefined)
                setTimeout(() => fn(lastData[eventName]), 0);

            return () => {
                listeners[eventName] = listeners[eventName].filter(f => f !== fn);
            };
        }
    };

    const lastData = {};

    function receive (eventName, data) {
        lastData[eventName] = data;
        dispatch(eventName, data);
    }

    if (hasEngine) {
        window.__JUCE__.backend.addEventListener("rafaState",  d => receive("state", d));
        window.__JUCE__.backend.addEventListener("rafaTracks", d => receive("tracks", d));
        // ohlasime motoru, ze obrazovka je nacitana a chce cely stav
        window.addEventListener("DOMContentLoaded", () =>
            window.__JUCE__.backend.emitEvent("rafaCommand", { command: "uiReady", args: { apiVersion: API_VERSION } }));
    }

    // =========================================================================
    // SIMULACNY REZIM – ked sa ui/index.html otvori priamo v prehliadaci.
    // Spravanie napodobuje motor, aby sa dal ladit dizajn bez programu.
    // =========================================================================
    const simulation = (function () {
        if (hasEngine)
            return { handle () {} };

        let nextId = 1001;
        const s = {
            apiVersion: API_VERSION,
            transport: { playing: false, recording: false, position: 0, countingIn: false },
            song: { name: "Skladba (simulácia)", length: 0 },
            meter: { inputDb: -100 },
            inputs: [ { id: "in-1", name: "Input 1 (BEAM Mini – simulácia)" },
                      { id: "in-2", name: "Input 2 (BEAM Mini – simulácia)" } ],
            selectedInput: "in-1",
            monitoring: false,
            device: { ok: true, driver: "Windows Audio (simulácia)", input: "BEAM Mini", output: "Reproduktory",
                      sampleRate: 48000, buffer: 480, latencyMs: 32.5, cpu: 3 },
            status: { text: "Simulácia – program nie je pripojený, dáta sú vymyslené.", level: "info" },
            busy: { diagnosticsAllowed: true, editingAllowed: true },
            // --- API v2 ---
            tempo: { bpm: 120, numerator: 4, denominator: 4 },
            metronome: { on: false, volume: 0.5, countInBars: 1 },
            loop: { on: false, start: 0, end: 8, lengthBars: 4, canRecord: true, recordingPass: false },
            musicalPosition: { bar: 1, beat: 1 },
            // --- API v3 ---
            mixer: { masterDb: 0, masterLevelDb: -100, masterLevelL: -100, masterLevelR: -100, trackLevels: [] },
            // --- API v4 ---
            selectedTrackId: "",
            inputGainDb: 0
        };
        let recordStart = 0;      // kde zacina nahravanie (po odpocitani)
        let loopWrapped = false;
        let lastPos = 0;
        let tracks = [];
        let recordingTrack = null;

        function fakePeaks (n, seed) {
            const out = [];
            for (let i = 0; i < n; i++) {
                const env = 0.35 + 0.35 * Math.sin(i / 9 + seed) * Math.sin(i / 37 + seed * 2);
                out.push(Math.max(0.02, Math.min(1, Math.abs(env + (Math.random() - 0.5) * 0.3))));
            }
            return out;
        }

        function songLength () {
            return tracks.reduce((m, t) => Math.max(m, ...t.clips.map(c => c.start + c.length), 0), 0);
        }

        function emitTracks () {
            s.song.length = songLength();
            receive("tracks", {
                tracks: JSON.parse(JSON.stringify(tracks)),
                viewLength: Math.max(30, s.song.length + 5, s.transport.position + 5)
            });
        }

        function setStatus (text, level) { s.status = { text: text, level: level || "info" }; }

        const secPerBeat = () => 60 / s.tempo.bpm * 4 / s.tempo.denominator;
        const secPerBar  = () => secPerBeat() * s.tempo.numerator;
        const snapBeat   = t => Math.round(Math.max(0, t) / secPerBeat()) * secPerBeat();

        function updateLoopInfo () {
            const len = s.loop.end - s.loop.start;
            s.loop.lengthBars = Math.round(len / secPerBar() * 100) / 100;
            s.loop.canRecord = len >= 2;
        }

        function setLoopRange (start, end) {
            if (s.transport.recording) return;
            start = Math.max(0, start);
            if (end <= start + 0.1) return setStatus("LOOP: koniec slučky musí byť za začiatkom.", "warn");
            s.loop.start = start; s.loop.end = end; updateLoopInfo();
        }

        function finishLoopPass () {
            s.loop.recordingPass = false;
            s.transport.recording = false;
            if (recordingTrack) {
                recordingTrack.recording = false;
                recordingTrack.clips = [ { start: s.loop.start, length: s.loop.end - s.loop.start, peaks: fakePeaks(200, nextId) } ];
                setStatus("Vrstva uložená (" + (s.loop.end - s.loop.start).toFixed(1) + " s) – slučka hrá ďalej. REC = ďalšia vrstva.", "ok");
                recordingTrack = null;
                emitTracks();
            }
        }

        function stopAll () {
            const wasRecording = s.transport.recording;
            s.loop.recordingPass = false;
            s.transport.playing = false;
            s.transport.recording = false;

            if (wasRecording && recordingTrack) {
                const len = Math.max(0.5, s.transport.position - recordStart);
                recordingTrack.recording = false;
                recordingTrack.clips = [ { start: 0, length: len, peaks: fakePeaks(200, nextId) } ];
                setStatus("Nahrávka uložená (" + len.toFixed(1) + " s). Stlač PREHRAŤ alebo nahraj ďalšiu vrstvu.", "ok");
                recordingTrack = null;
                emitTracks();
            } else {
                setStatus("Zastavené.");
            }
        }

        const handlers = {
            record () {
                if (s.transport.recording) return s.loop.recordingPass ? finishLoopPass() : stopAll();
                if (s.loop.on && ! s.loop.canRecord) return setStatus("Slučka je príliš krátka na nahrávanie (minimum 2 s). Predĺž ju.", "warn");
                // v4: vybraná prázdna vrstva, inak posledná prázdna, inak nová
                const empty = tracks.find(t => t.id === s.selectedTrackId && ! t.clips.length) || (tracks.length && ! tracks[tracks.length - 1].clips.length ? tracks[tracks.length - 1] : null);
                if (empty) { recordingTrack = empty; recordingTrack.recording = true; if (/^Stopa /.test(recordingTrack.name)) recordingTrack.name = "Gitara " + (tracks.indexOf(empty) + 1); }
                else { recordingTrack = { id: String(nextId++), name: "Gitara " + (tracks.length + 1), muted: false, solo: false, volumeDb: 0, pan: 0, recording: true, clips: [] }; tracks.push(recordingTrack); }
                s.selectedTrackId = recordingTrack.id;
                recordStart = s.loop.on ? s.loop.start : 0;
                s.transport.position = recordStart - s.metronome.countInBars * secPerBar();
                s.transport.playing = true;
                s.transport.recording = true;
                s.loop.recordingPass = s.loop.on;
                loopWrapped = false;
                lastPos = s.transport.position;
                setStatus(s.loop.on ? "● Nahrávam vrstvu slučky do stopy „" + recordingTrack.name + "“ – jeden prechod, potom hrá ďalej."
                                    : "● Nahrávam od začiatku do stopy „" + recordingTrack.name + "“ – STOP ukončí nahrávanie.", "recording");
                emitTracks();
            },
            play () {
                if (s.transport.recording) return;
                if (s.loop.on && (s.transport.position < s.loop.start || s.transport.position >= s.loop.end)) s.transport.position = s.loop.start;
                s.transport.playing = true; setStatus(s.loop.on ? "Prehrávam slučku…" : "Prehrávam…", "ok");
            },
            stop () { stopAll(); },
            togglePlay () { (s.transport.playing || s.transport.recording) ? stopAll() : handlers.play(); },
            rewind () { if (! s.transport.recording) s.transport.position = s.loop.on ? s.loop.start : 0; },
            seek (a) { s.transport.position = Math.max(0, Number(a.seconds) || 0); },
            selectInput (a) { s.selectedInput = a.id; },
            setMonitoring (a) { s.monitoring = !! a.on; },
            trackMute (a) { const t = tracks.find(x => x.id === a.trackId); if (t) { t.muted = !! a.muted; emitTracks(); } },
            trackRename (a) { const t = tracks.find(x => x.id === a.trackId); if (t) { t.name = String(a.name); emitTracks(); } },
            trackDelete (a) {
                if (s.transport.recording) return;
                if (window.confirm("Simulácia: zmazať stopu?")) { tracks = tracks.filter(x => x.id !== a.trackId); emitTracks(); setStatus("Stopa zmazaná."); }
            },
            newSong () { tracks = []; s.transport.position = 0; s.song.name = "Nová skladba (simulácia)"; emitTracks(); setStatus("Nová skladba vytvorená."); },
            openSong () { setStatus("Simulácia: tu by sa otvorilo okno na výber skladby."); },
            openSongFolder () { setStatus("Simulácia: tu by sa otvoril priečinok skladby."); },
            exportMix () { setStatus(tracks.length ? "Simulácia: mix exportovaný." : "Nie je čo exportovať – najprv niečo nahraj.", tracks.length ? "ok" : "warn"); },
            openAudioSettings () { setStatus("Simulácia: tu by sa otvorili Nastavenia zvuku."); },
            runDiagnostics () {
                if (! s.busy.diagnosticsAllowed) return setStatus("Diagnostiku spusti, keď je prehrávanie zastavené.", "warn");
                setStatus("Simulácia: diagnostika skopírovaná do schránky.", "ok");
            },
            uiReady () {},
            // --- API v3: mix ---
            trackVolume (a) { const t = tracks.find(x => x.id === a.trackId); if (t) { const db = Number(a.db) || 0; t.volumeDb = db <= -60 ? -100 : Math.min(6, Math.max(-60, Math.round(db * 10) / 10)); emitTracks(); } },
            trackPan (a) { const t = tracks.find(x => x.id === a.trackId); if (t) { t.pan = Math.min(1, Math.max(-1, Math.round((Number(a.pan) || 0) * 100) / 100)); emitTracks(); } },
            trackSolo (a) { const t = tracks.find(x => x.id === a.trackId); if (t) { t.solo = !! a.solo; emitTracks(); } },
            // --- API v4: konzola ---
            selectTrack (a) { if (tracks.some(t => t.id === a.trackId)) s.selectedTrackId = a.trackId; },
            trackAdd () { if (s.transport.recording) return; const t = { id: String(nextId++), name: "Stopa " + (tracks.length + 1), muted: false, solo: false, volumeDb: 0, pan: 0, recording: false, clips: [] }; tracks.push(t); s.selectedTrackId = t.id; emitTracks(); },
            setInputGain (a) { s.inputGainDb = Math.min(24, Math.max(-24, Number(a.db) || 0)); },
            saveSong () { s.status = { text: "Skladba uložená.", level: "ok" }; },
            masterVolume (a) { const db = Number(a.db) || 0; s.mixer.masterDb = db <= -60 ? -100 : Math.min(6, Math.max(-60, Math.round(db * 10) / 10)); },
            // --- API v2 ---
            setTempo (a) {
                if (s.transport.recording) return;
                const bars = [s.loop.start / secPerBar(), s.loop.end / secPerBar()];
                s.tempo.bpm = Math.min(240, Math.max(40, Math.round((Number(a.bpm) || 120) * 10) / 10));
                s.loop.start = bars[0] * secPerBar(); s.loop.end = bars[1] * secPerBar(); updateLoopInfo();
            },
            setTimeSignature (a) {
                if (s.transport.recording) return;
                s.tempo.numerator = Math.min(7, Math.max(2, Math.round(Number(a.numerator) || 4)));
                s.tempo.denominator = Number(a.denominator) === 8 ? 8 : 4; updateLoopInfo();
            },
            setMetronome (a) { s.metronome.on = !! a.on; },
            setMetronomeVolume (a) { s.metronome.volume = Math.min(1, Math.max(0, Number(a.volume) || 0)); },
            setCountIn (a) { if (! s.transport.recording) s.metronome.countInBars = [0, 1, 2].includes(Number(a.bars)) ? Number(a.bars) : (Number(a.bars) > 2 ? 2 : 0); },
            setLoop (a) {
                if (s.transport.recording) return;
                s.loop.on = !! a.on;
                if (s.loop.on && (s.transport.position < s.loop.start || s.transport.position >= s.loop.end)) s.transport.position = s.loop.start;
            },
            toggleLoop () { handlers.setLoop({ on: ! s.loop.on }); },
            setLoopBars (a) {
                const bars = Math.min(32, Math.max(1, Math.round(Number(a.bars) || 4)));
                const start = Math.round(s.loop.start / secPerBar()) * secPerBar();
                setLoopRange(start, start + bars * secPerBar());
            },
            setLoopRange (a) { setLoopRange(Number(a.start) || 0, Number(a.end) || 0); },
            loopIn () {
                if (s.transport.recording) return;
                const p = snapBeat(s.transport.position);
                const end = p < s.loop.end - 0.1 ? s.loop.end : p + Math.max(s.loop.end - s.loop.start, secPerBar());
                setLoopRange(p, end); setStatus("LOOP IN nastavený na " + p.toFixed(2) + " s");
            },
            loopOut () {
                if (s.transport.recording) return;
                const p = snapBeat(s.transport.position);
                if (p <= s.loop.start + 0.1) return setStatus("LOOP OUT musí byť za LOOP IN – posuň prehrávaciu hlavu ďalej.", "warn");
                setLoopRange(s.loop.start, p); setStatus("LOOP OUT nastavený na " + p.toFixed(2) + " s");
            }
        };

        let last = performance.now();

        function tick () {
            const now = performance.now();
            const dt = (now - last) / 1000;
            last = now;

            if (s.transport.playing) {
                s.transport.position += dt;

                if (s.loop.on && s.transport.position >= s.loop.end && s.transport.position - dt >= s.loop.start - 0.001)
                    s.transport.position = s.loop.start + (s.transport.position - s.loop.end);   // slucka sa pretoci

                if (s.loop.recordingPass) {
                    if (s.transport.position < lastPos - 0.25) loopWrapped = true;
                    if (loopWrapped && s.transport.position >= s.loop.start + 0.06) finishLoopPass();
                }
                lastPos = s.transport.position;

                if (! s.loop.on && ! s.transport.recording && tracks.length && s.transport.position > songLength())
                    stopAll();
            }

            s.transport.countingIn = s.transport.recording && s.transport.position < recordStart;

            // meraky stop: pri prehravani vymyslena uroven podla hlasitosti, stlmenia a sola
            const anySolo = tracks.some(t => t.solo);
            s.mixer.trackLevels = tracks.map(t => {
                const audible = s.transport.playing && t.clips.length && ! t.muted && (! anySolo || t.solo) && t.volumeDb > -60;
                const prev = (s.mixer.trackLevels.find(l => l.id === t.id) || {}).levelDb ?? -100;
                const target = audible ? -18 + t.volumeDb + (Math.random() - 0.5) * 12 : -100;
                return { id: t.id, levelDb: Math.round(Math.max(target, prev - 1.5) * 10) / 10 };
            });
            s.mixer.masterLevelDb = s.mixer.trackLevels.reduce((m, l) => Math.max(m, l.levelDb), -100);
            // L/R: panoráma stôp rozdelí úroveň medzi kanály, hlavná hlasitosť sa pripočíta
            let L = -100, R = -100;
            for (const t of tracks) {
                const lv = (s.mixer.trackLevels.find(l => l.id === t.id) || {}).levelDb ?? -100;
                if (lv <= -99) continue;
                const p = t.pan || 0, gl = 20 * Math.log10(Math.max(0.05, Math.cos((p + 1) * Math.PI / 4) * 1.414)), gr = 20 * Math.log10(Math.max(0.05, Math.sin((p + 1) * Math.PI / 4) * 1.414));
                L = Math.max(L, lv + gl); R = Math.max(R, lv + gr);
            }
            const md = s.mixer.masterDb <= -99 ? -100 : s.mixer.masterDb;
            s.mixer.masterLevelL = md <= -99 ? -100 : Math.round((L + md) * 10) / 10;
            s.mixer.masterLevelR = md <= -99 ? -100 : Math.round((R + md) * 10) / 10;
            if (s.selectedTrackId && !tracks.some(t => t.id === s.selectedTrackId)) s.selectedTrackId = tracks[0] ? tracks[0].id : "";
            const beats = s.transport.position / secPerBeat();
            s.musicalPosition = beats < 0 ? { bar: 0, beat: 0 }
                              : { bar: Math.floor(beats / s.tempo.numerator) + 1, beat: Math.floor(beats % s.tempo.numerator) + 1 };

            const hum = Math.random() < 0.08 ? -6 - Math.random() * 10 : -30 - Math.random() * 25;
            s.meter.inputDb = Math.max(hum, s.meter.inputDb - 2.5);

            s.busy.editingAllowed = ! s.transport.recording;
            s.busy.diagnosticsAllowed = ! s.transport.playing && ! s.transport.recording;

            receive("state", JSON.parse(JSON.stringify(s)));
        }

        setInterval(tick, 50);
        setTimeout(emitTracks, 0);
        console.info("[Rafa] SIMULAČNÝ REŽIM – program nie je pripojený.");

        return {
            handle (command, args) {
                const h = handlers[command];
                if (! h) { console.warn("[Rafa] neznámy príkaz:", command); return; }
                h(args);
                tick();
            }
        };
    })();

    window.Rafa = Rafa;
})();
