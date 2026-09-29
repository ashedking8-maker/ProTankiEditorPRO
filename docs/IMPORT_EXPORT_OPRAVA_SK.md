> Historický rozbor prvej opravy. Aktuálne správanie a výsledky druhej opravy sú v `OPRAVY_V2_SK.md`. Najmä pôvodné odmietanie viacerých koreňov 3DS už neplatí; nahradil ho výber prvého použiteľného koreňa s upozornením.

**Oprava kompatibility importu a exportu — ProTanki Editor PRO, 2026-09-29**

Balík obsahuje konkrétne zmeny zdrojového kódu k zaslanému projektu 0.5.28. GPU vykresľovanie zostáva zachované. Opravy sa týkajú toho, ako sa vyberie objekt z knižnice, ako sa prevedú jeho súradnice, ako vzniknú kolízie a čo sa zapíše pri exporte. Nie je potrebné vrátiť editor na technológiu Adobe AIR.

**Čo bolo skutočne analyzované**

Referenciou je priamo zaslaný `GTanksEditor(1).swf`, nie verejný mod iného editora. Komprimovaný SWF bol rozbalený a jeho ActionScript ABC bloky rozobrané vlastným čítačom bytekódu. Čítač spracoval 1 099 ABC blokov a dekódoval 16 777 tiel metód, spolu 772 323 inštrukcií, bez neznámych opcode alebo chýb v overení cieľov skokov. Tieto počty opisujú technickú kontrolu disassembléra, **nie tvrdenie, že boli ručne preskúmané všetky funkcie aplikácie**.

Ručne boli sledované relevantné cesty `PropsLibrary`, `MeshLoader`, `Parser3DS`, `LibraryManager`, `MeshProp`, `XMLImporter`, `TanksXmlExporter` a triedy kolíznych primitív. Pôvodný AIR program nebol v tomto prostredí spustený. Závery o jeho algoritme pochádzajú zo statickej analýzy bytekódu; matematické a súborové operácie boli potom overované na dostupných vzorkách.

| Otázka | Konkrétna cesta v pôvodnom SWF | Zistenie |
|---|---|---|
| Ktorý objekt z 3DS je vizuálny? | `PropsLibrary.createMeshLoaderPair`, `MeshLoader.on3DSLoadingComplete` | Výslovný `mesh/@object`; ak chýba, prvý prvok množiny koreňových detí. Názov súboru nie je pravidlom výberu. |
| Ako sa interpretujú súradnice? | `Parser3DS.buildMesh`, `setBasicObjectProperties`, `buildHierarchy` | Pri keyframe hierarchii inverzná lokálna matica 0x4160, odčítanie pivotu, transformácia keyframe a rodičovská hierarchia. |
| Čo sa stane s umiestnením modelu v pôvodnom 3DS? | `LibraryManager.createRegularProp`, `Prop` konštruktor | Súradnice vybraného vizuálneho objektu sa nastavia na nulu; objekt sa vloží do obalového prop objektu. Rotácia a mierka vizuálu ostávajú. |
| Odkiaľ pochádzajú kolízie? | `MeshProp.parseCollisionData`, `CollisionPrimitivesCache` | Z priamych pomocných detí vybraného vizuálneho objektu, nie zo všetkých meshov v súbore. |
| Ako vzniknú rozmery kolízneho boxu? | `CollisionBox.parse` | Minimum a maximum lokálnych vrcholov; následne rotácia a posun helpera. Mierka helpera sa tu osobitne nenásobí. |
| Ako vznikne rovina a trojuholník? | `CollisionRect.parse`, `CollisionTriangle.parse` | Prvá plocha poskytne bázu. Rovina používa hrany pri najdlhšej diagonále; trojuholník ťažisko a lokálne vrcholy. |
| Ako sa zapisujú kolízie? | `CollisionRect.getXml`, `CollisionBox.getXml`, `CollisionTriangle.getXml` | Transformácia kolízie sa kombinuje s transformáciou prop objektu; rotácie X/Y/Z sa zapisujú so šiestimi desatinnými miestami, polohy a rozmery s tromi. |
| Čo robí Light import mapy? | `XMLImporter`, `TanksXmlExporter` | Rekonštruuje objekty z knižnice. Samostatnú sekciu collision-geometry nepreberá ako editovateľnú scénu; pri exporte kolízie generuje z knižničných helperov. |

