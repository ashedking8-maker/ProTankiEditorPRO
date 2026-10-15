# Fogtown: oprava vkladania 3DS bez pomocných kolízií

## Zistená príčina

Log z 2026-10-16 potvrdzuje, že `env5.3ds`, `env6.3ds` a `v1.3ds` sa úspešne načítajú do náhľadu, ale pri umiestnení editor vypíše `No native plane/box/triangle helpers in this model` a vráti mapu do pôvodného stavu. Nejde o chybu textúry ani nenačítaných vrcholov; konkrétne objekty neobsahujú *pôvodnú pomocnú kolíznu geometriu* a pôvodná verzia povoľovala bezpečné vloženie len s jej dôkazom. `cti` má osobitnú kolíziu overenú pôvodnou mapou a funguje.

Dodaný `Fogtown(2).tara` má 327 súborov s rovnakými bajtmi ako `Fogtown(1).zip`; ani iný variant knižnice teda dodatočné kolízne dáta neposkytuje.

## Čo bolo zmenené

- Pri umiestnení sa naďalej preferujú originálne 3DS kolízne helpery, pôvodná mapa, katalóg overených kolízií a overené povrchy. Použitie pôvodných dát sa nemení.
- Ak neexistujú helpery ani overená kolízia, `MeshDerivedCollision::Read` vyberie **skutočné vizuálne vrcholy a trojuholníky** z 3DS, odstráni lokálnu 3DS maticu/pivot a z nich vytvorí natívne kolízne trojuholníky. Používa stabilnú os rotácie aj pre zvislé steny (napr. `peter.3ds`).
- Degenerované nulové trojuholníky sa preskočia a ich počet sa zapíše do logu. Ak nie je žiadna reálna plocha, vloženie sa odmietne.
- Pri nejednoznačnej hierarchii, nesprávnej pomocnej kolízii, neplatnom 3DS, nebezpečnom transformačnom meradle alebo neúnosne veľkom modeli fallback nevyrába fiktívne kolízie; pôvodné bezpečnostné kontroly stále platia.
- Výsledná kolízna geometria ide rovnakou existujúcou cestou cez `MapDocument::AddImportedCollisionForProp`: je naviazaná na objekt, dá sa spolu s ním presúvať, otočiť, zmazať a štandardne exportovať v mapovom XML.
- UI a log viditeľne odlišujú **približnú kolíziu odvodenú z vizuálu** od overenej originálnej kolízie.

## Rozsah reálneho testovania

- S CPU C++ buildom prebehlo všetkých 80 odlišných Fogtown `.3ds` modelov: 80 načítaných, 0 odmietnutých, maximálny rozdiel pri spätnej rekonštrukcii plôch ≈0,0044 mapovej jednotky.
- Nový regresný C++ test testuje `env5`, `env6`, `v1`, `peter`, dekódovanie/rekonštrukciu plôch, degenerované trojuholníky, prednosť originálnych helperov a neplatný binárny súbor.
- Prešli aj existujúce CPU testy pôvodných svahov Fogtown a politiky vkladania kolízií. Uvedený nový test je registrovaný aj vo Windows CTest.
- Kompletný Windows/MSVC build, GUI, zápis mapového XML v bežiacom Windows editore a fyzické kolízie v ProTLVK **neboli v tomto prostredí spustené**. ZIP obsahuje zdrojový projekt, nie vyskúšané EXE.

## Dôležitá hranica

3DS nemôže poskytnúť originálnu hernú fyziku, ak v ňom vôbec nie je. Táto oprava v takom prípade používa presné *viditeľné plochy* ako približnú fyziku. Môže sa líšiť od herného zámeru (napr. priechodné dekorácie, zjednodušené kolízie budov). Oproti pôvodnému zdroju preto môže vytvoriť viac trojuholníkov. Originálne kolízie majú vždy prioritu. Otestuj novovložené objekty v ProTLVK a ich kolízne plochy cez kláves `G`.

## Použitie

1. Rozbaľ tento kompletný zdrojový projekt (nie iba hotfix súbory) do GitHub repozitára, ktorý buduje editor z `src/`.
2. Spusť Windows build cez existujúci GitHub Actions workflow (`.github/workflows`).
3. V novom zostavení vyber knižnicu a mapu, umiestni `Fogtown/bg/Envb5`, `Fogtown/bg/Envb6` a `Fogtown/b/V1`; nemali by dostať pôvodné chybové okno o chýbajúcich helperoch.
4. Vyskúšaj otočenie, posun, Undo/Redo, Save a Reload; stlač `G`, aby si videl kolízie.
5. Testuj výsledné XML v ProTLVK, hlavne steny a strechy. Pred testom si zálohuj pôvodnú mapu.
