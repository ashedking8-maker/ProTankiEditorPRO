# 0.5.12 validation status

Source preflight, 11 Python audit cases, and five independently compilable
portable C++ regressions (grid step, bonus modes/spawn headings, projected volume
picking, isolated object draft and first-run guidance) passed in the source workspace. The Windows MSVC build, D3D shader
compilation, full CTest suite, installer packaging, GUI behavior, and real ProTLVK
runtime verification **have not yet been run against these 0.5.12 changes**.

Actual original light mapping remains independently preserved in native XML.
The new render preview is a local visual approximation and does not invent or
change any `<light>` metadata. Source-only distribution is not a Windows EXE.
