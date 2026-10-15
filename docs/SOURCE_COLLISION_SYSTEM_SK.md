# Všeobecné overovanie kolízií zo zdrojových máp

## Použitie patchu

Rozbaľ ZIP do koreňa svojho existujúceho GitHub projektu, kde je CMakeLists.txt. Zachovaj adresáre a nahraď priložené súbory. Ide o zdrojový patch, nie celý projekt ani hotové EXE. Vychádza z poslednej verzie s opravami gameplay overlays a názvu ProTanki Editor PRO; zahŕňa aj nadväzujúce zmeny CTI. Ak si odvtedy upravoval tie isté súbory, zlúč zmeny namiesto slepého prepísania.

GitHub musí zostaviť projekt a pribaliť assets/source-collision-catalog.xml. CMake ho kopíruje aj pri lokálnom zostavení. Starší SourceMapMeshCollision.h už aktívny kód nepoužíva; môže v repozitári zostať.

## Príčina

V logu sa vloženie CTI aj Envb5 rušilo pri kontrole kolízií. Viditeľný mesh nie je automaticky fyzikálny mesh. Niektoré dodané 3DS nemajú pôvodné pomocné kolízne uzly, hoci pôvodná mapa obsahuje samostatné collision-plane a collision-triangle prvky. Preto ani úspešné zobrazenie modelu nezaručuje dostatok údajov na správne nové vloženie.

Rozbor pôvodného SWF ukázal výber meshu cez library mesh/@object (inak prvý koreňový objekt), zber priamych detí meshu okrem occ a export pomocných plane/box/tri prvkov. Nie je to dôkaz, že každý viditeľný trojuholník má byť kolíziou. Pri CTI sa z 84 vizuálnych trojuholníkov 82 zhoduje s fyzikálnymi plochami; dva protiľahlé vnútorné trojuholníky v pôvodných kolíziách nie sú.

## Nové spoločné riešenie

1. Zachováva prednosť existujúcich natívnych kolíznych helperov. Ich chyby neobchádza odhadom podľa vizuálneho obalu.
2. Pri načítanej zdrojovej mape a dostupnej knižnici skúša priradiť pôvodné kolízne plochy k lokálnej geometrii objektu. Vyžaduje aspoň dva priestorovo odlišné výskyty a zhodný výsledok pri všetkých odlišných výskytoch daného typu.
3. Kontroluje vrcholy, orientáciu, pokrytie plôch, duplicity a prekrytia. Neznáme XML metadáta alebo neúplné pokrytie bránia vytvoreniu šablóny.
4. Overenú šablónu viaže súčasne na knižnicu/skupinu/názov a presnú geometriu. Rovnaký vizuálny mesh iného dekoratívneho objektu preto automaticky nepreberie fyziku. Zmena geometrie zneplatní zhodu.
5. Používa spoločný výsledok pri vložení a naviazaní pôvodných kolízií. Protichodné šablóny označí za konflikt; nevyberie náhodne jednu z nich.
6. Ukladá overené šablóny medzi spusteniami. Pribalený katalóg obsahuje 18 už overených typov, ktoré fungujú bez opätovného otvorenia referenčnej mapy. Ďalšie môže editor overiť po otvorení vhodnej pôvodnej mapy s jej knižnicou.
7. Pri odmietnutí vloženia ukáže dôvod a možnosti ponechať náhľad alebo zrušiť vloženie. Nevznikne dojem ignorovaného kliknutia. Počas dialógu sa náhľad neposúva ani neprepína.

Používateľský katalóg je v %LOCALAPPDATA%\GTanksNextEditor\source-collision-catalog.xml. Starý názov adresára nastavení zostáva kvôli kompatibilite. Katalóg má formát v2 a zapisuje sa cez dočasný súbor. Neplatný súbor neprepíše už načítané údaje.

Pribalené typy: cti, flipt, land 1, land 2, line 1, line 2, t11, t21, t22, t32, t33, t55, CB7, wt1, wt1x2, wt1x3, wts1, wts1x2. Sú to výsledky overenia dát, nie pravidlá typu „ak názov obsahuje CTI, vymysli kolíziu“.

## Čo bolo overené

- 81 existujúcich Python auditov a kontrola konzistencie zdrojov prešli.
- Nový CPU regresný test používa ľubovoľné názvy objektov. Overuje opakované zdrojové výskyty, vloženie, transformovaný export, opätovné načítanie, uloženie katalógu a odmietnutie zmenenej geometrie, konfliktov, chýbajúcich alebo duplicitných kolízií a neznámych metadát.
- Test CTI preveril všetkých 11 pôvodných výskytov: spolu 132 rovín a 638 trojuholníkov, teda 12 + 58 na výskyt. Prešlo uloženie, opätovné načítanie, transformovaná kópia, Undo/Redo a mazanie.
- Na celej dodanej mape Fogtown bolo overených 18 z 57 kandidátskych typov bez helperov. Zo 16 206 relevantných výskytov sa 16 195 dalo jednoznačne naviazať; 11 nejednoznačných ostalo bez nového vlastníctva kolízií. Ich pôvodné kolízie zostávajú v mape.
- Po načítaní, overení, naviazaní a uložení bolo normalizované XML celej mapy rovnaké: 18 286 objektov, 22 211 kolíznych prvkov a 17 svetiel. Porovnanie zahŕňa atribúty, textové hodnoty a poradie prvkov, ignoruje len formátovanie a poradie atribútov. Nejde o tvrdenie o bajtovo identickom súbore.
- Nové C++ testy sú pridané do CMake/CTest pre následný build na GitHube.

## Hranice opravy

Toto nie je záruka, že ľubovoľný neúplný model získa správnu fyziku. Pri 39 skúmaných typoch nebol dôkaz dostatočný; Envb5 medzi vyriešené typy nepatrí. Môže potrebovať pôvodné helpery alebo explicitné kolízne dáta. Bez nich nemožno spoľahlivo určiť neviditeľnú, zjednodušenú alebo od vizuálu odlišnú fyziku.

Nové učenie je zámerne obmedzené na preukázateľné roviny a trojuholníky. Natívne kolízne boxy používa existujúci importer; tento systém nevyrába boxy z vizuálnych rozmerov. Naklonené alebo inak nepodporované prípady odmietne. Existujúca výslovná pokročilá voľba vizuálneho vloženia sa nemení, automatický priechodný fallback sa nepridáva.

Presné zachovanie XML bolo overené na dodanej mape, nie na každej existujúcej mape ani v hernom engine. Kopírovanie kolízií bolo overené backendovým testom, nie ručným klikaním vo Windows.

Kompletný MSVC/NSIS build ani spustenie D3D11 aplikácie a test jazdy v hre tu neprebehli. Dostupný lokálny Windows syntax checker padal bez diagnostiky; preto sa úspech Windows zostavenia netvrdí. Pred vydaním EXE musí prejsť GitHub build a CTest. Linuxové CPU testy neoverujú ovládače GPU ani Windows GUI.
