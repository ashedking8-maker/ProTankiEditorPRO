ProTanki Editor PRO – overená kolízia CTI

Použite nad poslednou verziou s Overlays_Branding_fix. Zachovajte cesty a nahraďte súbory v GitHub projekte. Ak bol použitý Placement_Recovery_fix, CTI teraz dostane overenú kolíziu automaticky bez potvrdenia priechodného objektu.

Zistenie
Chyba bola v predpoklade, že absencia pomocných uzlov Box/Plane/Tri znamená, že nemáme použiteľnú kolíziu objektu. Dodaný cti.3ds má jediný vizuálny mesh, ale dodaná pôvodná mapa obsahuje opakovateľnú kolíziu. Všetkých 11 výskytov CTI má zodpovedajúcu sadu 12 collision-plane + 58 collision-triangle. Vkladať CTI bez kolízie by bolo nesprávne.

Riešenie
Spoločný čítač kolízií najprv používa natívne helpery. Ak chýbajú, ponechá existujúce overené terény a skúsi zdrojovou mapou overenú geometriu. Nová CTI referencia obsahuje pôvodné kolízne primitíva, nie odhadnutý obal. Zhodu overuje podľa všetkých 160 vrcholov a 84 plôch, poradia indexov a lokálnej transformácie, nie podľa názvu súboru či objektu. Upravený alebo iný model nesmie prebrať nesprávnu kolíziu.

V pôvodnej mape 82 vizuálnych trojuholníkov pokrýva 58 kolíznych trojuholníkov a 12 obdĺžnikových plôch. Dve opačne orientované vnútorné vizuálne plochy sa do kolízie nedopĺňajú. Zachovávajú sa typy, rozmery, lokálne polohy, orientácia a poradie vrcholov pôvodných primitív. Číselné ID a poradie prvkov XML sa pri novej mape môžu líšiť; geometrická zhoda je testovaná.

Spoločná cesta sa používa pri authoringu vloženia aj opätovnom naviazaní kolízií. Vytvorené primitíva sa ukladajú do mapy a patria príslušnému objektu pri presune, kopírovaní a mazaní. Renderovanie cez GPU sa nemení.

Overenie
81 Python auditov a source preflight: PASS.
Nový C++ test: všetkých 11 pôvodných inštancií, 132 plôch + 638 trojuholníkov; naviazanie na nezávisle prevzaté pôvodné XML, vloženie, uloženie/opätovné načítanie, presun/rotácia, kopírovanie kolízií, Undo/Redo, mazanie a odmietnutie pozmeneného 3DS: PASS. Test je zaregistrovaný v CMake/CTest ako CtiOriginalMapCollision.
Windows GUI/herný beh a plný MSVC build tu neboli vykonané. Lokálny Windows syntax checker v tomto behu havaroval bez diagnostiky; nenahrádza GitHub build.

Rozsah
Oprava potvrdzuje CTI, nie automaticky všetky objekty bez helperov. Pri Envb5 porovnanie v dodanej mape nepotvrdilo zhodné trojuholníky ani plochy, preto mu nie je vymyslená fyzika. Tento patch nenahrádza všetky kolízie visual meshom a neodstraňuje ochranu podláh. Už uložené chybné mapy bez CTI kolízie sa týmto patchom automaticky neopravujú; novovložený CTI kolíziu dostane.

Zdroj cti.3ds SHA256: becaae80cd3f6dce1c926c29cd2ff37ce815962d26c820129cc98c548303de41
Zdroj map_fogtown(2).xml SHA256: cc100a0e0ebd394393ef7c86939421989a7796fe198e6464ab3dc4ef5934655d
