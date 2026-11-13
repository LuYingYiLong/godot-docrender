# Third-party notices

## PDFium

On Windows x64, `PDFReader`, `PDFWriter`, and the PDF texture importer use `bin/pdfium.dll` from `pdfium-win-x64.tgz`, release `chromium/8152` (PDFium `159.0.8152.0`). Windows x64 export presets with the `docrender_pdf_reader` feature include this DLL and the `PDFium-LICENSES/` directory. The SHA-256 of the DLL from this archive is `ba884cc5c6f2c3cd7c2ecca9dce36c2e341f78eb586abd691a2bbc017aa9871e`. [Upstream release](https://github.com/bblanchon/pdfium-binaries/releases/tag/chromium/8152)

- The binary distribution's MIT license. [Package license](PDFium-LICENSES/LICENSE)
- The PDFium license and notices included for the PDFium code. [PDFium notice](PDFium-LICENSES/licenses/pdfium.txt)
- The complete set of notices supplied for components in the archive, including FreeType, ICU, HarfBuzz, libjpeg-turbo, libpng, and zlib. [Component notices](PDFium-LICENSES/licenses/)

## Merman

Windows x64 MSVC builds with Mermaid support use the Merman `0.9.3` native full SDK. The SDK includes Merman's MIT and Apache 2.0 license texts, its component notices, and instructions for obtaining the corresponding source. [SDK notices](Merman-LICENSES/THIRD_PARTY_NOTICES.md) · [Component licenses](Merman-LICENSES/THIRD_PARTY_LICENSES/) · [Corresponding source](Merman-LICENSES/SOURCE_AVAILABILITY.md)

## Built-in dependencies

- godot-cpp provides the C++ bindings for the GDExtension. [MIT license](godot-cpp-LICENSE.md)
- MicroTeX, TinyXML-2, and the embedded math fonts are used for LaTeX formula rendering. [Licenses and notices](MicroTeX-LICENSES.txt)
- litehtml v0.10 and its bundled Gumbo parser provide HTML parsing and layout for `HTMLDocument`. Both are compiled into the extension. [litehtml BSD 3-Clause license](LiteHTML-LICENSES/litehtml.txt) · [Gumbo Apache 2.0 license](LiteHTML-LICENSES/gumbo.txt)
- Unicode 16.0.0 case-folding data is used to match CSS font family names. [Unicode license](LiteHTML-LICENSES/unicode-case-folding.txt) · [Source data](https://www.unicode.org/Public/16.0.0/ucd/CaseFolding.txt)
