# Opravy V2 — výber, kopírovanie, ovládanie a kompatibilita exportu

Tento kumulatívny balík je určený pre zaslaný projekt ProTankiEditorPRO-main 0.5.28. Obsahuje aj predchádzajúce opravy importu/exportu. GPU vykresľovanie zostáva zachované. Nie je to hotová EXE; tú zostaví existujúci GitHub Windows workflow.

## 1. Čo ukázal tvoj log a prečo sa mohli strácať objekty pri výbere

Log `GTanksNextEditor-20260929-181155-pid25964.log` uvádza 1 039 objektov mapy, ale iba 995 vykreslených mesh inštancií a 36 sprite inštancií, spolu 1 031. Osem načítaní `library/Exterior/cond1.3ds` skončilo chybou „Ambiguous 3DS roots: set mesh object in library.xml.“ Predchádzajúca oprava bola pri viacerých koreňoch príliš prísna.

Zároveň výber obdĺžnikom závisel od úspešne načítaného modelu. Objekt zostal v dokumente, ale bez výberovej reprezentácie mohol chýbať vo výbere. Kopírovanie potom dostalo iba časť statickej mapy. To je konkrétna chybná cesta potvrdená kódom. Dodaný log končí po zostavení scény, neobsahuje samotnú udalosť Ctrl+C/V, takže presný priebeh posledného pokusu z neho nemožno dokázať.

Oprava má dve časti:

- Spoločný výber 3DS objektu stále uprednostňuje výslovný `mesh/@object` z knižnice. Bez neho použije prvý deklarovaný použiteľný koreňový mesh. Pri viacerých možnostiach vydá upozornenie namiesto odmietnutia celého modelu. Rovnaké pravidlo používajú vizuál aj kolízny importér. Chybné explicitné meno ani poškodená hierarchia sa tým nezakrývajú.
- Aj objekt s chýbajúcim alebo nenačítateľným modelom dostane výberový objem pri svojej polohe. Jeho mapové dáta sa tým nestratia z výberového systému. Ctrl+A vyberá priamo všetky statické objekty dokumentu; nezávisí od GPU dávok ani úspechu načítania knižnice. Pri celej statickej mape sa kopíruje pôvodný kompletný kolízny balík. Pri čiastočnom výbere zostáva ochrana pred nesprávnym priradením neznámych kolízií; pri nejednoznačnosti editor vysvetlí blokovanie a odporučí Ctrl+A na celú statickú mapu.

Do logu pribudol počet kopírovaných objektov oproti dokumentu a informácia o úplnosti výberu. Tak sa pri ďalšom hlásení dá odlíšiť problém načítania, výberu a schránky.

Toto nie je výnimka pre názov cond1 alebo konkrétnu mapu. Samotný `cond1.3ds` však medzi dostupnými prílohami nebol. Pôvodný AIR používa pri chýbajúcom explicitnom mene `Set.peek()`; poradie tejto množiny neviem pre ľubovoľný nejednoznačný súbor zaručene napodobniť. Prvý deklarovaný mesh je deterministický fallback, nie dôkaz presnej zhody každého takého modelu s AIR. Pre absolútne jednoznačný výber slúži `mesh/@object`.

## 2. Fabr Tower: výsledok porovnania AAA a BBB

Porovnávané sú presne dodané exporty `TEST_MAP_AAA(1).xml` a `TEST_MAP_BBB.xml`. Oba obsahujú jeden objekt `City Builds/default/Fabr Tower`, bez rotácie.

| Údaj | AAA — nový editor | BBB — originál |
|---|---|---|
| Poloha objektu X/Y/Z | 0 / 0 / 0,500 | 500 / 0 / 0 |
| Kolízne roviny | 6 | 6 |
| Boxy / trojuholníky | 0 / 0 | 0 / 0 |
| Roviny po odčítaní polohy objektu | Presná zhoda s BBB | Presná zhoda s AAA |
| ID rovín | 0 až 5 | Opakované 0 |

Porovnanie používa desatinné čísla bez tolerancie a ignoruje iba poradie záznamov a ID. Pozície rovín vzhľadom na objekt, rozmery aj rotácie sa zhodujú. Rozdiel polohy celého objektu je (-500; 0; +0,500). To znamená, že dodané XML nie sú textovo identické, ale lokálna kolízna geometria veže je identická.

Oba exporty majú šesť bočných rovín. Ani jeden nepridáva uzatváraciu hornú/dolnú plochu alebo plný box. Z týchto dát preto nevyplýva rozdiel v nepriechodnosti pri rovnakom umiestnení; fyziku v hre som tu nespúšťal a netvrdím, že ide o uzavretý nepriechodný objem zo všetkých smerov.

Automatický posun Z o 0,5 bol reálny posun objektu aj kolízií, nie iba grafická korekcia. Predvolene je teraz vypnutý. Nastavenie má nový kľúč `nativeSurfaceOffsetEnabled`, takže staré uložené automatické zapnutie sa neprenesie. Používateľ môže posun znovu výslovne zapnúť. Tým sa pri štandardnom umiestnení nevnucuje výškový rozdiel oproti originálu.