SHA-256 referenčného SWF: `045c9d239364a7fdce31a177ef2ac371ffd3fc100cd1bee748437fe5d2af5226`.
SHA-256 vstupného projektového ZIP-u: `d5de22c202b5d8b19132da62cb64909b9a2ddff4fb737752bbe86061e5918167`.

**1. Jeden výber vizuálneho modelu pre celý editor**

Doteraz vizuálny importér a kolízny importér vyberali vizuál rozdielnymi heuristikami. Jeden zohľadňoval meno súboru a materiály; druhý mal aj výnimky pre Container a tunely. Tak mohli z toho istého 3DS vybrať odlišný základ.

Nový `src/Native3DSScene.h` číta skutočné objekty a keyframe hierarchiu. `AssetRegistry` zachováva atribút `object` z `<mesh file="..." object="...">`. Rovnaký výber používa vizuálny import, čítač kolízií, náhľady, Object Editor aj export vlastného objektu. GPU cache a zoskupovanie meshov obsahujú aj názov vybraného objektu, takže dve definície odkazujúce na odlišné objekty v jednom 3DS nezdieľajú nesprávny model.

Ak `object` chýba, prijme sa jediný koreňový mesh. Pri viacerých koreňoch sa načítanie odmietne s konkrétnou správou, aby bolo možné doplniť výslovný výber v `library.xml`. Toto je vedomá odlišnosť: pôvodný `Set.peek()` závisí od poradia enumerácie ActionScript množiny a bez spustenia AIR neviem pre ľubovoľný súbor zaručiť rovnaký prvok. Výber náhodného veľkého meshu by kompatibilitu negarantoval.

**2. Pivoty, matice a pôvodná kolízna geometria**

Spoločný čítač používa plnú inverznú maticu, nie iba odčítanie posunu ani predpoklad jednotkových kolmých osí. Pri keyframe objekte nasleduje odčítanie pivotu. Vizuál ďalej používa vlastnú mierku a rotáciu; pôvodný absolútny posun vizuálneho koreňa sa pri knižničnom prop objekte zahodí rovnako ako v origináli. Konverzia do GPU súradníc `(x,z,y)` zostáva oddelená od natívnych Z-up dát.

Assimp naďalej zabezpečuje čítanie materiálov, UV a normál. Jeho vrcholy sa priraďujú k pôvodným vrcholom, aby vizuál používal súradnice podľa natívneho parsera. Ak sa zodpovedajúci priestor nedá potvrdiť, import sa zastaví s chybou. Túto integračnú časť musí ešte overiť Windows test s reálnym Assimpom.

Kolízny čítač prechádza iba priamych potomkov vybraného objektu a rozlišuje prefixy plane/box/tri. Vnorené vnúčatá, susedné korene a occlusion pomocné objekty sa nestávajú kolíziami. Namiesto výnimiek pre `contain`, `tunnel_1` a `tunnel_2` funguje všeobecné pravidlo hierarchie. Test porovnáva výsledok aj po premenovaní všetkých troch súborov.

Konkrétny rozdiel sa ukázal pri `combuild_comb3.3ds`: pôvodný algoritmus dáva lokálne rozmery boxu `500 × 30 × 150`; predchádzajúci importér nového editora očakával `500 × 60 × 150`. Rozdiel vznikal samostatným započítaním mierky helpera. Regresný test bol zmenený podľa operácií v `CollisionBox.parse`, nie iba preto, aby prešiel nový kód.

**3. Opravený XML zápis naklonených kolízií**

V `MapDocument.cpp` bola jednoznačná chyba: pri vytváraní novej kolízie sa X a Y rotácia zapisovali iba vtedy, ak príslušné XML elementy už existovali. Nový uzol ich však nemal. V pamäti teda mohla byť naklonená rovina alebo trojuholník, ale uložená mapa obsahovala iba rotáciu Z.

