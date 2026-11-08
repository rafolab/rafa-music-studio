# RAFA MUSIC STUDIO – grafické doladenie Model 5 (V2, na kontrolnú integráciu)

Podklady: pôvodný Claude balík zo stavu `4615766`. **Neprepísať motor ani súradnice.**

## Zmenené súbory
- `podklad/panel.png` (presne 1672 × 941): veľmi jemné vyrovnanie širokého tmavého pásu pod PAN gombíkmi, lokálna redukcia zvyškov oranžových odleskov pri vypnutých tlačidlách 1/2 a pri M2. Nápisy, stupnice, faderové dráhy, gombíky aj pozície nezmenené.
- `prvky-png/btn_layer1_off.png`, `btn_layer2_off.png`: odstránenie bočných oranžových zvyškov v pokojovom stave.
- `prvky-png/btn_layer1_on.png`, `btn_layer2_on.png`, `btn_play_on.png`, `btn_metroOn_on.png`: rovnomernejšie jantárové osvetlenie z V1.
- `prvky-png/btn_metroOn_off.png`: lokálna úprava tieňa na tvári tlačidla.
- `prvky-png/btn_loop_off.png`, `btn_loop_on.png`: jemné zjednotenie farebnosti kovu s IN/OUT, zachovaná identická pôvodná perspektíva a symbol.
- `kod/console.css`: **iba** miernejšia vonkajšia žiara tlačidiel (štyri box-shadow hodnoty). Žiadne HTML, JS ani audio zmeny.

Rozmery jednotlivých sprite obrázkov, názvy, RGBA alfa masky a pozície v `controls.json` zostali nezmenené. Nevykonaný žiadny posun hitboxov. Zmenené PNG treba konvertovať do WebP s priehľadnosťou pod rovnakými názvami, ako používa program.

## Náhľady
- `RAFA_Model5_GRAFIKA_V2_VYPNUTE.jpg`: simulované vybrané vypnuté tlačidlá.
- `RAFA_Model5_GRAFIKA_V2_ZAPNUTE.jpg`: simulované vybrané zapnuté tlačidlá.
Tieto náhľady sú iba registrácia spritov na podklad, nie screenshot aplikácie s CSS, VU a displejom.

## Poznámka k vizuálnej vernosti
Spoj nad fadermi je potlačený iba opatrne, pretože cez oblasť prechádzajú označenia stupníc aj skutočné tiene gombíkov. Ide o konzervatívnu retuš; potrebné ešte overiť v bežiacom WebView2, či vizuálne vyhovuje. Ak nie, bude potrebná samostatná presná rekonštrukcia tejto časti, nie prelepovanie obdĺžnikom.

## Integračný postup pre Claudea
1. Vytvor zálohu pôvodného podkladu, spritov a CSS.
2. Prekopíruj uvedené zmenené PNG, konvertuj na WebP so zachovanou alfa maskou (program aktuálne používa WebP); `panel.webp` prekonvertuj z opraveného `panel.png`.
3. Zmeň 4 CSS shadow hodnoty podľa `kod/console.css` alebo nahraď tento jediný CSS súbor, ak je jeho verzia stále identická s balíkom stavu `4615766`.
4. Over na Windows: pokojové tlačidlá, PLAY, LOOP, MUTE/SOLO, ON a metronóm; faderové značky a správne polohy ostali viditeľné.
5. Ak sa prejaví akýkoľvek neželaný posun či artefakt, vráť tú konkrétnu úpravu; pôvodné zdrojové obrázky zostaňú nedotknuté.
