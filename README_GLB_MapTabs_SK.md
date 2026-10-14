# GLB textúry a viac otvorených máp

## Inštalácia

Rozbaľ obsah ZIP-u do koreňa aktuálneho GitHub projektu. Zachovaj priečinky
src, tests a tools a nahraď priložené súbory vrátane CMakeLists.txt.
Pridaj aj nové hlavičky, testy a malé GLB testovacie modely. Commitni všetky
zmeny spolu a spusti existujúci GitHub Actions build.

Patch nadväzuje na ObjectEditor_safety_fix. Obsahuje aj zmenené súbory oboch
nasledujúcich CI hotfixov, takže ich netreba znova aplikovať po tomto ZIP-e.
Nie je to celý repozitár ani hotový EXE. Ostatné súbory projektu ponechaj.
Zoznam dodaných súborov a ich SHA-256 je v PATCH_FILES.sha256.

## Viac máp naraz

- File > Open otvorí mapu v novej karte. Existujúce mapy ostávajú v pamäti.
- File > New alebo tlačidlo + vytvorí ďalší NewProject.
- Namiesto názvu Viewport sa zobrazuje názov aktívneho XML bez prípony,
  prípadne NewProject. Pod titulkom sú karty všetkých otvorených máp.
- Už otvorené XML sa aktivuje namiesto otvorenia druhej nezávislej kópie.
- Každá mapa má samostatný dokument, príznak neuložených zmien, Undo/Redo
  a uloženú polohu kamery. Pri prepnutí sa dokument nečíta znova z disku.
- Clipboard je spoločný. Označ objekty, Ctrl+C, klikni na inú kartu a Ctrl+V.
  Objekty sa objavia ako ghost skupina; Space ich vloží, pravé tlačidlo zruší.
  Výška kopírovanej skupiny sa prenáša zo zdroja.
- Prenášajú sa aj existujúce balíky natívnych kolízií a oddelené pôvodné XML
  uzly. Clipboard ostáva použiteľný aj po zatvorení zdrojovej mapy.
- Index objektu v zdrojovom XML sa pri prenose do inej mapy nepovažuje za index
  cieľového objektu. Nové kolízne ID sa vytvárajú pri serializácii cieľa.
- X zatvorí kartu. Pri neuložených zmenách nasleduje Save / Don't save / Cancel.
  Pri zatváraní aplikácie sa postupne riešia všetky zmenené mapy. Cancel
  ponechá karty otvorené, aj tie už potvrdené cez Don't save.
- Save As odmietne cestu XML, ktoré je otvorené v inej karte.
- Undo po Save As nemení cestu späť na starý súbor.

Mapy zdieľajú jednu zvolenú knižnicu assetov. Zoznam otvorených kariet je stav
aktuálneho spustenia, nie nový projektový formát ani automatická obnova relácie.
Pri prepnutí sa zruší rozpracovaný ghost a výber; dáta mapy sa nemenia.
Doterajšie ochrany proti kopírovaniu nejasných či nepodporovaných kolízií ostávajú.
Karty samy osebe neodstraňujú obmedzenia existujúceho importu.

## GLB v objektovom editore

Importér načíta vložené PNG/JPEG obrázky base color, správny UV kanál a farbu
materiálu. GPU náhľad dekóduje obrázky z pamäte cez WIC. Zmena Draft tint už
neprepíše vloženú textúru jednoliatou farbou. Opravená je orientácia UV medzi
glTF, Assimp a rendererom a orientácia trojuholníkov pri zrkadlenom uzle.

Po novom importe GLB sú body a hrany vypnuté, aby bolo vidieť povrch.
Zapneš ich cez Show vertices / Show face edges. Nový GLB dostane počiatočnú
kolíznu obálku podľa rozmerov modelu, nie nesúvisiaci predvolený box.

Táto obálka je kváder, nie presná kolízia tvaru! Pri dverách, otvoroch či
zložitom modeli zapni Show collision boxes a uprav alebo rozdeľ boxy.
Fit selected box to mesh bounds vie obálku znovu prispôsobiť modelu.