Zapisovač teraz pri novo vytvorených rovinách, boxoch aj trojuholníkoch vytvorí všetky tri zložky rotácie. Pri bezstratovej kópii pôvodného XML zachováva jeho dodatočné polia a pôvodnú prítomnosť nulových X/Y zložiek. Pridaný Windows regresný test kontroluje nenulové X/Y, opätovné načítanie, väzbu na vlastníka a ďalšie uloženie.

**4. Kopírovanie časti mapy zachová skutočné kolízie**

Úplná mapa sa už kopírovala so svojím XML kolíznym balíkom. Čiastočný výber však prechádzal opätovným generovaním z knižnice. To mohlo zmeniť kolízie, ktoré boli v zdrojovej mape upravené alebo sa líšili od odhadovaného modelu.

Nová metóda `CopyNativeCollisionForProps` skopíruje existujúce roviny, boxy a trojuholníky aj pre podmnožinu objektov, ak majú overených vlastníkov. Indexy vlastníkov sa premapujú do clipboardu a pri vložení do cieľovej mapy. Uchová sa aj pôvodný XML podstrom každého primitívu. Pri kopírovaní celej mapy sa naďalej prenesú aj nepriradené kolízie.

Skupinu, ktorej každá kolízia má vlastníka, možno následne upravovať a mazať po jednotlivých objektoch. Ak kopírovaná skupina obsahuje nepriradené kolízie, samostatný presun sa blokuje, aby fyzická geometria nezostala na pôvodnom mieste. Celú takúto zostavu možno znovu vložiť cez úplný bezstratový clipboard. Nejde o implementáciu ľubovoľného skupinového presunu neznámych kolízií.

Ak dostupná knižnica obsahuje rozpoznané pomocné kolízie, ale tie sa nedajú spoľahlivo spárovať s XML mapy, editor označí vlastníctvo za nevyriešené. Presun, čiastočné kopírovanie a čiastočné mazanie dotknutého objektu sa zastavia s vysvetlením. Samotné zachovanie a uloženie pôvodných dát je stále možné. Tento mechanizmus nenahrádza chýbajúcu knižnicu ani nevytvára univerzálne vlastnícke väzby pre neznáme mapy.

**5. Export vlastného 3DS má správnu hierarchiu**

`Native3DSWriter` doteraz zapisoval všetky keyframe uzly ako korene s rodičom 65535. Originál však kolízie hľadá medzi deťmi vizuálneho meshu. Súbor preto mohol obsahovať správne pomenované boxy a trojuholníky, ktoré originál vôbec nepoužil ako kolízie objektu.

Nový zapisovač ponechá vizuál ako koreň a pomocné kolízne uzly pripojí k uzlu 0. Exportované `library.xml` obsahuje výslovné `object="ptpro_mesh"`.

Staršie vygenerované PTPRO súbory možno načítať cez úzku migráciu: koreň musí byť `ptpro_mesh`, pomocné názvy musia zodpovedať známym generovaným vzorom, všetky uzly musia byť jednoznačné a matice, pivoty a transformácie musia zodpovedať pôvodnému jednotkovému generovanému rozloženiu. Migrácia nemení vstupný súbor. Bežné pôvodné knižnice tento fallback nepoužívajú.

Materiálové a vyhladzovacie informácie sa teraz priraďujú v rovnakom normalizovanom priestore ako vizuál. Úzko zameraný export terénu zostáva obmedzený na už podporovaný postup úpravy jedného vrcholu; nepodporované keyframe transformácie odmietne.

**Dôležitá hranica medzi kompatibilitou a zachovaním mapy**

Nový editor naďalej uchováva existujúce kolízie a neznáme XML údaje. Pôvodný Light editor ich pri exporte rekonštruuje z knižnice. Neprepol som celý nový editor na automatické zahodenie všetkých existujúcich kolízií: pri vlastných mapách by to mohlo zničiť zámerné úpravy. Pôvodné pravidlá teraz slúžia na interpretáciu modelov a vznik nových kolízií; existujúce overené dáta sa kopírujú bez regenerácie.

