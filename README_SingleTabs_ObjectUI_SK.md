# Oprava horných kariet a sprehľadnenie Object Editora

Tento ZIP aplikuj NA POSLEDNÝ ProTankiEditorPRO_GLB_MapTabs_patch.
Rozbaľ ho do koreňa GitHub repozitára, nahraď priložené súbory a pridaj nové
src/MapTabsUi.h a tests/map_dock_tabs_regression.cpp. Commitni celý obsah
vrátane CMakeLists.txt a testov. Ostatné súbory projektu ponechaj.

## Karty máp

Predošlá verzia mala súčasne dockovací riadok aj druhý tabbar vo vnútri plochy.
Druhý tabbar je odstránený. Každá mapa má teraz vlastnú natívnu dockovaciu kartu
v spoločnom hornom riadku. Tlačidlo + je priamo v tomto riadku.

Po štarte je NewProject a +. Kliknutie na + používa rovnakú požiadavku na
otvorenie mapy ako File > Open map, vrátane existujúceho sprievodcu pri prvom
použití. Nový prázdny projekt sa naďalej vytvára cez File > New.

Keď sa nová mapa úspešne načíta, nahradí nedotknutú prázdnu aktívnu kartu.
Odstránia sa aj ostatné nedotknuté prázdne karty. Platí to aj pre načítané
prázdne XML, nielen pre nepomenovaný NewProject. Pri zrušení dialógu alebo
chybe načítania sa nič nenahrádza. Otvorenie už otvorenej mapy aktivuje jej kartu.

Za nedotknutú prázdnu mapu sa považuje iba dokument bez neuložených zmien,
bez Undo/Redo a bez objektov, kolízií, spawnov, vlajok, bodov, bonusov,
zón, svetiel či waypointov. Prázdna mapa po editovaní sa automaticky nezatvorí.
Tak sa zachová aj práca, pri ktorej si všetky objekty zmazal.

Nový projekt začína bez príznaku neuloženej zmeny; skutočné úpravy ho nastavia
bežným spôsobom. Samostatné Undo/Redo, kamery, spoločný clipboard a ghost
vkladanie z predchádzajúceho patchu ostávajú. Nové načítanie nepoužije starú
kameru prázdnej karty, aj keď karta pri nahradení dostala rovnaké interné ID.

## Object Editor

Odstránený je nadpis „3D model + independent collision boxes“ nad náhľadom.
Tlačidlo Frame model zostáva. Ovládanie je rozdelené do zbaliteľných sekcií:
Source, Library reference, Source details, Object, Materials & Shading,
Visual mesh (s vnorenými Topology tools), Collision boxes, Save editable draft
 a Export game library.

Dlhé vysvetľujúce odseky boli presunuté na príslušné sekcie a ovládacie prvky
ako tooltipy. Zobrazujú sa po jednej sekunde nad tým istým prvkom; prechod
na iný prvok spustí jeho čakanie odznova. Fungujú aj na zakázaných prvkoch.
Rovnaké oneskorenie teraz používa existujúca pomoc HoverHelp aj v ostatnom UI.
Skutočné hodnoty, stav načítania a konkrétne chyby validácie ostávajú viditeľné.

## Overenie

- 81/81 Python auditov a kontrola konzistencie zdrojov: úspech.
- Test priamo v ImGui 1.91.9b-docking, bez GPU: jeden natívny riadok kariet,
  NewProject +, premenovanie karty bez vytvorenia ďalšej, viac máp, kliknutie
  na + a prepnutie dokumentu: úspech.
- Test dokumentov: oddelené Undo, XML/kolízie medzi mapami, Save As, zatváranie,
  nahradenie čistého prázdneho dokumentu, odstránenie viacerých prázdnych kariet
  a zachovanie editovanej či gameplay mapy: úspech.
- App.cpp, EditorUi.cpp a SceneRenderer.cpp prešli syntaktickou kontrolou
  Clang pre Windows x86-64 s reálnymi Windows/DirectX/ImGui hlavičkami.

Úplné MSVC zostavenie, linkovanie a interaktívny Windows EXE tu neboli spustené.
Tie musí potvrdiť GitHub Actions a následné spustenie aplikácie. Priložený nový
ImGui test je zaregistrovaný v CTest, aby sa kontrola jedného riadku spúšťala
aj pri ďalších buildoch. Tento patch nemení import/export geometrie ani textúr.
