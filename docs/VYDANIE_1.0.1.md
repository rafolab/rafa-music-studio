## RAFA MUSIC STUDIO 1.0.1 – opravné vydanie

Stiahni `RAFA-MUSIC-STUDIO-1.0.1-Windows.zip`, rozbaľ a spusti `RAFA MUSIC STUDIO.exe`. Návod je v súbore `NAVOD.txt` v balíku.

**Opravy oproti 1.0.0**
- REC stlačený hneď po STOP: kým sa predošlá nahrávka nedokončí, program nové nahrávanie nezačne a napíše „Dokončujem predchádzajúcu nahrávku – stlač REC o chvíľu znova.“ Predošlá nahrávka sa už nemôže stratiť z histórie ani z UNDO.
- STOP tesne po odpočítaní: program už neodhaduje podľa času. Nahrávku zahodí iba vtedy, keď je celá v odpočítaní. Ak sa nahral čo i len kúsok, uloží sa normálne a dá sa vrátiť cez UNDO.
- Úprava hlasitosti metronómu sa pri zostavovaní overuje – nemôže sa potichu stratiť.
- Stabilné vydanie sa už nikdy neprepíše; každá oprava dostane nové číslo.

Verzia 1.0.0 zostáva dostupná medzi vydaniami.