Overené Fogtown podlahové šablóny v projekte zostávajú zachované. Vo sledovanej ceste originálneho SWF som nepotvrdil univerzálny generátor podlahy pre helperless modely ani konkrétnu výnimku Fogtown. Preto túto existujúcu funkciu neopisujem ako prenesený algoritmus pôvodného editora.

**Výsledky overenia a zostávajúce limity**

| Overenie | Stav |
|---|---|
| Dekódovanie ABC blokov SWF a kontrola inštrukcií/skokov | Úspešné; ide o statickú analýzu. |
| Existujúce Python kontroly projektu | 77/77 prešlo. Štyri kontroly starých textových heuristík boli aktualizované; geometrické testy zostali zachované. |
| `tools/check_source_consistency.py` | Prešlo. |
| Samostatný C++ test 3DS zapisovača | Prešlo. |
| Samostatný C++ test materiálov a smoothing metadát | Prešlo. |
| Samostatný C++ test terénu 136 → 138 trojuholníkov a TARA roundtrip | Prešlo; geometria sa porovnáva s priloženou herne overenou referenčnou vzorkou. |
| Nový samostatný C++ test SWF kompatibility | Prešlo: matice, pivot, angle-axis, výber uzla, rodičovstvo, cyklus, premenovanie, lokálne rozmery a migrácia. |
| Prechod čítača cez všetkých 30 3DS súborov projektu | 21 modelov s kolíziami, 9 bez helperov; žiadna chyba parsovania alebo výberu. Nejde o vizuálne overenie všetkých modelov. |
| Nový XML test rotácií, kopírovania a vlastníctva | Pridaný do CMake/CTest; lokálne nevykonaný. |
| Aktualizovaný test čítania `mesh/@object` cez AssetRegistry | Pridaná kontrola v existujúcom Windows teste; lokálne nevykonaná. |
| Kompletný MSVC build, reálny Assimp, GPU náhľad, AIR a hra | Tu nevykonané. Existujúce GitHub workflow už zostavujú projekt a spúšťajú CTest pred balením. |

Štyri spustené C++ testy používajú skutočné hlavičky parsera a zapisovača. Na Linuxe je nahradený len dátový typ `DirectX::XMFLOAT3` tromi floatmi; nie matematika, čítač súborov ani testované algoritmy. Postup je reprodukovateľný skriptom `tools/run_native_cpu_tests.py`.

Patch nie je všeobecným prepisom všetkých formátov originálu. Nový editor naďalej pracuje s plochým XML 1.0/1.0.Light. XML 2.0/3.0 teraz výslovne odmietne, aby nepôsobilo úspešne načítané s prázdnou alebo neúplnou scénou; konverziu možno vykonať originálnym editorom. Viacnásobné 3DS inštancie s nejednoznačnými názvami, ľubovoľné animácie, všetky materiálové režimy originálu a všeobecný GLB → natívny export nie sú týmto patchom vyriešené. Rotácia naviazaných prop objektov pri editovaní ostáva obmedzená na os Z; lokálne X/Y rotácie kolízií sa však správne zachovávajú a zapisujú.

Po zostavení je praktická akceptačná skúška otvoriť rovnakú mapu, vložiť Container a oba tunely, skopírovať naklonený objekt aj menšiu skupinu, uložiť, znova načítať a porovnať kolízny náhľad aj správanie v hre. Až takáto skúška s tvojou kompletnou knižnicou môže potvrdiť výslednú kompatibilitu konkrétnych máp.

**Použitie balíka**

Rozbaľ ZIP do existujúceho projektu a nahraj všetky jeho súbory na rovnaké cesty. Dôležité sú aj `CMakeLists.txt`, nové hlavičky v `src/`, zmenené testy a kontrolný skript — nestačí vymeniť iba dva importéry. Koreňové duplikáty zmenených C++ súborov sú zosúladené s `src/`; CMake používa `src/`. Presný zoznam a kontrolné súčty sú v `PATCH_IMPORT_EXPORT_MANIFEST.json`.
