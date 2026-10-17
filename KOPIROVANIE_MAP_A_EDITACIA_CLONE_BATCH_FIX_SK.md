# ProTanki Editor PRO – oprava editovania objektov po kopírovaní celej mapy

## Potvrdená príčina
V logu `ProTankiEditorPRO-20261018-020116-pid23352.log` je zapísané:

- `Clipboard selection: selected=909 document=909 full=1`
- `Lossless native static paste committed: props=909 planes=1074 boxes=145 triangles=759`
- `Delete request: selected=796 totalProps=3049 ...`
- `Deletion blocked: lossless native clone batch 1 is only partially selected (793/909)`

Pôvodný mechanizmus uzamkol **celú** vloženú kopírovanú skupinu, keď v nej zostala čo len jedna kolízia bez jednoznačne overeného vlastníka. Rovnaká plošná podmienka blokovala zmenu polohy jednotlivých objektov.

## Čo sa zmenilo

1. Individuálny pohyb/skupinové mazanie teraz posudzuje **vybraný objekt**, nie celú skupinu. Objekt s presne overenými vlastnými natívnymi kolíziami sa môže upraviť, aj keď v jeho dávke existujú iné nepriradené kolízie.
2. Pri čiastočnom odstránení sa vymažú iba kolízie s explicitným vlastníkom vymazaného objektu. Neznáme kolízie ostanú zachované, nepremiestňujú sa „odhadom" podľa rovnakého stredu.
3. Ak sú označené všetky **zostávajúce** objekty danej kopírovanej skupiny, odstránia sa aj všetky jej zostávajúce nepriradené kolízie. Môže ísť iba o jednu kopírovanú skupinu, nie nevyhnutne celú mapu.
4. Ak je vlastníctvo vybraného objektu naďalej neoverené, editor vypíše jeho **index a identitu** a zmenu bezpečne odmietne. Nepovolená operácia nesmie vykonať ani čiastočné zmazanie.
5. Pri novom vložení celé mapy log zobrazí `Lossless clone ownership: ... verifiedProps=... opaquePlanes=... opaqueBoxes=... opaqueTriangles=...`.

**Čo sa NEROBÍ:** program nevymýšľa vlastníctvo kolízií na základe vzdialenosti, nemení modely, textúry, importný parser ani XML export. Nie je možné garantovať individuálne úpravy úplne *každého* neznámeho objektu bez bezpečne zisteného priradenia pôvodných natívnych kolízií.

## Inštalácia

Tento malý balík patrí na posledný zdrojový projekt, ktorý už obsahuje opravy Fogtown importu, AX Library a Geometry G Live Refresh. Zálohuj projekt, rozbaľ ZIP do jeho **koreňového priečinka** so zachovaním štruktúry `src/`, `tests/` a prepíš existujúce súbory. Zostav nový Windows installer v GitHub Actions. Samotná aktualizácia zdrojákov nemení už nainštalovaný `.exe`.

## Overovanie

- Pri skopírovanej mape skús presunúť a zmazať jednu budovu/terén, ktorého kolízie sa naviazali; Geometry View (`G`) musí ukázať správnu polohu aj po Undo/Redo.
- Skús mazať viac overených objektov naraz. Kolízie ostatných objektov musia zostať.
- Skús zmeniť objekt, ktorého kolízie zostali nejednoznačné – zmena sa musí odmietnuť **s konkrétnou identitou**; nesmie nastať čiastočné zmazanie.
- Skús zmazať **celú zostávajúcu kopírovanú skupinu** a skontroluj v Geometry View, že nezostali jej nepriradené kolízie.
- Používaj *Save As* a ponechaj originálnu mapu ako zálohu. Po opätovnom načítaní skontroluj aj export a kolízie v hre.

## Testy

- `tests/native_clone_edit_policy_regression.cpp` – prenositeľný C++ test pravidiel bezpečného výberu (spustený s GCC).
- `tests/native_clone_partial_edit_regression.cpp` – regresný scenár MapDocument vrátane čiastočného/všetkého mazania, nepovolenej transformácie a zachovania neznámych kolízií; registrovaný do Windows CTest.
- `tests/test_native_clone_partial_edit_audit.py` – statický audit prepojenia implementácie a testov.

V tomto prostredí bolo spustených 29 Python auditov a prenositeľný C++ policy test. Celé Windows zostavenie ani runtime MapDocument C++ test tu neboli spustené. Pred nasadením do hlavnej mapy sú potrebné.
