> Historický popis V4. Dva snap režimy nahrádza jediný Grid snap v `OPRAVY_V5_SK.md`.

# Opravy V4 — dva režimy snapu a ručné pridanie do AX

Kumulatívny patch k zaslanému ProTankiEditorPRO-main 0.5.28, 2. 10. 2026. Obsahuje V1–V3 aj nové zmeny. Log zostavený z tohto zdroja má označenie `0.5.28-scene-snap-ax-v4`.

## Grid snap a Total grid snap

V Tools a v Placement settings sú pod sebou dve samostatné voľby:

| Voľba | Správanie |
|---|---|
| Grid snap | Jemné prisunutie blízkej hrany z polohy určenej bežnou mriežkou. Dosah je 55 % kroku, najviac 10 jednotiek. Pri kroku 10 sa preto opraví aj odchýlka 4,25. |
| Total grid snap | Vyhľadáva hrany z nezaokrúhlenej polohy kurzora. Vie zachytiť hranu medzi uzlami hrubej mriežky, napríklad 424,25 pri kroku 500. Dosah je 55 % kroku, obmedzený na 2 až 275 jednotiek. |

Total grid snap je predvolene zapnutý. Grid snap je vtedy odškrtnutý a jeho ovládanie zablokované. Vypnutie Total automaticky zapne jemný Grid snap; ten možno následne tiež vypnúť. V každom snímku sa vykonáva iba jeden režim prisúvania, nie dva za sebou.

Samostatné nastavenie `Coordinate grid (XYZ)` ovláda bežnú súradnicovú mriežku. Je predvolene zapnuté. Ak sa nenájde vhodná hrana, umiestnenie zostáva na tejto mriežke. Hrana zachytená jedným zo snap režimov má prednosť. Nastavenie kroku a jemnejší krok so Shift zostávajú zachované.

## Okolité objekty a stabilita ghostu

V3 obmedzovala referenciu na posledný vložený objekt. V4 vyhľadáva medzi všetkými blízkymi mesh objektmi už vykreslenej scény, vrátane objektov z načítanej mapy. Nie je potrebné objekt najprv znovu vložiť ani označiť.

Najprv sa vykoná lacná kontrola priestorových hraníc. Presnejší vonkajší obrys sa počíta iba pre blízke meshe a ukladá do cache. Pri transformácii objektu alebo prestavaní scény sa príslušná cache zruší. Z kandidátov sa vyberie najmenšia korekcia; pri rovnakej vzdialenosti je poradie stabilné.

Total používa pôvodnú polohu kurzora pred zaokrúhlením na mriežku. Každý kandidát sa porovnáva s tou istou vstupnou polohou. Výsledok sa priradí ako výsledná poloha ghostu a nepripočítava sa k výsledku minulého snímku. Korekcia je obmedzená dosahom. Tým sa odstraňuje skladanie korekcií a nevhodné rozhodovanie iba podľa už zaokrúhlenej polohy.

Spresnená je tiež kontrola kontaktu: dotykový bod/úsek musí ležať pri konečnej hrane, nestačí priblíženie k jej nekonečnému predĺženiu. Aktívna vodiaca čiara je označená podľa použitého režimu. Zobrazí sa iba pri skutočne nájdenom kandidátovi, nie nepretržite.

Malá medzera `Edge clearance` zostáva predvolene 0,02 jednotky. Súradnica pivotu preto nemusí byť rovnaká ako súradnica cieľovej hrany: zohľadňuje rozmery a rotáciu modelu aj túto medzeru. Vyhľadávanie rešpektuje výškový rozsah objektov a neprisúva predmety z oddelených poschodí.

Dodaný log identifikuje V3 a obrázky ukazujú aj aktivovaný vodiaci úsek. Neobsahujú však súvislý záznam pohybu kurzora a polôh ghostu. Presný používateľov priebeh „utekania“ preto nebol reprodukovaný v spustenej Windows aplikácii; oprava vychádza z kontroly zdrojov a testov stabilného výpočtu.

### Geometrický rozsah

Algoritmus používa vonkajší konvexný obrys renderovaného meshu po rotácii. Podporuje nepravouhlé rotácie, ale nezaručuje prispôsobenie vnútorným výrezom konkávnych L/U modelov. Pri nerovnobežných stranách môže byť kontakt rohom k strane. Ide o prisunutie nového samostatného mesh objektu; nie je to všeobecná kontrola prienikov so všetkými objektmi, skladanie ľubovoľných 3D povrchov ani zarovnávanie sprite objektov či kopírovaných skupín. Pri chýbajúcom meshi sa nevymýšľajú falošné hrany.

## Pravý klik → AX library

Pravý klik na položku v Library (vo vyhľadávaní aj stromovom zozname) alebo na kartu v Browse Library:

- vyberie model a pripraví ho na umiestňovanie;
- označí položku ako ručne pridanú do AX, bez vloženia objektu do mapy;
- zachová jej dostupnosť v AX aj pri zapnutom `Only used` a pri prázdnej mape;
- pri Browse zachová kliknutú textúrovú variantu a nechá prehliadač otvorený, aby bolo vidieť potvrdenie;
- zobrazí zelené zvýraznenie na jednu sekundu a potom ho počas 0,6 sekundy plynulo stlmí na bežný stav vybranej položky.

Ľavý klik funguje ako predtým. AX zoznam aj Tab + koliesko používajú rovnaký filter, takže ručne pridaná položka je dostupná oboma cestami. Neplatné indexy sa stále ignorujú. Ručne pridané položky sa nevyhadzujú pri bežnom orezaní posledných 24 položiek histórie; odstrániť sa dajú existujúcim odobratím z AX.

Ručné pridanie je stav aktuálnej relácie editora. Nevkladá fiktívny objekt do XML a nepretrváva po reštarte aplikácie či výmene zdrojovej knižnice, rovnako ako doterajšia pamäťová AX história.

## Overenie a nahratie

Lokálne prešlo 79 Python kontrol, kontrola konzistencie zdrojov a sedem C++ regresných programov. Nové prípady overujú odlišný dosah jemného/Total režimu, zachytenie hrany pri kroku 500, stabilný výber spomedzi viacerých kandidátov počas 1 000 opakovaní bez akumulácie, zachovanie ručne pridanej AX položky vo filtri Only used a časovanie zeleného potvrdenia. Matematický test prisúvania aj test AX sú súčasťou Windows CTest.

Kompletný Windows/MSVC/Direct3D build a klikací priebeh v EXE tu neboli vykonané. Výsledky CPU testov nenahrádzajú vizuálne overenie. Po GitHub zostavení over oba režimy pri krokoch 10 a 500, staršie objekty načítanej mapy, rotáciu ghostu, pravý klik v oboch knižniciach a prepínanie cez Tab.

Rozbaľ ZIP a nahraj jeho celý obsah do koreňa existujúceho projektu so zachovaním `src/`, `tests/`, `tools/`, `docs/` a koreňových súborov. Prepíš rovnomenné súbory, ale nevymaž ostatné súbory projektu. GitHub workflow musí zostaviť projekt a spustiť CTest pred vydaním EXE. Nie je potrebné osobitne aplikovať staršie ZIP-y. Podrobný zoznam zmien so SHA-256 je v `PATCH_IMPORT_EXPORT_MANIFEST.json`.