Opakovanie chyby pri inej rotácii, inom modeli alebo inej knižnici týmto jedným porovnaním nie je vylúčené. Test presne pokrýva tvoje dva exporty a nepredstiera univerzálny simulátor fyziky.

## 3. Hromadný výber gameplay prvkov a mazanie

Výber obdĺžnikom zahŕňa aj viditeľné vlajky, spawny, kontrolné body, bonusové oblasti, špeciálne zóny a svetlá. Rešpektuje prepínače viditeľnosti a filter herného režimu. Shift pridáva kliknutú položku, Ctrl ju prepína vo výbere; obdĺžnik s modifikátorom pridáva položky. Ctrl+A vyberie všetky statické objekty a aktuálne viditeľné gameplay položky.

Delete odstráni celú vybranú skupinu vrátane zmiešaného výberu objektov a gameplay. Pri mazaní sa indexy spracujú od najvyššieho, aby zmenšovanie zoznamov nepreskočilo položky. Operácia má jeden Undo záznam; pri chybe sa gameplay operácia vráti na predchádzajúci dokument. Označené gameplay položky majú zvýraznenie a Properties zobrazí počet členov skupiny.

Keďže Ctrl slúži na výber, zmena veľkosti bonusových oblastí a zón používa Ctrl+Shift. Nová hromadná funkcia zahŕňa výber a mazanie, nie skupinový presun ani kopírovanie gameplay. Pri zmiešanom výbere Ctrl+C kopíruje statické objekty a ich príslušnú kolíznu geometriu; gameplay skupinu tým neklonuje. Samostatné kopírovanie jedného podporovaného gameplay prvku ostáva.

## 4. Držaný objekt a usporiadanie panelov

- Pred vložením WASD posúva objekt v horizontálnej rovine vzhľadom na kameru. Q znižuje a E zvyšuje jeho výšku. Shift zjemňuje krok. Klávesový posun sa pripočítava k náhľadu pod kurzorom, takže ho nasledujúci snímok neprepíše. Zmena výšky nemení horizontálnu polohu v dôsledku perspektívy. Existujúca rotácia X zostáva.
- Predvolené poradie dokovaných panelov je Properties, Scene, Lighting. Upravené je aj poradie ich vytvárania.
- Kliknutie na objekt v Scene aktualizuje rovnaký zoznam výberu ako kliknutie vo viewporte; nezostáva iba vizuálne aktívny riadok bez položky pre kopírovanie.

## 5. Overenie a jeho hranice

Lokálne prešlo 79 Python kontrol, kontrola konzistencie zdrojov a päť samostatných C++ regresných programov. Nové overenia pokrývajú presnú zhodu šiestich rovín AAA/BBB, výber 1 000 gameplay položiek, deduplikáciu, filtre viditeľnosti, obdĺžnikový výber a fallback výberu 3DS koreňa. CPU testy používajú iba typovú náhradu `XMFLOAT3` tromi floatmi; nenahrádzajú algoritmy importu.

Do Windows CTest boli doplnené testy kopírovania a uloženia 2 000 objektov s 2 000 kolíznymi boxmi bez dostupnej knižnice, ako aj hromadného mazania gameplay s Undo/Redo a opätovným načítaním XML. Tieto Windows/XML testy tu neboli spustené. V tomto prostredí nie je kompletný Windows SDK/Direct3D/Assimp zostavovací reťazec; nie je tu overený celý MSVC build, klikací priebeh GUI ani hra. Existujúci GitHub workflow má zostaviť projekt a spustiť CTest pred zabalením EXE.

Po zostavení over v tvojej skutočnej knižnici: otvor problémovú mapu, skontroluj počty a varovania, Ctrl+A/C/V, vlož kópiu, ulož a znovu otvor. Potom označ viac viditeľných spawnov/zón, Delete a Undo. Pri držanom objekte over Q/E a X. Pri Fabr Tower porovnávaj rovnakú polohu a rotáciu; starý export AAA mal inú polohu než BBB.

## 6. Nahratie na GitHub

Rozbaľ ZIP a nahraj jeho obsah do koreňa existujúceho repozitára so zachovaním ciest `src/`, `tests/`, `tools/`, `docs/` a koreňového `CMakeLists.txt`. Prepíš zodpovedajúce súbory. Nenahrávaj iba samotný ZIP a nevymaž zvyšok projektu: balík obsahuje len zmenené a nové súbory. Je kumulatívny, predchádzajúci patch netreba aplikovať osobitne.

Aktívny build používa `src/`. Koreňové zrkadlové súbory dodané v pôvodnom projekte sú synchronizované tiež. Balenie ponecháva verziu 0.5.28; log opraveného zdroja obsahuje `0.5.28-selection-compatibility-v2`. Zoznam súborov a SHA-256 je v `PATCH_IMPORT_EXPORT_MANIFEST.json`. Historická analýza SWF je v `IMPORT_EXPORT_OPRAVA_SK.md`; jej pôvodné odmietanie viacerých koreňov nahrádza pravidlo V2 popísané vyššie.
