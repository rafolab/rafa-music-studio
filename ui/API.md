# RAFA MUSIC STUDIO – API medzi obrazovkou (HTML) a motorom (C++)

**Verzia API: 1**  ·  vlastník mosta: Claude  ·  zmeny len po dohode (pozri `KOORDINACIA.md`)

Obrazovka používa iba dve funkcie z `ui/rafa-bridge.js`:

```js
Rafa.send("play");                          // príkaz pre motor
Rafa.send("selectInput", { id: "in-1" });   // príkaz s parametrami
Rafa.on("state",  s => { ... });            // pravidelný stav (cca 20× za sekundu)
Rafa.on("tracks", t => { ... });            // zoznam stôp (len keď sa zmení)
Rafa.isSimulation                           // true = otvorené v prehliadači bez programu
```

`rafa-bridge.js` načítaj ako prvý skript: `<script src="rafa-bridge.js"></script>`.

---

## 1. Príkazy (obrazovka → motor)

| Príkaz | Parametre | Čo spraví |
| --- | --- | --- |
| `record` | – | Ak sa nenahráva: vytvorí novú vrstvu (stopu) a nahráva **od začiatku skladby**. Ak sa nahráva: ukončí nahrávanie. |
| `play` | – | Prehrávanie od aktuálnej pozície. |
| `stop` | – | Zastaví prehrávanie aj nahrávanie. |
| `togglePlay` | – | Prehrať / zastaviť (pre medzerník). |
| `rewind` | – | Presun na začiatok (0 s). |
| `seek` | `{ seconds: number }` | Presun prehrávacej hlavy. |
| `selectInput` | `{ id: string }` | Výber vstupu pre gitaru (id zo `state.inputs`). |
| `setMonitoring` | `{ on: boolean }` | Počúvať vstup cez počítač. |
| `trackMute` | `{ trackId: string, muted: boolean }` | Stlmiť / zapnúť stopu. |
| `trackRename` | `{ trackId: string, name: string }` | Premenovať stopu. |
| `trackDelete` | `{ trackId: string }` | Zmazať stopu. Motor sa sám opýta na potvrdenie (natívne okno), UI sa nepýta. |
| `newSong` | – | Nová skladba. |
| `openSong` | – | Otvorí natívne okno na výber skladby. |
| `openSongFolder` | – | Otvorí priečinok skladby v Prieskumníkovi. |
| `exportMix` | – | Export mixu do WAV. |
| `openAudioSettings` | – | Otvorí natívne okno nastavení zvuku. |
| `runDiagnostics` | – | Diagnostika (skopíruje správu do schránky). |

Klávesové skratky rieši UI a posiela príkazy: **R** → `record`, **medzerník** → `togglePlay`, **Home** → `rewind`.

---

## 2. Udalosť `state` (motor → obrazovka, cca 20× za sekundu)

```json
{
  "apiVersion": 1,
  "transport": { "playing": false, "recording": false, "position": 12.4 },
  "song":      { "name": "Skladba 2026-11-07 00-40", "length": 31.5 },
  "meter":     { "inputDb": -18.2 },
  "inputs":    [ { "id": "in-1", "name": "Input 1 (BEAM Mini)" },
                 { "id": "in-2", "name": "Input 2 (BEAM Mini)" } ],
  "selectedInput": "in-1",
  "monitoring": false,
  "device": {
    "ok": true,
    "driver": "Windows Audio",
    "input": "BEAM Mini",
    "output": "Reproduktory",
    "sampleRate": 48000,
    "buffer": 480,
    "latencyMs": 32.5,
    "cpu": 4
  },
  "status": { "text": "Pripravené.", "level": "info" },
  "busy": { "diagnosticsAllowed": true, "editingAllowed": true }
}
```

Poznámky:
- `meter.inputDb` je v dB: −100 = ticho, 0 = maximum. Odporúčané pásma: zelená do −12, žltá do −3, červená nad −3.
- `status.level` je jedno z: `info`, `ok`, `warn`, `error`, `recording`.
- `device.ok = false` → zobraziť výzvu „Otvor Nastavenia zvuku“.
- `busy.editingAllowed = false` počas nahrávania → zablokovať tlačidlá Nová skladba, Otvoriť, Export, Nastavenia zvuku, výber vstupu, mazanie stôp.
- `busy.diagnosticsAllowed = false` počas prehrávania a nahrávania.

---

## 3. Udalosť `tracks` (motor → obrazovka, len pri zmene)

```json
{
  "tracks": [
    {
      "id": "1001",
      "name": "Gitara 1",
      "muted": false,
      "recording": false,
      "clips": [
        { "start": 0.0, "length": 31.5, "peaks": [0.02, 0.4, 0.71, ...] }
      ]
    }
  ],
  "viewLength": 36.5
}
```

- `peaks` sú hodnoty 0–1 rovnomerne rozložené po dĺžke klipu (cca 200 bodov), na kreslenie vlnového priebehu.
- `viewLength` je odporúčaná dĺžka viditeľnej časovej osi v sekundách (časová os v UI = 0 až `viewLength`).
- Prehrávaciu hlavu kresli podľa `state.transport.position`.
- Stopa s `recording: true` je tá, do ktorej sa práve nahráva (zvýrazniť červenou).

---

## 4. Rozloženie a technické požiadavky na UI

- Okno programu: minimálne **980 × 640 px**, dá sa zväčšiť. UI musí byť pružné (flex/grid), bez pevnej šírky.
- Žiadne externé zdroje (CDN, webové písma). Všetko v priečinku `ui/`.
- Súbory UI: `ui/index.html`, `ui/styles.css`, `ui/app.js`, voliteľne `ui/assets/`.
- Bez frameworkov s build krokom (React/Vite a pod.). Čisté HTML + CSS + JS.
- UI nič neukladá a nepočíta zvuk. Iba zobrazuje `state`/`tracks` a posiela príkazy.

## 5. História verzií API

| Verzia | Zmena |
| --- | --- |
| 1 | Prvá verzia: transport, vstup, merač, stopy, skladby, export, diagnostika |
