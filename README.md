# RAFA MUSIC STUDIO

Jednoduchá nahrávacia „loop station“ pre Windows: gitara, slučky, vrstvy, metronóm, mix a export do Cakewalk/Sonar. Program je postavený na open-source nahrávacom motore [Tracktion Engine](https://github.com/Tracktion/tracktion_engine) a knižnici [JUCE](https://juce.com).

## Stiahnutie – verzia 1.1.0 (stabilná)

**[Stiahnuť RAFA MUSIC STUDIO 1.1.0 pre Windows](https://github.com/rafolab/rafa-music-studio/releases/tag/v1.1.0)**

1. Stiahni `RAFA-MUSIC-STUDIO-1.1.0-Windows.zip`.
2. ZIP ulož na disk, klikni naň pravým tlačidlom a vyber **Extrahovať všetko…**
3. Spusti `RAFA MUSIC STUDIO.exe`. Ak Windows zobrazí modré okno „Systém Windows chránil váš počítač“, klikni na **Ďalšie informácie → Spustiť aj tak**. Program nie je digitálne podpísaný (podpis je platený), inak je v poriadku.
4. Návod je v súbore `NAVOD.txt`.

Skladby sa ukladajú do priečinka `Dokumenty\RAFA MUSIC STUDIO\Skladby`.

## Čo vie
- **Slučky** dlhé 1, 2, 4, 8 a 16 taktov, s odpočítaním aj bez neho. Prvé opakovanie hrá bez medzery a nechýba ani jedna doba.
- **Vrstvy** sa nahrávajú počas hrania, bez zastavenia, a presne sedia na prvej.
- **Ovládanie a mix**: UNDO/REDO, mute, solo, fadery, PAN, odstránenie stopy s možnosťou vrátenia.
- **Metronóm** s hlasitosťou od úplného ticha. Do nahrávky sa nenahráva.
- **Merače VU** ukazujú signál gitary.
- **Skladby**: uloženie, otvorenie, export stôp a mixu do WAV (24-bit) pre Cakewalk/Sonar.
- **Diagnostika** a záznam udalostí na rýchle hľadanie chýb.

## Čo potrebuješ
- Windows 10 alebo 11 (64-bit).
- Zvukovú kartu alebo USB kombo, najlepšie s ovládačom ASIO. Overené s Blackstar BEAM Mini.

Efekty (tlačidlo EFFECTS) prídu vo verzii 1.1.

## Pre vývojárov

```
cmake -S . -B build -A x64
cmake --build build --config Release
```

Závislosti (JUCE a Tracktion Engine) sa stiahnu automaticky a sú zafixované na konkrétne verzie v `CMakeLists.txt`. Na Windows sa automaticky zapne ASIO (hlavičky ASIO SDK sú súčasťou JUCE).

## Licencia

GPLv3, pozri [LICENSE.md](LICENSE.md). Tracktion Engine a JUCE sa používajú pod ich open-source licenciami.
