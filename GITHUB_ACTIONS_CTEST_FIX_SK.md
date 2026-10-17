# ProTanki Editor PRO – oprava GitHub Actions / CTest

## Co sa pokazilo
GitHub Actions build 0b5521a (2026-10-17) zostavil ProTankiEditorPRO.exe,
ale pri `ctest --test-dir build -C Release --output-on-failure` spadli 2/45 testov:
- `NativeXmlRotationAndOwnedClipboard` (native_xml_compatibility_regression.cpp:54),
- `NativeClonePartialOwnerEditing` (native_clone_partial_edit_regression.cpp:58).

## Oprava
1. `src/MapDocument.cpp` a `src/NativeCloneEditPolicy.h`: pri vymazani objektu
   oznaceneho ako `collisionOwnershipUnresolved` nevypne ochranu len preto, ze je
   jediny zostavajuci v clone batch. Vynimka je plny vyber batch, ktory skutocne
   obsahuje svoje vlastne opaque collision primitives; tie sa odstrania spolu
   s celou skupinou. Vsetky ostatne parcialne operacie zostavaju bezpecne.
2. `tests/native_clone_partial_edit_regression.cpp`: odstranuje logicky protichodny
   check, ktory nad identickym stavom mapy vyzadoval najprv odmietnutie a
   okamzite potom uspesne vymazanie toho isteho posledneho clone prop.
3. `tests/native_clone_edit_policy_regression.cpp` a Python audit: doplnene
   testy pre 4 kombinacie bezpecneho mazania s unresolved ownership.

## Instalacia
Pouzi na projekt, kde uz bol nainstalovany balicek
`ProTankiEditorPRO_GLOBAL_CLONE_EDIT_FIX_LEN_OPRAVENE_SUBORY_2026-10-18.zip`.
Rozbal iba obsiahnute subory do korena projektu a prepis povodne.
Potom spusti GitHub Actions build nanovo. Nepotrebujes pridavat celu kniznicu.

## Testy v tomto prostredi
- `python tools/check_source_consistency.py` - PASS
- `python -m unittest discover -s tests -p 'test_*audit.py' -q` - 87 PASS
- `g++ -std=c++20 -Wall -Wextra -Werror -Isrc tests/native_clone_edit_policy_regression.cpp` - PASS
Windows MSVC build/CTest tu nie je spustitelny a treba ho potvrdit na GitHube.

## Co ak build znova zlyha?
Posli dalsi log z GitHub Actions, hlavne `ctest --output-on-failure`.
