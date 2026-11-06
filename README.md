# RAFA MUSIC STUDIO

Vlastná jednoduchá nahrávacia aplikácia pre Windows: gitara, ďalšie audio stopy a MIDI, metronóm, slučky, efekty, mix a ukladanie projektov. Postavená na open-source nahrávacom motore [Tracktion Engine](https://github.com/Tracktion/tracktion_engine) a knižnici [JUCE](https://juce.com).

## Stiahnutie

Najnovšia verzia pre Windows sa vyrába automaticky po každej zmene:

**[Stiahnuť RAFA-MUSIC-STUDIO-Windows.zip](https://github.com/rafolab/rafa-music-studio/releases/download/najnovsia-verzia/RAFA-MUSIC-STUDIO-Windows.zip)**

1. Stiahni ZIP a rozbaľ ho (pravé tlačidlo → Extrahovať všetko).
2. Spusti `RAFA MUSIC STUDIO.exe`.
3. Windows môže pri prvom spustení zobraziť modré okno „Systém Windows chránil váš počítač“. Klikni na **Ďalšie informácie → Spustiť aj tak**. Program nie je digitálne podpísaný (podpis sa platí), inak je v poriadku.

Skladby sa ukladajú do `Dokumenty\RAFA MUSIC STUDIO\Skladby`.

## Etapa 1 – skúška zvuku (aktuálna verzia)

Cieľ: overiť, že notebook + Blackstar BEAM Mini spoľahlivo nahrávajú gitaru.

Čo program vie:
- výber zvukového ovládača a zariadenia (Windows Audio / ASIO / DirectSound),
- výber vstupu pre gitaru a merač úrovne signálu,
- nahrávanie do novej stopy, prehrávanie, stop, návrat na začiatok,
- každé ďalšie nahrávanie ide do novej stopy, takže počuješ predchádzajúce,
- stlmenie a zmazanie stopy, premenovanie dvojklikom na meno,
- automatické ukladanie skladby, otvorenie staršej skladby,
- export mixu do WAV (do priečinka `Export` v skladbe),
- tlačidlo **Diagnostika** – skopíruje technickú správu do schránky.

Klávesové skratky: **R** = nahrávať, **medzerník** = prehrať / zastaviť, **Home** = na začiatok.

### Testovací postup

1. Pripoj BEAM Mini cez USB a zapni ho. Až potom spusti program.
2. Klikni **Nastavenia zvuku** a skontroluj, že ako vstup je vybraté kombo.
   - Najprv skús ovládač **Windows Audio**.
   - Ak je oneskorenie veľké, skús **Windows Audio (Exclusive Mode)** alebo ASIO (ak je nainštalovaný Blackstar ovládač alebo ASIO4ALL).
3. V poli **Vstup pre gitaru** vyber vstup, pri ktorom sa merač hýbe, keď hráš.
4. Stlač **NAHRÁVAŤ**, zahraj pár taktov, stlač **STOP**.
5. Stlač **Na začiatok** a **PREHRAŤ**. Nahrávka má znieť čisto, bez praskania.
6. Nahraj druhú stopu k prvej a skontroluj, či obe sedia v rytme.
7. Klikni **Diagnostika** a výsledok (Ctrl+V) pošli do chatu.
8. Voliteľné: otvor `.tracktionedit` súbor skladby vo **Waveform Free**, alebo exportovaný WAV v **Sonare**.

## Plán ďalších etáp

| Etapa | Obsah |
| --- | --- |
| 1 | Skúška zvuku – nahrávanie a prehrávanie gitary (táto verzia) |
| 2 | Viac stôp, tempo, metronóm s odpočítaním |
| 3 | MIDI z Akai MPK Mini Play, vstavaný nástroj, kvantizácia |
| 4 | Slučky (loop) a viac pokusov (takes) |
| 5 | Efekty, mixér, VST3 pluginy |
| 6 | Export pre Sonar (WAV stopy + MIDI), inštalátor, finálny vzhľad |

## Pre vývojárov

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Závislosti (JUCE a Tracktion Engine) sa stiahnu automaticky a sú zafixované na konkrétne verzie v `CMakeLists.txt`. Na Windows sa automaticky zapne ASIO (hlavičky ASIO SDK sú súčasťou JUCE).

## Licencia

GPLv3 – pozri [LICENSE.md](LICENSE.md). Tracktion Engine a JUCE sa používajú pod ich open-source licenciami.
