# Karty, gameplay zóny a vlastné Browse filtre

## Inštalácia

Tento patch aplikuj po ProTankiEditorPRO_SingleTabs_ObjectUI_fix.zip.
Rozbaľ súbory do koreňa repozitára a nahraď ich pri zachovaní priečinkov.
Pridaj aj nové hlavičky a test, commitni všetko vrátane CMakeLists.txt.
Ostatné súbory projektu ponechaj. Následne spusti existujúci GitHub Actions build.

## Horné karty

+ sa presúva za poslednú otvorenú kartu. Pred novým vykreslením dockovacieho
riadka sa opraví poradie; nové mapy už nepribudnú až za pôvodné +.
Zostáva jediný horný riadok a + naďalej otvára mapu.

Bodka pri názve bola indikátor neuložených zmien z ImGui. Už sa nekreslí.
X ostáva ovládaním zatvorenia karty. Interný príznak neuložených zmien aj
potvrdenie Save / Don't save / Cancel zostávajú funkčné. Samostatná voľba
NoAssumedClosure zachová kartu počas potvrdenia bez zobrazovania bodky.

## Gameplay

Kill zones a Kick zones majú vlastné checkboxy, položky vo View aj zoznamy.
Zapnutie Show gameplay overlays aktivuje spawny, vlajky, kontrolné body a
bonusové regióny; samo nezapína kill ani kick zóny. Tie zapínaš samostatne.
Vypnutie všetkých gameplay overlayov vypne aj oba typy zón.
Skratka J teraz prepína kill zóny; kick zóny majú vlastný prepínač.

Rovnaký filter sa používa pre GPU wireframe, kliknutie, hromadný výber aj živý
obrys. Skryté kick zóny sa preto neprimiešajú do výberu kill zón a naopak.
Nerozpoznané pôvodné akcie sa v XML zachovávajú. Na ich zobrazenie treba zapnúť
oba typy; prípadné pôvodné záznamy sú dostupné aj v Other special zones.

V Add gameplay elements sú Kill zone a Kick zone pri CTF vlajkách, nie na konci
bonusových nastavení. Tlačidlo bonusu sa volá Add bonus / drop region.
Pri vytváraní novej zóny sa zapne iba potrebný typ. Pri zmene jej Action
na iný typ sa zapne zodpovedajúci overlay, aby upravovaný objekt nezmizol.

## Vlastné filtre v Browse library

1. V Type vyber Create custom...
2. Prehliadač prejde na All types a otvorí knižnice. Ľavým kliknutím vyberaj
   dlaždice; zelený rámik a Selected označujú výber. Opätovný klik výber zruší.
   Počas tvorby filtra kliknutie nevloží objekt do mapy ani nezatvorí prehliadač.
3. Môžeš používať vyhľadávanie aj bežné kategórie. Už označené položky ostávajú
   vybrané aj mimo práve zobrazeného zoznamu.
4. Enter alebo Finish selection otvorí zadanie názvu. Save filter ho uloží.
   Back to selection sa vráti k označovaniu; Cancel selection tvorbu zruší.
5. Nový filter sa pridá medzi možnosti Type a hneď sa použije.

Vyberá sa konkrétna zobrazená dlaždica, vrátane jej textúrovej varianty.
Identita obsahuje knižnicu, skupinu, názov objektu a názov varianty; neukladá sa
poradie ani číselný index assetu. Zmena poradia knižnice preto filter nerozbije.
Ak knižnica práve nie je načítaná, príslušné položky sa nezobrazia a zoznam
filtra ostáva uložený. Vyhľadávanie teraz zhodne filtruje aj názvy variantov.

ĽAVÝ klik na vlastný filter ho vyberie a otvorí požadované potvrdenie vymazania.
Delete filter ho odstráni; Keep filter ho ponechá a použije. Escape tiež
ponechá filter. Klávesnicový výber filtra bez kliknutia iba zmení aktívny filter.
Zmazanie filtra nikdy nemaže objekty, textúry ani knižnice.

Filtre sa ukladajú do:
%LOCALAPPDATA%\GTanksNextEditor\browse-custom-filters.xml

Pri ďalšom spustení sa načítajú. Zápis používa dočasný súbor a nahradenie až po
úspešnom dokončení. Ak uloženie alebo vymazanie zlyhá, pôvodné filtre ostanú
nezmenené a dialóg vypíše chybu. Mená filtrov musia byť jedinečné. Uloženie
filtrov je oddelené od mapových XML a od existujúceho controls.ini.

## Overenie

- 81 Python auditov a kontrola konzistencie zdrojov: úspech.
- Natívny ImGui test: + je za všetkými mapami aj po otvorení tretej mapy
  a po zatvorení, funguje kliknutie na + aj prepínanie; dirty bodka sa nekreslí.
- Test 5001 vybraných identít: varianty, zmena veľkosti písmen, uloženie a nové
  načítanie, názov s XML znakmi, duplicity, vymazanie, poškodený vstup a chybný
  cieľ uloženia: úspech.
- Funkčný XML test: samostatný výber kill/kick, spoločný režim pre neznáme
  akcie, mazanie iba viditeľného typu a pôvodné Undo/XML kontroly: úspech.
- EditorUi.cpp a SceneRenderer.cpp prešli Windows x86-64 syntaktickou kontrolou
  v Clang s reálnymi Windows/DirectX/ImGui hlavičkami.

Celé MSVC zostavenie, linkovanie a interaktívny Windows EXE tu neboli spustené.
Tie musí potvrdiť GitHub Actions a následné spustenie aplikácie. Tento patch
neupravuje geometriu ani spôsob mapového importu/exportu.
