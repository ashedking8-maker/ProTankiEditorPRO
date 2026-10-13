# Obj. editor: audit a opravy – 13. 10. 2026

## Základ a stav

Skontrolovaný bol posledný dostupný projekt ProTankiEditorPRO-main(2).zip
s naším patchom Fogtown_surface_fix. Nebol pripojený aktuálny GitHub repozitár
ani novšie .exe. Závery vychádzajú zo zdrojov, dodaných dát a CPU regresií;
nejde o vizuálny audit spustenej Windows aplikácie.

Tento ZIP je kumulatívny: obsahuje aj predchádzajúcu ochranu vkladania a opravu
svahov Fogtown. Starší patch potom znovu nenahrávaj, prepísal by nové zmeny.

## Je v editore systém, ktorý zabráni všetkým chybám?

Nie. Existuje súbor konkrétnych kontrol, nie systém, ktorý automaticky pozná
herné vlastnosti všetkých objektov. Priechodnosť sama o sebe nie je vždy chyba:
dekorácia môže byť zámerne priechodná. Z vizuálneho modelu nemožno spoľahlivo
odhadnúť všetky gameplay vlastnosti alebo správny efekt povrchu.

| Oblasť | Aktuálna ochrana | Hranica |
|---|---|---|
| Nové vloženie 3D objektu | Bez úspešne vytvorenej kolízie sa štandardne odmietne; skupina sa vráti celá | Výslovný visual-only režim povoľuje priechodný objekt na jedno vloženie |
| Fogtown land 1 a land 2 | Kontrola geometrie proti pôvodnej mape, oprava chýbajúcich plôch, kontrola duplicít a smeru | Nie všeobecná certifikácia celej Fogtown knižnice |
| Pôvodné XML | Projekt má zachovanie pôvodných podstromov a kontrolované klonovanie | Zachovaním starej mapy možno zachovať aj jej už existujúcu chybu |
| Neznáme gameplay údaje pri native exporte objektu | Nepodporované prop/mesh polia sa odmietajú; doplnená kontrola texture metadata | Nie sú automaticky prevedené do nového formátu |
| Kolízie vs. efekty povrchu | Sú to odlišné údaje; kolízna geometria sa overuje samostatne | Správny collider sám nepotvrdzuje sneh/prach, trenie ani trigger správanie |
| Obj. editor návrh | Obsahuje editovateľnú geometriu, boxy, účel a source XML sidecary | Uložený draft nie je herný asset |
| Obj. editor native export | Obmedzené podporované 3DS cesty a opätovné načítanie výstupu | Nie všeobecný prevod GLB, triggerov, plane/mixed geometrie ani ľubovoľných mesh úprav |

## Ako Obj. editor funguje

Má samostatný viewport a Undo/Redo oddelené od mapy. Zdrojový GLB/3DS sa načíta
do editovateľného návrhu. Dá sa meniť geometria, materiálové nastavenia, boxy a
pripojiť referenčné library XML. Save vytvára izolovaný adresár návrhu.
Experimentálny Export vytvára nový PTPRO_* adresár a TARA; pôvodnú knižnicu
neprepisuje. Native export kontroluje zdroj, podporované údaje, textúry,
geometriu a výsledné 3DS. Výber účelu „Driveable“ nie je nastavenie snehu/piesku.

## Konkrétne opravené problémy

1. **Prenášanie nastavení medzi modelmi.** Otvorenie iného 3DS/GLB alebo výber
   iného modelu knižnice nahrádzal iba časť dokumentu. Mohli zostať boxy, účel,
   tint, materiálové override či stará XML referencia. Nový zdroj teraz dostane
   nový dokument; pri výbere z knižnice dostane referenciu práve tohto modelu.
2. **Strata rozpracovaného objektu.** Otvorenie iného zdroja zrušilo históriu bez
   otázky. Pri neuložených zmenách sa ponúkne uloženie, pokračovanie bez uloženia
   alebo zrušenie. Neúspešné načítanie alebo zrušenie výberu súboru ponechá starý
   dokument. Pri zvolení uloženia sa pokračuje iba po jeho úspechu.
3. **Undo/Redo po uložení.** Obnovenie staršieho stavu neoznačilo draft ako zmenený,
   a zatvorenie preto nemuselo upozorniť. Undo/Redo teraz označí zmenu. Ide o
   konzervatívny príznak: aj návrat na obsah zhodný s uloženým môže žiadať uloženie.
