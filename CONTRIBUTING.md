# Contributing to Godot DocRender

## Set up the repository

Install Git, Python, and a C++ toolchain for your target. For the Windows builds below, install MSVC and the Windows SDK through Visual Studio or Visual Studio Build Tools.

```powershell
git clone https://github.com/LuYingYiLong/godot-docrender.git
cd godot-docrender
git submodule update --init --recursive
python -m pip install scons
```

The submodules provide godot-cpp, MicroTeX and its fonts, TinyXML-2, and litehtml v0.10 with bundled Gumbo. SCons compiles litehtml and Gumbo into the extension on each target. Run a build before opening the solution in Visual Studio so IntelliSense can find the generated godot-cpp bindings and staged litehtml headers. [Visual Studio solution](Godot-DocRender.sln)

DocRender's litehtml changes live in `scripts/litehtml/patches/`, grouped by document integration, CSS, render tree, layout, and Flexbox. Each patch mirrors one path relative to the pinned litehtml source root. SCons uses `git apply` on a temporary copy for each patched file and writes the result to `src/gen/html/`. Edit the patch when changing litehtml behavior. SCons regenerates its output when the patch or upstream input changes.

## Install optional SDKs

On Windows x64, install Merman for Mermaid rendering with MSVC and PDFium for PDF import, reading, and writing. The installers fetch the pinned archives and verify their checksums.

```powershell
python scripts/fetch_merman_sdk.py
python scripts/fetch_pdfium_sdk.py
```

For offline PDFium setup, pass a local copy of the pinned `pdfium-win-x64.tgz` archive. [PDFium SDK release](https://github.com/bblanchon/pdfium-binaries/releases/tag/chromium/8152)

```powershell
python scripts/fetch_pdfium_sdk.py --archive "C:/Downloads/pdfium-win-x64.tgz"
```

## Build and test

Run SCons from the repository root. Build `template_debug` for the editor and debug exports, and `template_release` for release exports.

```powershell
scons godot-docrender platform=windows target=template_debug arch=x86_64
scons godot-docrender platform=windows target=template_release arch=x86_64
```

SCons writes the libraries to `addons/docrender/bin/` and copies `pdfium.dll` there when the Windows x64 SDK is installed. Copy the full `addons/docrender` directory into a Godot project and reopen it to load the new binary. Use the CI build matrix to find `platform` and `arch` values for other targets. [Build workflow](.github/workflows/make_build.yml)

When changing editor text or class documentation, update and check the translation catalogs. [Localization tools](tools/README.md)
