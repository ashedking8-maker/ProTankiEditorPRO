# Windows distribution

The project has two release outputs:

- `ProTankiEditorPRO-0.5.8-Setup.exe` — standard x64 Windows installer.
- `ProTankiEditorPRO-0.5.8-Portable.zip` — portable build; unzip and run.

The end user does **not** need Visual Studio, CMake, Git, NSIS or the Visual C++ Redistributable. The Release target uses the static MSVC runtime and bundled libraries are linked statically.

The original ProTanki `library` directory is not bundled. On first use, choose it through **File > Set library folder...**. Existing map XML files remain external and are opened/saved directly.

## Building without installing Visual Studio on the user's PC

Use the included GitHub Actions workflow `.github/workflows/build-windows-installer.yml`. A Windows Server 2022 runner compiles the application and uploads the Setup EXE + portable ZIP as workflow artifacts.

A local developer build is also possible with `scripts/build-installer.ps1` on a machine that has Visual Studio 2022 C++ Build Tools and NSIS.
