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
| `ui/index.html`, `ui/styles.css`, `ui/app.js`, `ui/assets/` | **ChatGPT** (dizajn a obrazovka) | môžu len čítať a pripomienkovať |
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
7. **Malé kroky:** jedna zmena = jedna jasná úloha. Žiadne veľké prestavby bez dohody.

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
| 1 | Skúška zvuku – nahrávanie a prehrávanie gitary (dočasná C++ obrazovka) | hotové, čaká na test s gitarou |
| 1b | Most HTML ↔ motor (WebView2), `API.md` v1 | Claude – rozpracované |
| 1c | Dizajn obrazovky v HTML podľa `API.md` v1 | ChatGPT – na rade |
| 2 | Tempo, metronóm, slučka (loop) | neskôr |
