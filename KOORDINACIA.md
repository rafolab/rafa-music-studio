# Koordinácia projektu RAFA MUSIC STUDIO

Jeden produkt, jeden repozitár, dvaja AI pomocníci. Tento súbor je záväzný pre oboch.

## Cieľ produktu

Jednoduchý virtuálny **LOOP DECK** pre gitaristu na Windows: nahrávanie vrstiev gitary (a neskôr MIDI) cez Blackstar BEAM Mini, prehrávanie, slučky, základné efekty a mix. Nie komplikovaná DAW. Pokročilé úpravy sa robia v Cakewalk Sonar cez export.

## Architektúra (rozhodnuté)

```
┌─────────────────────────────────────────────┐
│  ui/  – obrazovka v HTML/CSS/JS   (ChatGPT) │
│        komunikuje LEN cez ui/rafa-bridge.js │
├─────────────────────────────────────────────┤
│  most: príkazy ↓  /  stav ↑  (ui/API.md)    │
├─────────────────────────────────────────────┤
│  Source/ – C++: Tracktion Engine + JUCE     │
│            zvuk, nahrávanie, súbory (Claude)│
└─────────────────────────────────────────────┘
```

Program je natívna Windows aplikácia. Obrazovka je HTML zobrazené vo vstavanom okne WebView2 (súčasť Windows 11). Všetok zvuk spracúva C++ motor, nie prehliadač.

## Kto čo vlastní

| Časť | Vlastník | Ostatní |
| --- | --- | --- |
| `ui/index.html`, `ui/console.css`, `ui/console.js`, `ui/controls.js`, `ui/assets/`, `ui/fonts/` | **Claude** (konzola Model 5, od kroku 2A) | ChatGPT kontroluje vzhľad, ergonómiu a slovenské texty menu |
| `Source/`, `CMakeLists.txt`, `.github/` | **Claude** (zvuk, motor, zostavenie) | môžu len čítať a pripomienkovať |
| `ui/rafa-bridge.js` | **Claude** | ChatGPT ho používa, nemení |
| `ui/API.md` (zmluva medzi UI a motorom) | **spoločná** | mení sa len po dohode cez Rafa |
| `KOORDINACIA.md`, `README.md` | **Claude** | zmeny navrhuje ChatGPT cez Rafa |

## Pravidlá

1. **Jediná verzia** projektu je vetva `main` v `rafolab/rafa-music-studio`. Nič sa nevyvíja mimo nej.
2. **UI hovorí s motorom iba cez `Rafa.send()` a `Rafa.on()`** z `ui/rafa-bridge.js`. Žiadne iné volania, žiadne priame `window.__JUCE__`.
3. **Nový príkaz alebo nové dáta?** ChatGPT napíše návrh zmeny `API.md` → Rafa ho prepošle Claudovi → Claude ho doplní do motora aj do `API.md`, zvýši verziu API → až potom ho UI použije.
4. **Bez internetu:** UI nesmie načítavať nič z webu (CDN, Google Fonts, externé knižnice). Všetko musí byť v `ui/`. Písma: systémové (`Segoe UI`, `Arial`) alebo súbor v `ui/assets/`.
5. **Texty v UI po slovensky.**
6. **Testovanie dizajnu:** `ui/index.html` sa dá otvoriť priamo v prehliadači. `rafa-bridge.js` vtedy beží v **simulačnom režime** (falošný merač, nahrávanie, stopy), takže dizajn sa dá ladiť bez programu.
7. **Súbory v `ui/` musia mať jedinečné mená** (aj v podpriečinkoch), lebo sa balia do programu podľa mena súboru. Napr. `ui/assets/logo.svg` sa načíta ako `assets/logo.svg` aj `logo.svg`.
8. **Kým v `ui/` nie je `index.html`**, program použije dočasnú klasickú obrazovku v C++. Tú si možno vynútiť aj podržaním **Shift** pri spustení programu (záchranný režim).
9. **Malé kroky:** jedna zmena = jedna jasná úloha. Žiadne veľké prestavby bez dohody.

## Zistenia z testu (7. 11. 2026)

- Blackstar BEAM Mini posiela cez USB **4 vstupné kanály**; signál gitary je na kanáloch 1, 2 a 3, kanál 4 je ticho. Predvolený je kanál 1.
- K dispozícii je ovládač **ASIO „Blackstar USB Audio“** (nižšia latencia). Windows Audio: 48 kHz, buffer 480, cca 30 ms.
- Efekty komba sa nastavujú cez aplikáciu v telefóne (Bluetooth).

## Ako sa súbory dostanú do repozitára

- **Claude** nahráva priamo (má prístup).
- **ChatGPT** prístup nemá. Rafa nahrá jeho súbory jedným z dvoch spôsobov:
  - na GitHube: priečinok `ui` → **Add file → Upload files** → potiahnuť súbory → **Commit changes**,
  - alebo ich pošle Claudovi do chatu a Claude ich nahrá.
- Po každom nahraní sa program automaticky zostaví (cca 6 min) a je na rovnakej adrese na stiahnutie.

## Postup práce (minimum krokov pre Rafa)

1. ChatGPT pošle hotové súbory `ui/…` → Rafa ich nahrá alebo pošle Claudovi.
2. Claude skontroluje, či UI dodržiava `API.md`, a zostaví program.
3. Rafa stiahne program a vyskúša ho s gitarou.
4. Pripomienky Rafa napíše tomu, koho sa týkajú: vzhľad → ChatGPT, zvuk/funkcia → Claude.

## Stav

| Etapa | Obsah | Stav |
| --- | --- | --- |
| 1 | Skúška zvuku – nahrávanie a prehrávanie gitary | **otestované 7. 11. 2026 s BEAM Mini – funguje** |
| 1b | Most HTML ↔ motor (WebView2), `API.md` v1 | hotové |
| 1c | Obrazovka LOOP DECK Model 5 v HTML (ChatGPT) | **otestované na Windows – funguje** |
| 2 | Tempo, metronóm, slučka (LOOP): `API.md` v2 + motor + simulácia (Claude) | hotové, overené v simulovanom zvukovom prostredí |
| 3 | Mix vrstiev: `API.md` v3 + motor + simulácia (Claude) | hotové, overené so skutočným motorom (JACK dummy) |
| 3c | ~~Oživenie fádrov… (ChatGPT)~~ – nahradené novou konzolou Model 5 | zrušené |
| 2A–2C | Konzola Model 5: čistý panel, 44 samostatných prvkov (`ui/assets`, `ui/controls.js`, `ui/console.js`), API v4, motor. Robí Claude, ChatGPT kontroluje vzhľad a ergonómiu. Nástroje na výrobu grafiky: `tools/model5/` | hotové, čaká na test na Windows |
| – | MIDI (klávesy Akai) | odložené – Windows klávesy zatiaľ nevidí (problém s USB pripojením) |
| 2c | Oživenie ovládačov LOOP, METRONÓM, TEMPO v HTML podľa `API.md` v2 | hotové – **jednorazová výnimka: zapracoval Claude** (ZIP od ChatGPT sa nedal stiahnuť); kontrolu UI robí ChatGPT |
