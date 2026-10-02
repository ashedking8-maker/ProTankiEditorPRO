ProTanki Editor PRO 0.5.28 - V8 LIGHT CLIPBOARD HOTFIX

Base: V7 LARGE-WORLD 3DS
Log marker: 0.5.28-light-clipboard-v8

Fixes:
- Native lights can be selected through the same gameplay selection model.
- Newly placed/duplicated lights are synced into functionalSelection_.
- Light click hit radius increased to 24 px.
- Ctrl+C / Ctrl+V now supports a single selected light, preserving its position-independent native properties.
- Pasted light uses gameplay ghost placement (WASD/QE + Space/LMB, RMB cancel).
- Box selection / Ctrl+A visible gameplay selection / Delete work with visible light markers.
- Supported copied omni lights preserve detached native <light> XML, including unknown attributes/children.
- Save refreshes each light's detached native XML snapshot for later copy operations.

Safety:
- Unsupported/unverified new light types remain fail-closed.
- Static prop and collision clipboard logic is unchanged.

Validation performed here:
- source consistency preflight: PASS
- Python unittest discovery: 81 tests PASS
- native CPU regression runner: 7 scalar C++ programs PASS
- Windows/MSVC + full CTest + ProTLVK runtime still require GitHub/user validation.
