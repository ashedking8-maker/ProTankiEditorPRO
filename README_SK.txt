ProTanki Editor PRO – gameplay overlays a názov aplikácie

Použite nad posledným patchom Gameplay_ID_hotfix (alebo Gameplay_CustomFilters_fix; oprava ID je zahrnutá). Rozbaľte do koreňa GitHub projektu a nahraďte súbory pri zachovaní priečinkov.

Kill zones a Kick zones sa zobrazujú len pri zapnutom Show gameplay overlays, ale automaticky sa nezaškrtnú. Vypnutie hlavného prepínača zruší všetky gameplay zaškrtnutia a nastaví None (hide all). Rovnaká logika platí pre menu a klávesovú skratku.

Výstupný súbor je ProTankiEditorPRO.exe. Inštalátor, odkaz na ploche, spustenie po inštalácii, ikona odinštalovania, manifest a Windows metadata používajú nový názov. Starý EXE v rovnakom inštalačnom priečinku sa odstráni až po overení prítomnosti nového. Logy majú prefix ProTankiEditorPRO.

Kompatibilita: existujúci LocalAppData/GTanksNextEditor priečinok nastavení zostáva, aby sa zachovali ovládanie a vlastné filtre. Interný názov CMake targetu a názvy zdrojových ikon zostávajú kompatibilné s buildom. Historické dokumenty o pôvodnom GTanks editore neboli premenované.

Overenie: 81 Python auditov, source preflight, test zapnutia/vypnutia/opätovného zapnutia overlayov, konzistencia EXE a inštalátora, Windows-target syntax EditorUi.cpp a Logger.cpp. Kompletný MSVC/NSIS build a GUI test tu neboli spustené; vykoná ich GitHub/Windows.
