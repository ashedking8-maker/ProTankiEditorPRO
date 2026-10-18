# ProTanki Editor PRO – oprava nemožnosti mazať Bush/Tree po Ctrl+C / Ctrl+V

Záznam z 18.10.2026 potvrdzuje: kopírovanie map_canyon.xml (909 objektov) prešlo, pričom editor rozpoznal 877 objektov s priradenými kolíziami a 11 nepriradených trojuholníkov. Po vložení odmietol mazať Bush/default/Tree 1 (index 2696) a Bush/default/Bush 1 (index 2529).

## Čo sa opravilo

Editor teraz po načítaní knižnice označí asset ako **overený kolízne prázdny sprite** iba vtedy, keď presná položka v AssetRegistry obsahuje sprite a nemá mesh, nemá priradenú kolíziu, nemá nevyriešeného vlastníka a nemá explicitné `with_collision=1`.

Tento dôkaz zostane v pamäti pri Ctrl+C/V a dovolí bezpečne presúvať/mazať individuálne kríky a stromy aj vedľa nepriradených mapových kolíznych trojuholníkov. Nepriradené kolízie sa nemanipulujú a nevymazávajú. **Neznáme mesh objekty ostávajú chránené**, ich kolízie sa nesmú hádať.

## Inštalácia

Rozbaľ obsah tohto ZIP do koreňa poslednej verzie zdrojového projektu s globálnou Clone Fix + GitHub CTest Fix a nahraj zmeny do GitHubu. Znova spusti `release-windows.yml`, nainštaluj nový EXE. Priložené testy sú v priečinku tests/. Nie je to samotný EXE.

## Overenie

1. Spusť aktuálny build. Načítaj knižnicu s Bush a mapu Canyon.
2. Skopíruj 909 objektov do Battle Royal a potvrď vloženie.
3. Vyber skopírovaný **Bush 1** a **Tree 1** – premiestni ich, potom zmaž samostatne.
4. Stlač `G`: 11 samostatných nepriradených trojuholníkov sa nemá premiestňovať spolu s kríkom.
5. Save As, mapu znova otvor a skontroluj fyziku/kolízie.
6. V logu hľadaj `Lossless clone ownership` a `verifiedCollisionFreeSprites=`; to druhé by malo byť 31 pre dané dáta, ak sú aktíva kompletne dostupné.

## Testovanie a limity

87 starších Python auditov + 3 nové auditné kontroly a samostatný kompilovaný C++ test politiky úspešné. Úplný Windows build/CTest a beh GUI nie sú v tomto prostredí potvrdené. Neznáme mesh s nepreukázaným vlastníctvom kolízie zostávajú zámerne blokované. Oprava nemení exportný formát, ale jeho použitie v hre treba ešte overiť.
