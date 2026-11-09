# RAFA MUSIC STUDIO – API medzi obrazovkou (HTML) a motorom (C++)

**Verzia API: 5**  ·  vlastník mosta: Claude  ·  zmeny len po dohode (pozri `KOORDINACIA.md`)

Obrazovka používa iba dve funkcie z `ui/rafa-bridge.js`:

```js
Rafa.send("play");                          // príkaz pre motor
Rafa.send("selectInput", { id: "in-1" });   // príkaz s parametrami
Rafa.on("state",  s => { ... });            // pravidelný stav (cca 20× za sekundu)
Rafa.on("tracks", t => { ... });            // zoznam stôp (len keď sa zmení)
Rafa.isSimulation                           // true = otvorené v prehliadači bez programu
Rafa.apiVersion                             // 5
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

Klávesové skratky rieši UI a posiela príkazy: **R** → `record`, **medzerník** → `togglePlay`, **Home** → `rewind`, **L** → `toggleLoop` (nové v API 2).

### Príkazy pridané v API 2 – tempo, metronóm, LOOP

| Príkaz | Parametre | Čo spraví |
| --- | --- | --- |
| `setTempo` | `{ bpm: number }` | Tempo skladby 40–240 BPM (zaokrúhli sa na 0,1). Slučka si zachová dĺžku v taktoch. Nahraté vrstvy sa **nepreťahujú** – po zmene tempa nesedia s metronómom. Počas nahrávania sa ignoruje. |
| `setTimeSignature` | `{ numerator: 2–7, denominator: 4 \| 8 }` | Metrum (predvolené 4/4). Počas nahrávania sa ignoruje. |
| `setMetronome` | `{ on: boolean }` | Zapne/vypne metronóm (počuť ho pri prehrávaní aj nahrávaní, prvá doba taktu je zdôraznená). |
| `setMetronomeVolume` | `{ volume: 0–1 }` | Hlasitosť metronómu (0 = najtichšie, nie úplne ticho). |
| `setCountIn` | `{ bars: 0 \| 1 \| 2 }` | Odpočítanie pred nahrávaním v taktoch (predvolené 1). Pri odpočítaní znie metronóm aj keď je vypnutý. |
| `setLoop` | `{ on: boolean }` | Zapne/vypne slučku. Pri zapnutí sa prehrávacia hlava presunie na LOOP IN, ak je mimo slučky. |
| `toggleLoop` | – | Prepne slučku (tlačidlo so šípkami / kláves L). |
| `setLoopBars` | `{ bars: 1–32 }` | Dĺžka slučky v taktoch od LOOP IN (LOOP IN sa zarovná na takt). Najrýchlejší spôsob pre loop deck: 1, 2, 4, 8 taktov. |
| `setLoopRange` | `{ start: s, end: s }` | Presný rozsah slučky v sekundách. |
| `loopIn` | – | LOOP IN = aktuálna pozícia prehrávacej hlavy, zarovnaná na najbližšiu dobu. Ak by bol za LOOP OUT, slučka si zachová dĺžku. |
| `loopOut` | – | LOOP OUT = aktuálna pozícia, zarovnaná na najbližšiu dobu. Musí byť za LOOP IN, inak sa ignoruje a v `status` príde upozornenie. |

### Príkazy pridané v API 5 – etapa 2D (UNDO, skladby, export stôp)

| Príkaz | Parametre | Čo spraví |
| --- | --- | --- |
| `undo` | – | Vráti posledný pokus (nahrávku) alebo odstránenie stopy. Počas nahrávania nič nespraví. Najviac 20 krokov. |
| `redo` | – | Obnoví posledný vrátený krok. Nový pokus vetvu REDO ukončí. |
| `listSongs` | – | Motor pošle v `state.songList` zoznam skladieb (`Skladby` aj `Rozpracované`). |
| `openSongPath` | `{ path }` | Otvorí skladbu zo zoznamu. |
| `saveSongAs` | `{ name }` | Nepomenovanú skladbu presunie do `Skladby/<name>`, pomenovanú skopíruje. Ak názov existuje, nič nespraví a ohlási to. |
| `discardSong` | `{ thenOpen? }` | Nepomenovanú skladbu presunie do koša Windows, potom otvorí `thenOpen` alebo novú skladbu. |
| `deleteSong` | – | Aktuálnu skladbu presunie do koša Windows a otvorí novú. Obrazovka sa vopred opýta. |
| `moveSongsToDrafts` | `{ paths: [] }` | Vybrané pomenované skladby presunie do `Rozpracované`; najprv ich skopíruje do `Záloha/<dátum>`. |
| `exportTracks` | – | Každá stopa ako samostatný surový WAV (bez fadera, PAN a M/S), všetky od 0 s a rovnako dlhé – pre Cakewalk Sonar. |
| `openExportFolder` | – | Otvorí priečinok s exportmi. |

Zvukové súbory sa pri UNDO ani pri odstránení stopy nemažú. Stopa obnovená cez UNDO dostane nové `id` (meno, nastavenia a poradie ostanú).

Polia pridané v API 5: `song.isDraft`, `song.hasRecordings`, `song.folder`, `history { canUndo, canRedo, undoLabel, redoLabel }`, `songList` (po `listSongs`).

### Príkazy pridané v API 4 – konzola Model 5

| Príkaz | Parametre | Čo spraví |
| --- | --- | --- |
| `selectTrack` | `{ trackId: string }` | Vyberie aktívnu vrstvu (tlačidlá 1–4, DATA). Ukladá sa do skladby. REC nahráva do vybranej vrstvy, ak je prázdna. |
| `trackAdd` | – | Pridá prázdnu vrstvu „Stopa N“ a vyberie ju. Počas nahrávania nič nespraví. |
| `setInputGain` | `{ db: −24…+24 }` | Citlivosť vstupu gitary (gombík INPUT). Platí pre nahrávanie aj merač vstupu. Pamätá sa v nastaveniach programu. |
| `saveSong` | – | Okamžite uloží skladbu (inak sa ukladá automaticky). |

MONITOR na konzole používa existujúci príkaz `setMonitoring` (zapnutý / vypnutý). Plynulú hlasitosť odposluchu motor Tracktion nemá; hlasitosť gitary sa reguluje na kombe.

Zmeny mixu, tempa a metronómu sa ukladajú s oneskorením 0,8 s (ťahanie faderom nepíše súbor 20× za sekundu). Pri zatvorení skladby sa uloží vždy.

### Príkazy pridané v API 3 – mix vrstiev

| Príkaz | Parametre | Čo spraví |
| --- | --- | --- |
| `trackVolume` | `{ trackId: string, db: number }` | Hlasitosť stopy v dB, rozsah −60 až +6. Hodnota ≤ −60 = ticho (stav vráti −100). 0 dB = bez zmeny. |
| `trackPan` | `{ trackId: string, pan: −1…1 }` | Panoráma stopy: −1 vľavo, 0 stred, 1 vpravo. |
| `trackSolo` | `{ trackId: string, solo: boolean }` | Sólo: ak má aspoň jedna stopa sólo, počuť len stopy so sólom. Môže ich byť viac. |
| `masterVolume` | `{ db: number }` | Hlavná hlasitosť (−60 až +6 dB, ≤ −60 = ticho). |

Mix sa dá meniť kedykoľvek, aj počas prehrávania a nahrávania. Ukladá sa do skladby. `trackMute` (z API 1) ostáva bez zmeny – stlmenie a sólo sú nezávislé.

Všetky nastavenia tempa, metronómu a slučky sa ukladajú do skladby. Nová skladba: 120 BPM, 4/4, metronóm vypnutý, odpočítanie 1 takt, slučka 4 takty od 0 s (vypnutá).

### Správanie transportu (API 2)

| Situácia | `record` | `play` / `togglePlay` | `stop` | `rewind` |
| --- | --- | --- | --- | --- |
| **LOOP vypnutý** | Nová vrstva od 0 s (po odpočítaní); ďalší `record` alebo `stop` ukončí nahrávanie a zastaví. | Hrá od aktuálnej pozície. | Zastaví. | Na 0 s. |
| **LOOP zapnutý** | Odpočítanie → nahrá sa **presne jeden prechod** slučky (LOOP IN → LOOP OUT) do novej vrstvy → nahrávanie skončí **samo** a slučka **hrá ďalej** s novou vrstvou. Ďalší `record` = ďalšia vrstva. `record` počas nahrávania ukončí vrstvu skôr (slučka hrá ďalej). Slučka musí mať aspoň 2 s (`loop.canRecord`). | Hrá slučku dokola; ak je hlava mimo slučky, začne od LOOP IN. | Zastaví všetko (aj nahrávanie). | Na LOOP IN. |

Počas nahrávania sa ignorujú: `seek`, `rewind`, zmena tempa, metra, odpočítania a slučky.

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
  "busy": { "diagnosticsAllowed": true, "editingAllowed": true },

  "tempo":     { "bpm": 120, "numerator": 4, "denominator": 4 },
  "metronome": { "on": false, "volume": 0.5, "countInBars": 1 },
  "loop":      { "on": true, "start": 0.0, "end": 8.0, "lengthBars": 4,
                 "canRecord": true, "recordingPass": false },
  "musicalPosition": { "bar": 3, "beat": 2 },

  "mixer": {
    "masterDb": 0.0,
    "masterLevelDb": -14.2,
    "trackLevels": [ { "id": "1001", "levelDb": -16.8 }, { "id": "1002", "levelDb": -100 } ],
    "masterLevelL": -15.1,
    "masterLevelR": -17.9
  },

  "selectedTrackId": "1001",
  "inputGainDb": 0.0
}
```

