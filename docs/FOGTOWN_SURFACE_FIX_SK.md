# Fogtown – oprava chýbajúcich kolízií svahov

Patch pripravený 6. 10. 2026 pre posledný zaslaný projekt ProTankiEditorPRO-main(2).zip
(light-clipboard-v8). Zahŕňa aj predchádzajúcu ochranu collision_guard; netreba
aplikovať oba ZIP-y za sebou. Tento patch nahraj ako posledný.

## Čo sme potvrdili

Fogtown/land01.3ds má 16 vrcholov a 18 viditeľných trojuholníkov. land02.3ds má
15 vrcholov a 16 trojuholníkov. Modely neobsahujú pomocné kolízne objekty.
Dodaný pôvodný map_fogtown(1).xml však obsahuje samostatné kolízie:

| Model | Výskytov v pôvodnej mape | Trojuholníkov na výskyt | Overených plôch |
|---|---:|---:|---:|
| land 1 | 11 | 18 | 198 |
| land 2 | 2 | 16 | 32 |

Všetkých 230 kolíznych plôch sa zhoduje s vizuálnymi plochami po transformácii
do sveta vrátane orientácie normál. Najväčší nameraný rozdiel rohov pri porovnaní
source XML a zdrojového modelu bol približne 0,001405 mapovej jednotky.

TEST_MAP_BBB(2).xml obsahoval 6 objektov a 200 kolíznych trojuholníkov. Oba
Fogtown land 1 boli bez svojich 18-plochových kolízií. Chyba vznikla už pri vložení:
log uvádza native=0, visualOnly=1. Starý kód automaticky dovolil vloženie 3DS
bez helperov; kontrola plochých podláh nepokrývala tento svah. Zobrazenie cez G
správne ukazovalo, že táto fyzická geometria v mape chýba.

Nový Fogtown(1).tara má rovnaké modely ako predchádzajúci balík. Rozdiel je len
v library.xml: druhý duplicitný názov textúry grf5 bol opravený na grf6. To nie je
príčina chýbajúcich kolízií svahov. Knižnicu tento patch neprepisuje.

## Riešenie v editore

1. Native helpery majú naďalej prednosť. Keď chýbajú, nový resolver skontroluje,
   či lokálne vrcholy a topológia zodpovedajú geometrii overenej pôvodnou mapou.
   Názov súboru ani rozmery bounding boxu nie sú dôkazom správnej kolízie.
2. Pre overený povrch vytvorí natívne trojuholníky rovnakým spôsobom ako existujúci
   triangle exporter: lokálne vrcholy, stred a rotácia. Transformácia objektu
   presunie aj fyzický povrch. Nie je to plný box pod svahom ani odhad jeho výšky.
3. Nové vloženie a bežné vloženie kopírovaného objektu používajú tento resolver.
   Pri načítaní sa existujúce kompletné kolízie priradia objektu porovnaním
   skutočných svetových rohov a smeru plôch. Umožňuje to ich spoločný pohyb.
4. Pri Save/Save As sa overené svahy skontrolujú a doplnia sa len chýbajúce
   trojuholníky. Existujúce presné plochy sa použijú bez duplikovania a bez
   prepisovania ich XML. Oprava funguje aj pre čiastočne chýbajúci súbor plôch.
5. V Properties pri podporovanom svahu pribudlo tlačidlo
   "Verify / repair original terrain collision". Opraví ho pred uložením,
   takže ho možno hneď skontrolovať cez G. Oprava má Undo/Redo snapshot.
6. Pri obrátenej ploche, duplicitnej geometrii, konflikte vlastníka či zhodných
   prekrývajúcich sa inštanciách sa automatická oprava odmietne. Save pri chybe
   vráti rozpracované opravy mapy späť. Žiadne časti neúspešnej opravy neuloží.

Výpočet používa spoločné pravidlá pre oba svahy a kontrolu skutočnej geometrie,
nie dve ručne odhadnuté obálky. Zdrojové modely sa počas preflightu čítajú len
raz na kombináciu súboru a meshObject; kandidáti na zhodné trojuholníky sa hľadajú
cez priestorové bunky. Toto nepredstavuje benchmark celej aplikácie na veľkej mape.

## Rozsah a obmedzenia

Overený katalóg povrchov v tomto patchi obsahuje land 1 a land 2. Automatické
vytváranie fyziky zo všetkých ľubovoľných 3DS zostáva zakázané: stromy, dekorácie,
strechy a iné objekty môžu mať zámerne odlišnú kolíziu. Iné modely bez overeného
zdroja musia byť výslovne vložené ako visual-only alebo vyžadujú ďalší podklad.
Pôvodných šesť overených plochých Fogtown dlaždíc používa existujúcu opravu.

Zmenený model, iná topológia alebo nepodporovaný náklon sa nesmie vydávať za
pôvodne overený povrch. Editor pri oprave potrebuje dostupnú správnu knižnicu.
Táto oprava netvrdí, že odhalí všetky možné chyby všetkých knižníc.

Zhoda znamená rovnakú fyzickú geometriu, nie bajtovo rovnaké XML. Nové ID,
poradie plôch a ekvivalentná reprezentácia rotácie sa môžu líšiť. Export je
naďalej v existujúcom natívnom formáte; nepoužíva renderovaciu GPU sieť ako
neoverenú náhradu kolíznych dát.

## Overenie

- Prešlo 9 CPU C++ regresných programov vrátane všetkých 13 pôvodných inštancií,
  230 plôch a správneho smeru normál.
- Testuje sa chýbajúca aj čiastočná kolízia, opakovanie opravy, duplicity,
  opačné normály, iný vlastník a odmietnutie upravenej vizuálnej geometrie.
- Prešla kontrola štruktúry zdrojov a zhodnosti koreňových/src kópií.
- Opravené XML sa znova načítalo a geometricky overilo: 6 nezmenených objektov,
  200 zachovaných pôvodných plôch, 36 nových plôch, spolu 236 unikátnych ID.
- Do CMake/CTest je doplnený Windows test dokumentu: oprava, opakovanie,
  Undo/Redo, save/reload, bind, pohyb, kopírovanie a mazanie kolízií.
- Celý Windows/MSVC build, Windows test dokumentu, UI a jazda v ProTLVK
  tu NEBOLI spustené. Tie zostávajú na GitHub/Windows zostavení a hernom teste.

## Nasadenie a použitie

Nahraj zdrojové súbory zo ZIP-u do koreňa existujúceho GitHub projektu so
zachovaním priečinkov. Ide o patch, nie kompletný projekt. Ak si odvtedy menil
rovnaké zdrojáky, zmeny zlúč; nevymieňaj svoj novší kód naslepo. Súbor
PATCH_FOGTOWN_SURFACE_MANIFEST.json obsahuje kontrolné súčty východiskových
aj opravených súborov. EditorUi.h sa neprepisuje, takže predchádzajúca zmena
pozadia 180/180/180 zostáva zachovaná.

Po zostavení otvor testovaciu mapu a pripoj Fogtown knižnicu. Pri existujúcom
svahu použi tlačidlo opravy v Properties alebo mapu ulož. G potom musí zobraziť
aj nové trojuholníky svahu. Nové land 1/land 2 ich dostanú už pri vložení.

V repaired-maps/TEST_MAP_BBB_repaired.xml je samostatná opravená kópia tvojej
poslednej testovacej mapy. Tá je na otestovanie v hre, nie na kompiláciu .exe.
Pôvodný zaslaný súbor zostal nezmenený.
