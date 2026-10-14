OPRAVA KONFLIKTU ID V GAMEPLAY PANELI

Aplikujte na verziu s patchom ProTankiEditorPRO_Gameplay_CustomFilters_fix.zip.
Nahraďte src/EditorUi.cpp a spustite build na GitHube.

Kill zones a Kick zones mali rovnake ImGui ID ako stromove zoznamy s rovnakym nazvom.
Prepinace maju teraz skryty suffix ##overlay_visibility; viditelny text zostava rovnaky.
Tym je odstranena pricina cervenej diagnostickej hlasky a konfliktneho zvyraznenia.
Diagnostika ImGui zostava zapnuta. Normalne zvyraznenie tlacidiel a vyziadane tooltipy zostavaju.

Overenie: 81 Python auditov, source preflight, Windows-target syntax kontrola EditorUi.cpp.
Plny MSVC build a GUI beh vyzaduju Windows/GitHub build.
