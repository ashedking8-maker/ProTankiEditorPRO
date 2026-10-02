# Opravy V3 — umiestňovanie, Grid snap a gameplay overlays

Balík pre zaslaný projekt ProTankiEditorPRO-main 0.5.28, pripravený 2. 10. 2026. Je kumulatívny: zahŕňa aj opravy V1/V2. Aktuálny log obsahuje označenie `0.5.28-placement-grid-v3`; názvy inštalátora ostávajú 0.5.28.

## Q/E po Quick add Blue flag

Dodaný log pochádza z `0.5.28-selection-compatibility-v2`. V kóde tejto verzie `BeginFunctionalPlacement` vyplo statický ghost, ale ponechalo výber predchádzajúceho objektu. Obsluha klávesov nemala samostatnú vetvu pohybu gameplay ghostu, preto mohla pokračovať do transformácie označených statických objektov. Log obsahuje štarty gameplay umiestňovania a transformácie statických objektov; nezaznamenáva každé stlačenie klávesu, takže presný sled používateľových vstupov z neho samotného rekonštruovať nemožno.

Oprava vyčistí starý výber a rozpracované ťahanie pri vstupe do gameplay umiestňovania. Aktívne umiestňovanie má vlastnú obsluhu WASD/QE s ukončením spracovania pred transformáciou mapových objektov. Q/E mení iba výšku ghostu; do mapy sa údaje dostanú až pri vložení. Do logu pribudla výška ghostu s poznámkou `document unchanged`.

Prepínanie späť do umiestňovania statického objektu alebo schránky ukončí gameplay režim, aby neboli súčasne aktívne dva ghosty. Zmena výšky sa zobrazuje aj v informačnom riadku gameplay ghostu.

## Výška pri zmene modelu

Výška je spoločný stav umiestňovania. Q/E ju upravuje priamo; pri výbere iného modelu v knižnici alebo cez Tab + koliesko sa už neresetuje skrytý výškový offset. Horizontálny klávesový posun sa pri zmene modelu resetuje, takže nový ghost nasleduje kurzor, ale drží poslednú výšku.

Samostatná rovina pre premietnutie kurzora zabraňuje nechcenému horizontálnemu posunu pri samotnom Q/E. Otvorenie novej mapy výšku aj referenciu hrany resetuje. Zachováva sa výška pivotu objektu; rôzne modely s odlišne umiestneným pivotom nemusia mať v tej istej výške aj spodnú plochu.

## Grid snap

V Tools aj Placement settings je názov iba **Grid snap**, predvolene zapnutý. Ovláda mriežku pre horizontálne umiestňovanie a presuny X/Y aj výšku Z cez Q/E. Shift používa desatinu kroku. Vypnutie vypne zarovnávanie umiestňovania aj magnetické hrany. Samotné zapnutie neposúva existujúce objekty.

Pri ťahaní sa zarovnáva výsledná poloha aktívneho objektu, nie iba relatívny pohyb myši. Pri ťahaní skupiny sa aplikuje spoločný posun, takže sa zachovajú rozostupy. Pri kopírovaných objektoch zostáva zachovaná fáza zdrojovej mriežky. Zmena modelu bez Q/E zachová aj výšku mimo mriežky; až výškový pohyb zarovná Z na zvolený krok.

Novou súčasťou Grid snap je prisunutie nového samostatného mesh objektu k **poslednému úspešne vloženému statickému objektu**:

1. Bežná mriežka určí kandidátnu polohu ghostu.
2. Z renderovanej geometrie po rotácii sa vytvorí konvexný vonkajší obrys v rovine XY.
3. Ak je blízko hrana posledného vloženého objektu a modely sa výškovo stretávajú, ghost sa prisunie k tejto hrane. Hrana môže mať ľubovoľné súradnice, napríklad X = 421,57; nemusí byť násobkom 100.
4. Aktívna hrana sa zvýrazní tyrkysovou vodiacou čiarou. Prisunutie sa prepočítava aj po rotácii ghostu.
5. Zachová sa malá horizontálna medzera, predvolene **0,02 jednotky**. Nastavuje sa v Tools > Placement settings > Edge clearance. Zmenšuje riziko prekrývania styčných plôch bez zvyšovania celého objektu.

