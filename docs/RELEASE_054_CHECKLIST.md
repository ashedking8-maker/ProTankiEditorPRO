# 0.5.4 validation and remaining scope

Source-preflight is not a Windows runtime or MSVC compile result. In GitHub Actions verify compilation, all CTest cases, Setup.exe output and ZIP integrity. Then on Windows verify:

1. View > Effects > Bright visibly changes building and ground brightness; Off restores original. The pixel shader now gets b0 camera/effect constants.
2. DM arrows green and red/blue team arrows have colored front and side faces on a map with multiple spawn types.
3. View > Editor theme switches three styles without changing map XML.
4. Install from **the newly built** Setup.exe, confirm only a Desktop shortcut appears and there is no Start-folder chooser. Old installers must not be reused.
5. Save As a copy of Sandbox, manipulate a prop, compare untouched XML and test undo; preserve backup before experimenting.

Known incomplete: functional zone editing and roundtrip test for all zone types, physics-faithful native Test (engine sources must be audited), startup splash before Windows process/loader overhead. Avoid inventing parity with original ProTLVK without evidence. Native Test is optional and loads its assets on demand.
