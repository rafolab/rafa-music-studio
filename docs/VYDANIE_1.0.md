# RAFA MUSIC STUDIO 1.0 – stabilná verzia

Jednoduchá nahrávacia „loop station“ pre Windows: gitara, slučky, vrstvy, mix a export do Cakewalk/Sonar.

## Stiahnutie a spustenie
1. Stiahni `RAFA-MUSIC-STUDIO-Windows.zip` (nižšie v časti Assets).
2. ZIP ulož na disk, klikni naň pravým tlačidlom → **Extrahovať všetko…**
3. Spusti `RAFA MUSIC STUDIO.exe`. Ak Windows ukáže modré okno: **Ďalšie informácie → Spustiť aj tak**.
4. Návod je v súbore `NAVOD.txt`.

## Čo vie
- **Slučky** dlhé 1, 2, 4, 8 a 16 taktov, s odpočítaním aj bez neho. Prvé opakovanie hrá bez medzery a nechýba ani jedna doba.
- **Vrstvy** sa nahrávajú bez zastavenia prehrávania a presne sedia na prvej.
- **Ovládanie a mix**: UNDO/REDO, mute, solo, fadery, PAN, odstránenie stopy s možnosťou vrátenia.
- **Metronóm** s hlasitosťou od úplného ticha. Do nahrávky sa nenahráva.
- **Skladby**: uloženie a otvorenie. Projekty zo starších verzií sa otvoria tiež.
- **Export** jednotlivých stôp a mixu do WAV (24-bit) pre Cakewalk/Sonar.
- **Diagnostika** a záznam udalostí (`udalosti.txt`) na rýchle hľadanie chýb.

## Overené
- Na Windows 11 s Blackstar BEAM Mini (ASIO, 48 kHz, buffer 512):
  - slučka 1 takt a 4 takty;
  - opakované nahrávanie do tej istej stopy;
  - vrstvy bez zastavenia;
  - UNDO/REDO, uloženie, otvorenie, export stôp a mixu;
  - 0 výpadkov zvuku.
- Automatické testy na skutočnom zvukovom motore:
  - všetky dĺžky slučiek a odpočítania;
  - presnosť vrstiev (do 0,2 ms);
  - plynulosť prechodu;
  - STOP počas odpočítania;
  - hlasitosť metronómu.

## Známe obmedzenia
- Iba Windows (64-bit).
- Efekty (tlačidlo EFFECTS) prídu vo verzii 1.1.
- Presné zarovnanie nahrávky s metronómom závisí od ovládača. Doladíš ho v **SETTINGS → Posun nahrávky**.