Dosah prisunutia je 55 % aktuálneho kroku, s hranicami 2 až 275 jednotiek. Pri kroku 100 je to 55 jednotiek. Vzdialené hrany a oddelené poschodia kandidátmi nie sú. Nový algoritmus nevyhľadáva tisícky objektov v mape každý snímok: používa posledný vložený objekt a obrysy ukladá do cache podľa meshu a rotácie.

Podporované sú aj nepravouhlé rotácie. Rovnobežné strany sa prisunú stranou k strane; pri nerovnobežných stranách môže vzniknúť kontakt rohom k strane. Bez zmeny rotácie nie je geometricky možné vždy spojiť dve nerovnobežné strany celou plochou. Editor preto sám nemení zvolenú rotáciu.

Pri zložitých konkávnych modeloch sa používa konzervatívny vonkajší obrys, nie vnútorné výrezy. Nie je to presné skladanie ľubovoľných 3D povrchov ani kontrola všetkých prienikov v celej mape. Automatické prisúvanie sa nepoužíva pre sprites, chýbajúce modely a kopírované skupiny. Pri Undo/Redo alebo mazaní sa referencia zruší, aby po zmene indexov nesmerovala na iný objekt. Ďalšie vloženie nastaví novú referenciu.

## AX a overlays

**AX Library: only used** je predvolene zapnuté. Filtruje položky histórie AX podľa objektov použitých v aktuálnej mape. Knižnica Browse Library zostáva cestou na pridanie doteraz nepoužitého modelu.

**Show gameplay overlays** zapína všetkých päť podvolieb: spawny, vlajky, kontrolné body, bonusové oblasti a kill/kick zóny. Ak bol filter `None / hide all`, zmení sa na `All`. Iný zvolený herný režim sa zachová. Vypnutie odškrtne všetkých päť volieb a ich ovládanie skryje. Rovnaká logika platí pre položku View a klávesovú skratku. Jednotlivé podvoľby sa po zapnutí dajú opäť ručne vypínať. Samostatné svetlá nie sú súčasťou týchto piatich gameplay prepínačov.

## Overenie

Lokálne prešlo 79 Python kontrol, kontrola konzistencie zdrojov a šesť samostatných C++ regresných programov. Nový `PlacementSnapRegression` overuje hranu 421,57, všetky štyri smery, nepravouhlé rotácie, nerovnobežný kontakt, oddelenie obrysov s medzerou, rôzne poschodia, ploché dlaždice, duplicitné vrcholy, grid on/off, zdrojovú fázu a stabilnú výšku po 1 000 opakovaniach bez výškového vstupu. Je pridaný aj do Windows CTest.

Matematické testy bežali priamo nad novým CPU algoritmom. Beh kompletného Windows/Direct3D UI, ovládanie myšou a zostavenie EXE tu neboli vykonané. Zdrojová kontrola nenahrádza MSVC kompiláciu. GitHub workflow musí zostaviť projekt a spustiť CTest pred vydaním EXE. Výpis lokálnych testov je v `IMPORT_EXPORT_TEST_RESULTS.txt`.

Po zostavení odporúčaný krátky priebeh: označ starý objekt, Quick add Blue flag, Q/E a over nezmenenú polohu starého objektu; zruš ghost. Vlož dlhý blok, uprav výšku, cez Tab zvoľ použitý múr alebo vyber iný v knižnici a over zachovanú výšku. Vyskúšaj priblíženie zo všetkých strán, X rotáciu a tyrkysovú vodiacu čiaru. Vypni Grid snap a porovnaj voľné umiestnenie. Nakoniec over master prepínač overlays z režimu None.

## Použitie balíka

Rozbaľ ZIP a nahraj celý rozbalený obsah do koreňa existujúceho repozitára. Zachovaj adresáre `src/`, `tests/`, `tools/`, `docs/` a prepíš rovnomenné súbory. Nevymaž ostatné súbory projektu. Samotný ZIP nahraný ako jeden súbor kód nezmení.

Tento balík už obsahuje V1/V2; nemusíš aplikovať staršie archívy pred ním. Nový rozbor V3 nahrádza starší popis výškového offsetu a samostatného voliteľného edge snapu. Import/export opravy z V1/V2 zostávajú zahrnuté. Balík neobsahuje zostavenú EXE ani pôvodné knižnice modelov.
