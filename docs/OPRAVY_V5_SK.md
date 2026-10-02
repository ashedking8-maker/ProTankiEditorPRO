> V6 rozširuje presúvanie existujúcich objektov a upravuje vodiacu čiaru; pozri `OPRAVY_V6_SK.md`.

# V5 — jediný Grid snap s dodatočnými polohami

Kumulatívny patch k zaslanému ProTankiEditorPRO-main 0.5.28. Nový log: `0.5.28-discrete-grid-v5`. Priložený používateľský log potvrdzuje spustenú V4.

## Čo spôsobovalo kĺzanie

V4 vypočítavala Total snap z nezaokrúhlenej polohy kurzora. Dorovnala vzdialenosť od hrany, ale pozdĺž nej ponechala plynulú súradnicu. Preto sa ghost mohol posúvať po jednotkách aj pri kroku 500. Ide o konkrétne správanie kódu, nie o zlyhanie načítania knižnice.

## Nové pravidlo

V Tools aj v Placement settings je jediný predvolene zapnutý **Grid snap**. Total grid snap aj samostatný Coordinate grid boli odstránené z ovládania.

1. Z kurzora vznikne bežná poloha na zvolenej mriežke, napríklad 400 alebo 500.
2. Z tejto polohy a hrán okolitých objektov sa vytvoria dodatočné diskrétne polohy. Poloha pri hrane môže byť aj medzi uzlami mriežky.
3. Kurzor iba vyberie najbližšiu z bežnej a dodatočných polôh. Nedodáva nezaokrúhlenú súradnicu výslednej polohy pozdĺž hrany.
4. Pri zhode vzdialeností má prednosť bežná mriežka. Žiadny výsledok sa nepripočítava k výsledku predchádzajúceho snímku.

Pri kroku 100 tak môžeš prejsť medzi 400, dodatočnou polohou pri hrane 444,57 a 500. Počas pohybu pozdĺž rovnej hrany zostáva druhá súradnica na bežných krokoch. Pri otočenej hrane sa dodatočná poloha vypočíta z mriežkového uzla a geometrie; aj ona je pre daný uzol pevná.

Poloha pivotu závisí od rozmerov/rotácie modelu a nemusí byť číselne zhodná s polohou hrany. Zostáva medzera `Edge clearance`, predvolene 0,02 jednotky, aby styčné plochy neležali cez seba. V teste objekt s ľavou hranou v pivote preto dosiahne 444,59 pri referenčnej hrane 444,57.

Vypnutie Grid snap vypne mriežkové zarovnávanie aj dodatočné polohy. Shift naďalej znamená desatinu zvoleného kroku. Q/E a zachovanie výšky pri prepínaní modelu zostávajú.

## Nové objekty aj schránka

Rovnaká vetva sa používa pri novom modeli, kopírovanom jednotlivom objekte aj kopírovanej skupine. Odstránená je podmienka zakazujúca edge snap pre schránku a viac objektov.

Pre skupinu sa vytvorí spoločný vonkajší obrys. Prisunutie mení celý ghost rovnakým posunom a aktualizuje aj jeho spoločný pivot. Relatívne polohy a rotácie jednotlivých objektov sa nemenia. Existujúci natívny clipboard export používa tento pivot aj na transformáciu prenesených kolízií. Vyhľadávajú sa okolité mesh objekty mapy, nie iba posledný vložený objekt.

Do obrysu skupiny sa zahrnú aj sprites pomocou konzervatívnych rozmerov dekódovaného obrázka, rovnako ako pri ich výberových hraniciach. Ak chýba geometria niektorého objektu, obyčajná mriežka zostáva funkčná, ale skupine sa nevymýšľa neznáma hrana.

Toto stále nie je presné skladanie vnútorných výrezov konkávnych L/U modelov. Používa sa vonkajší konvexný obrys; pri skupine spoločný obrys celej skupiny, nie každá vnútorná medzera. Doplnkové hrany poskytujú mesh objekty, nie gameplay markery. Funkcia nekontroluje všetky možné prieniky celej skupiny so všetkými ďalšími objektmi.

## Overenie

Prešlo 79 Python kontrol, zdrojová kontrola a sedem C++ regresných programov. Nové prípady v `placement_snap_regression.cpp` volajú priamo živý V5 algoritmus `FindDiscrete` a overujú:

- samostatnú dostupnosť uzlov 400, hrany 444,57 s medzerou a uzla 500;
- 481 jemných posunov kurzora pozdĺž hrany pri kroku 500 bez plynulého posúvania ghostu;
- následný skok druhej súradnice o 500;
- spoločný obrys a zachovanie rozostupov kopírovanej skupiny;
- pevný dodatočný bod pri šikmej hrane v rámci jedného mriežkového uzla.

Kompletný Windows/MSVC/Direct3D build ani klikací priebeh v EXE tu neboli spustené. GitHub workflow musí vykonať zostavenie a CTest. Po zostavení over kroky 100/500, priblíženie k hrane zo všetkých strán, pohyb pozdĺž hrany, Ctrl+C/V jednotlivého objektu aj skupiny a uloženie/opätovné načítanie.

## Použitie

Rozbaľ ZIP a nahraj celý obsah do koreňa existujúceho repozitára so zachovaním `src/`, `tests/`, `tools/`, `docs/`. Prepíš rovnomenné súbory, ostatné ponechaj. Balík zahŕňa V1–V5, netreba aplikovať staršie ZIP-y. Zachované je aj pravé tlačidlo na pridanie do AX, zelené potvrdenie a predchádzajúce opravy gameplay/importu/exportu.