4. **Posun boxu myšou.** Shift+ľavý ťah menil box bez Undo a bez dirty flagu.
   Teraz zachytí jednu snímku na celý ťah, kontroluje platné súradnice a ide vrátiť.
   Shift+klik už nezačne súčasne ťahať bod siete namiesto vybraného boxu.
5. **Export označoval draft ako uložený.** Native balík neobsahuje plný editovateľný
   draft. Úspešný native export preto už nezruší príznak neuložených úprav.
6. **Tiché zahodenie editovanej siete.** Pri nesúlade uložených vrcholov/plôch so
   zdrojom sa edit layer zahadzoval a ukázala sa pôvodná sieť. Teraz ostanú dáta
   návrhu zachované, preview/native export sa zablokuje a zobrazí sa dôvod.
7. **Fogtown v Obj. editore.** Mapový patch predtým nebol zapojený do tejto exportnej
   cesty. Helperless svah sa tu mohol exportovať s draft boxom. Teraz sa pri
   nezmenenej certifikovanej geometrii zapíše skutočných 18/16 trojuholníkov.
   Zmena polohy vrcholov, mierky alebo topológie takého fyzického povrchu sa
   odmietne; návrh sa naďalej dá uložiť. Nie je to podpora ľubovoľného modelovania
   terénu s automaticky správnou fyzikou. Explicitná dekorácia zostáva zámerne
   bez kolízie.
8. **Strata rozšírených údajov textúry.** Native exporter zapisuje name/diffuse-map;
   teraz odmietne ďalšie texture atribúty alebo vnorené elementy, ktoré by inak
   vynechal, napríklad alpha-map. Nepredstiera ich podporu.

## Zjednodušenie používania

- Pri názve pracovného priestoru je viditeľný stav „unsaved changes“.
- Zmena názvu objektu je tiež zahrnutá do Undo.
- Bod siete jasne uvádza Y ako výšku; kolízne boxy uvádzajú Z ako výšku.
  Vnútorné osi zostali kompatibilné s existujúcimi návrhmi.
- Nápoveda viewportu uvádza Shift+ťah boxu a Ctrl+Z.
- Pri overenom Fogtown povrchu je zobrazený počet kolíznych trojuholníkov a hranica
  podporovaných úprav.
- Neúspešný Save alebo Export už automaticky nezatvorí potvrdzovacie okno.
- Popisy rozlišujú uloženie editovateľného návrhu a vytvorenie hernej knižnice.

## Overenie a čo ešte overiť na Windows

Prešlo 10 samostatných CPU C++ regresných programov a kontrola štruktúry zdrojov.
Nový test overuje nový čistý dokument, zachovanie geometrie Fogtown pri exporte
3DS a opätovnom importe, vrcholy aj normály a odmietnutie zmenenej geometrie/
topológie. Existujúce testy pokrývajú Fogtown reference, 3DS, snapovanie a ďalšie
časti. Do existujúceho Windows integration testu pribudol celý NativeObjectExport
s Fogtown, odmietnutie škálovania a nepodporovaných texture údajov.

Windows/MSVC build, Assimp/XML integration testy, interakcie ImGui a hra tu
neboli spustené. V zostavení na Windows over:
- uprav box -> otvor iný model -> Cancel zachová starý; pokračovanie vytvorí čistý;
- ulož -> Undo -> zatvor: musí sa zobraziť upozornenie na zmeny;
- Shift+ťah boxu -> Undo: jeden krok vráti celý ťah, nie časť;
- otvor uložený návrh s nezhodným source mesh: nesmie zahodiť edit layer;
- exportuj nezmenený Fogtown land 1 -> v mape G ukáže 18 trojuholníkov;
- zmeň vrchol alebo mierku Fogtown a skús fyzický export: musí sa odmietnuť;
- vykonaj herný test. Úspech CPU testov nie je potvrdenie správania ProTLVK.

## Nasadenie

Skopíruj obsah ZIP-u do koreňa projektu so zachovaním priečinkov. Priložené
zdrojové súbory sú úplné náhrady; pri vlastných novších úpravách ich zlúč.
EditorUi.h zachováva default pozadia 180/180/180. ZIP neobsahuje nové .exe.
Priečinok repaired-maps obsahuje predchádzajúcu opravenú testovaciu mapu Fogtown,
ktorá neslúži na kompiláciu. Manifest obsahuje kontrolné súčty oproti poslednému
zaslanému projektu aj predchádzajúcemu Fogtown patchu.
