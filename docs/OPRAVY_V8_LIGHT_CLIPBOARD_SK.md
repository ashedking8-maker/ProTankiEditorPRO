# V8 – light selection / copy-paste

- Native `<light>` je teraz plnohodnotný `GameplaySelection::Light` aj pre Ctrl+C / Ctrl+V.
- Kliknutie, box-selection, Ctrl+A (ak sú light markers viditeľné), Delete/Undo a Properties používajú rovnaký selection model.
- Nový light po vložení sa ihneď vloží aj do `functionalSelection_`, takže sa správa rovnako ako ostatné gameplay elementy.
- Ctrl+C zachová všetky známe parametre lightu; detached `originalXml` navyše prenesie neznáme atribúty/child nodes pri kópii podporovaného `omni` lightu.
- Ctrl+V používa ghost placement, WASD/QE a Space/LMB rovnako ako ostatné gameplay prvky.
- Klikací radius light markeru je 24 px, aby bol marker praktickejšie vyberateľný.
- Fail-closed zostáva zachovaný: neoverený typ lightu sa ako nový light neautorizuje.

Log marker: `0.5.28-light-clipboard-v8`.
