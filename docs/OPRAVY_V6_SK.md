# V6 — Grid snap pri presúvaní a nastaviteľná vodiaca čiara

Kumulatívny balík V1–V6 pre zaslaný ProTankiEditorPRO-main 0.5.28. Označenie nového logu je `0.5.28-existing-snap-v6`.

## Presúvanie existujúcich objektov

Diskrétne dodatočné polohy pri hranách z V5 sa teraz používajú aj pri ťahaní existujúceho objektu/skupiny myšou a pri horizontálnom presune klávesmi. Bežná mriežka zostáva základom, nepribudol ďalší prepínač.

Pri ťahaní sa kandidát počíta z polohy pri začatí ťahania, nie z výsledku predošlého snímku. Celá skupina dostane rovnaký posun a zachová rozostupy. Presúvané objekty sú vylúčené zo zoznamu cieľov, takže sa nezarovnávajú samy na seba ani na ostatných členov výberu.

Klávesový posun môže zastaviť na dodatočnej polohe pred ďalším uzlom mriežky. Nasledujúce stlačenie pokračuje ďalej; aktuálna hrana nesmie objekt zadržať ani pritiahnuť dozadu. Pri skupine sa mriežka počíta zo spoločného aktívneho bodu a výsledná translácia sa aplikuje na všetkých členov. Rotácia a vertikálny pohyb zostávajú existujúcimi operáciami; nové hranové prisúvanie rieši horizontálny posun.

Zmeny polohy používajú existujúcu cestu aktualizácie mapy a kolízií. Ťahanie zostáva jednou Undo operáciou pri pustení tlačidla; klávesový krok používa spoločný záznam skupiny. Funkcia nevytvára dočasné duplikáty objektov v dokumente.

## Vodiaca čiara

Text pri vodiacej čiare bol odstránený. Zostáva iba tenká čiara s hrúbkou 1 pixel. Jej farba sa nastavuje v **Tools → Placement settings → Snap guide color**. Zmena sa ukladá rovnakým mechanizmom ako ostatné nastavenia editora a načíta sa po reštarte. Predvolená farba nadväzuje na zelenú/tyrkysovú z V5.

Čiara funguje pri ghoste aj pri presúvaní existujúcich objektov. Pri klávesovom kroku sa krátko zobrazí (0,35 s), aby bolo zarovnanie viditeľné. Vykresľuje sa po aktualizácii pohybu v UI, bez textového štítku.

## Overenie a hranice

Lokálne prešlo 79 Python kontrol, kontrola konzistencie zdrojov a sedem C++ regresných programov. Nové matematické prípady overujú zastavenie klávesového posunu na doplnkovej polohe, pokračovanie ďalším stlačením a odmietnutie spätného zachytenia. Zostávajú testy diskrétneho pohybu bez kĺzania, kopírovaných skupín, rotácií a predchádzajúcich opráv.

Kompletný Windows/MSVC/Direct3D build, ovládanie GUI a opätovné načítanie farby v spustenej EXE tu neboli vykonané. GitHub workflow musí vykonať build a CTest. Po zostavení over: presuň existujúci objekt myšou k hrane, zopakuj to so skupinou a klávesmi, použi Undo, zmeň farbu čiary a reštartuj editor.

Geometrické hranice V5 ostávajú: používa sa vonkajší konvexný obrys, pri sprites konzervatívne rozmery obrázka; neznáma geometria používa iba bežnú mriežku. Nie je to presné skladanie vnútorných výrezov konkávnych modelov ani úplná kontrola všetkých prienikov.

## Nahratie

Rozbaľ ZIP a nahraj celý obsah do koreňa existujúceho GitHub projektu so zachovaním `src/`, `tests/`, `tools/`, `docs/`. Prepíš rovnomenné súbory, ostatné ponechaj. Balík je kumulatívny a obsahuje aj predchádzajúce opravy. Hotová EXE nie je súčasťou balíka.
