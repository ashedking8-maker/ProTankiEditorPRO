# 0.5.12 MSVC compilation fix

The previous 0.5.12 source ZIP itself contained four duplicate member declarations in src/EditorUi.h. The GitHub Actions MSVC compiler stopped with C2535. This was not an old GitHub cache or old repository data.

Removed the repeated declarations of PushObjectUndo, UndoObject, RedoObject, ResetObjectHistory. The header retains one declaration of each; implementations in EditorUi.cpp are unchanged. Extended tools/check_source_consistency.py and added tests/test_editorui_header_audit.py so this cannot silently pass preflight again.

A passing local source/Python audit is not a Windows MSVC build. Please use GitHub Actions to verify the actual build and CTest.
