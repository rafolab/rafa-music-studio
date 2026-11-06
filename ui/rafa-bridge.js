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

    const API_VERSION = 1;
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
            transport: { playing: false, recording: false, position: 0 },
            song: { name: "Skladba (simulácia)", length: 0 },
            meter: { inputDb: -100 },
            inputs: [ { id: "in-1", name: "Input 1 (BEAM Mini – simulácia)" },
                      { id: "in-2", name: "Input 2 (BEAM Mini – simulácia)" } ],
            selectedInput: "in-1",
            monitoring: false,
            device: { ok: true, driver: "Windows Audio (simulácia)", input: "BEAM Mini", output: "Reproduktory",
                      sampleRate: 48000, buffer: 480, latencyMs: 32.5, cpu: 3 },
            status: { text: "Simulácia – program nie je pripojený, dáta sú vymyslené.", level: "info" },
            busy: { diagnosticsAllowed: true, editingAllowed: true }
        };
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

        function stopAll () {
            const wasRecording = s.transport.recording;
            s.transport.playing = false;
            s.transport.recording = false;

            if (wasRecording && recordingTrack) {
                const len = Math.max(0.5, s.transport.position);
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
                if (s.transport.recording) return stopAll();
                recordingTrack = { id: String(nextId++), name: "Gitara " + (tracks.length + 1), muted: false, recording: true, clips: [] };
                tracks.push(recordingTrack);
                s.transport.position = 0;
                s.transport.playing = true;
                s.transport.recording = true;
                setStatus("● Nahrávam od začiatku do stopy „" + recordingTrack.name + "“ – STOP ukončí nahrávanie.", "recording");
                emitTracks();
            },
            play () { if (! s.transport.recording) { s.transport.playing = true; setStatus("Prehrávam…", "ok"); } },
            stop () { stopAll(); },
            togglePlay () { (s.transport.playing || s.transport.recording) ? stopAll() : handlers.play(); },
            rewind () { s.transport.position = 0; },
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
            uiReady () {}
        };

        let last = performance.now();

        function tick () {
            const now = performance.now();
            const dt = (now - last) / 1000;
            last = now;

            if (s.transport.playing) {
                s.transport.position += dt;
                if (! s.transport.recording && tracks.length && s.transport.position > songLength())
                    stopAll();
            }

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