## GLB export do starej knižnice

Export NEW 3DS library vytvorí nový priečinok PTPRO_* a súrodenecký archív
.tara v zvolenom koreni knižníc. Balík obsahuje 3DS, library.xml a farebný
obrázok. Existujúci výstup neprepíše; pri chybe sa dočasné súbory odstránia.

Podporovaný je statický model s jedným vloženým nepriehľadným farebným
obrázkom/atlasom, bielym farebným faktorom materiálu a vlastnými solid boxmi.
Viac častí môže zdieľať ten istý obrázok a rovnaké nastavenie obojstrannosti.
Dekoratívny režim môže exportovať model bez boxov; pri jeho vkladaní stále
platia ochrany editora pre objekty bez kolízie.

Textúra sa prenesie bez zmeny bajtov. Natívny materiál zachová obojstrannosť.
Export sa spätne kontroluje cez natívny čítač vizuálu a kolízií. TARA má
existujúcu kontrolu zakódovania/dekódovania.

Obmedzenia sú zámerne pomenované, nie potichu ignorované:
- PBR normal map, metallic/roughness a animácia sa nezobrazujú ani neprevádzajú
  na vzhľad identický s moderným GLB rendererom. Prenáša sa base color.
- Viac rôznych farebných obrázkov vyžaduje vopred vytvorený spoločný atlas.
- Priehľadné materiály, PNG s alpha kanálom/tRNS a nebiely farebný faktor
  sú pri natívnom exporte odmietnuté; priprav nepriehľadný RGB PNG alebo JPEG.
- Skinning, morphs a vertex colors treba pred exportom zapiecť do statického modelu.
- Externé obrázky, UV transformácie a clamp/mirror sampling tento importér
  odmietne s vysvetlením; bežný vložený opakovaný PNG/JPEG je podporovaný.
- 3DS ostáva obmedzené na 65 535 vrcholov a 65 535 trojuholníkov.
- Pridané/odstránené vrcholy bez UV sa neexportujú. Posúvanie pôvodných vrcholov
  funguje. Predvolené GLB vyhladenie je smooth; dá sa zvoliť Flat.
- Triggery a odvodenie presnej prejazdovej geometrie z ľubovoľného GLB
  nie sú implementované. Kolízie GLB exportu tvoria upravené solid boxy.

## Čo bolo overené

- 81 Python auditov a kontrola konzistencie zdrojov: úspech.
- Nový C++ test kariet: prepínanie, vlastné Undo, kopírovanie XML a kolízií
  medzi dokumentmi, jedinečné kolízne ID, zatvorenie zdroja, Save As a Undo.
- Nový C++ test GLB: vložený obrázok, orientácia UV, 3DS, boxy, TARA,
  upravené vrcholy a odmietnutie nepodporovaných variantov.
- Existujúci funkčný XML/Undo test a Fogtown 200 -> 236 kolízií: úspech.
- Celý natívny exportný integračný test Land01 vrátane Fogtown: úspech.
- Dodaný Meshy Rustbound model: 282 vrcholov, 94 trojuholníkov; všetky pozície
  a UV po spätnom importe zodpovedajú zdroju v tolerancii testu; farebný JPEG
  je bajtovo identický a obojstrannosť materiálu sa zachovala.

C++ testy bežali lokálne na Linuxe s rovnakým Assimp 5.4.3 a pugixml 1.15.
Windows logovanie bolo v tomto lokálnom testovacom behu nahradené testovacou
implementáciou. Produkčný kód na Windows stále používa pôvodný logger a
MoveFileExW pri ukladaní. Testovacia náhrada nie je súčasťou patchu.

Windows/MSVC zostavenie, GPU vykreslenie, interaktívne klikanie na karty a
správanie exportu priamo v hre tu neboli spustené. Musí ich potvrdiť GitHub
build a následné spustenie EXE. Nie je to prísľub úplnej podpory každého GLB
ani dôkaz, že celý editor už nemôže obsahovať ďalšie chyby.