Polia pridané v API 2:
- `apiVersion` je `2`.
- `transport.countingIn` – `true` počas odpočítania pred nahrávaním (`position` je vtedy pred začiatkom nahrávania, môže byť záporná). Vhodné na zobrazenie „ODPOČÍTAVANIE“ a blikanie LED metronómu.
- `tempo` – tempo a metrum.
- `metronome` – metronóm, hlasitosť 0–1, odpočítanie v taktoch.
- `loop` – slučka: `start`/`end` v sekundách, `lengthBars` (môže byť desatinné), `canRecord` (dĺžka ≥ 2 s), `recordingPass` (`true` = práve sa nahráva vrstva slučky).
- `musicalPosition` – takt a doba prehrávacej hlavy (od 1); počas odpočítania `0`/`0`.

Polia pridané v API 3:
- `mixer.masterDb` – hlavná hlasitosť v dB (−100 = ticho).
- `mixer.trackLevels` – aktuálna úroveň signálu každej stopy pri prehrávaní (dB, −100 = ticho), na meradlá kanálov a VU metre. Úroveň je za hlasitosťou a panorámou stopy.
- `mixer.masterLevelDb` – najvyššia z úrovní stôp (približná úroveň výstupu).

Polia pridané v API 4:
- `mixer.masterLevelL`, `mixer.masterLevelR` – skutočná úroveň výstupu zvukovej karty v dB, zvlášť ľavý a pravý kanál (VU metre). Obsahuje aj metronóm a odposluch.
- `selectedTrackId` – vybraná vrstva (ak vybraná zanikla, prvá vrstva; bez vrstiev prázdny text).
- `inputGainDb` – citlivosť vstupu (gombík INPUT).

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
      "solo": false,
      "volumeDb": 0.0,
      "pan": 0.0,
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
- API 3: `solo`, `volumeDb` (−100 = ticho) a `pan` (−1…1) každej stopy. Udalosť `tracks` príde aj pri zmene týchto hodnôt.
- Panel Model 5 má 4 kanály: kanál 1–4 = prvé 4 stopy v poradí `tracks` (rovnako ako tlačidlá M).

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
| 5 | Etapa 2D: `undo`, `redo`, `listSongs`, `openSongPath`, `saveSongAs`, `discardSong`, `deleteSong`, `moveSongsToDrafts`, `exportTracks`, `openExportFolder` |
| 4 | Konzola Model 5: `selectTrack`, `trackAdd`, `setInputGain`, `saveSong`; v `state` pribudli `selectedTrackId`, `inputGainDb`, `mixer.masterLevelL/R` |
| 3 | Mix: `trackVolume`, `trackPan`, `trackSolo`, `masterVolume`; v `tracks` pribudli `solo`, `volumeDb`, `pan`; v `state` pribudol `mixer` (úrovne stôp) |
| 2 | Tempo, metrum, metronóm, odpočítanie, LOOP (IN/OUT/dĺžka v taktoch, nahrávanie vrstiev v slučke), `transport.countingIn`, `musicalPosition`, kláves L |

## API v5.1 (etapa 2E.6) – zvuk a diagnostika

| Príkaz | Argumenty | Popis |
|---|---|---|
| `setBufferSize` | `{ samples }` | veľkosť bufferu zvukového zariadenia (z `state.device.bufferSizes`); nie počas nahrávania |
| `setRecordOffset` | `{ ms }` | ručný posun nahrávok −200…+200 ms (kladný = nahrávka sa posunie skôr), uloží sa |
| `resetDropouts` | – | vynuluje počítadlo výpadkov zvuku |

Nové polia v `state.device`: `bufferSizes[]`, `cpuPeak` (%), `dropouts` (počet podozrení na výpadok od spustenia),
`recordOffsetMs`, `autoCompMs` (automatická kompenzácia motora).

LOOP: po nahratí prechodu slučky motor spojí rozdelené „takes" Tracktionu do jedného súboru `…_slucka.wav`
s presnou dĺžkou slučky (oprava chýbajúcej doby / ticha pri odpočítaní).
