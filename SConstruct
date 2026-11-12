#!/usr/bin/env python
from pathlib import Path

merman_sdk_option = ARGUMENTS.pop("merman_sdk", None)
pdfium_sdk_option = ARGUMENTS.pop("pdfium_sdk", None)
env = SConscript("godot-cpp/SConstruct")

env.Append(CPPPATH=["src/"])
if env.get("is_msvc", False):
    env.AppendUnique(CCFLAGS=["/utf-8", "/wd4828"])

mermaid_env = env.Clone()
if env["platform"] == "windows" and env["arch"] == "x86_64" and env.get("is_msvc", False):
    merman_sdk = Path(merman_sdk_option or "src/thirdparty/merman-sdk")
    if (merman_sdk / "include/merman.h").is_file() and (merman_sdk / "lib/merman_ffi.lib").is_file():
        mermaid_env.AppendUnique(CPPPATH=[str(merman_sdk / "include")], CPPDEFINES=["MARKDOWN_LABEL_MERMAN_ENABLED"])
        env.AppendUnique(LIBS=[File(str(merman_sdk / "lib/merman_ffi.lib")),
                               "advapi32", "bcrypt", "kernel32", "ntdll", "userenv", "ws2_32"])
    elif merman_sdk_option:
        print(f"Missing Merman headers or static library in {merman_sdk}.")
        Exit(1)
    else:
        print("Mermaid rendering requires the Windows x64 SDK. Run python scripts/fetch_merman_sdk.py to install it.")

tex_render = "src/core/tex_render.cpp"
mermaid_render = "src/core/mermaid_render.cpp"
pdfium_sources = Glob("src/core/pdf_reader*.cpp") + Glob("src/core/pdf_writer*.cpp") + [File("src/core/pdfium_backend.cpp")]
sources = [source for source in Glob("src/core/*.cpp", exclude=[tex_render, mermaid_render])
           if source not in pdfium_sources]
sources += Glob("src/nodes/*.cpp") + Glob("src/editor/*.cpp")
mermaid_object = mermaid_env.StaticObject if env["platform"] == "ios" else mermaid_env.SharedObject
sources += mermaid_object(mermaid_render)

pdfium_env = env.Clone()
pdfium_dll = []
if env["platform"] == "windows" and env["arch"] == "x86_64":
    pdfium_sdk = Path(pdfium_sdk_option or "src/thirdparty/pdfium-sdk")
    pdfium_library = pdfium_sdk / "bin/pdfium.dll"
    if (pdfium_sdk / "include/fpdfview.h").is_file() and pdfium_library.is_file():
        pdfium_env.AppendUnique(CPPPATH=[str(pdfium_sdk / "include")], CPPDEFINES=["DOCRENDER_PDFIUM_ENABLED"])
        pdfium_dll = env.Install("addons/docrender/bin", str(pdfium_library))
    elif pdfium_sdk_option:
        print(f"Missing PDFium headers or DLL in {pdfium_sdk}.")
        Exit(1)
    else:
        print("PDFium support requires the Windows x64 SDK. Run python scripts/fetch_pdfium_sdk.py to install it.")
pdfium_object = pdfium_env.StaticObject if env["platform"] == "ios" else pdfium_env.SharedObject
sources += pdfium_object(pdfium_sources)

microtex = "src/thirdparty/microtex"
tinyxml2 = "src/thirdparty/tinyxml2"
math_sources = [f"{tinyxml2}/tinyxml2.cpp"]
math_sources += [f"{microtex}/src/{name}.cpp" for name in ("latex", "render")]
for folder in ("atom", "box", "core", "fonts", "utils", "res/builtin", "res/font", "res/parser", "res/reg", "res/sym"):
    math_sources += Glob(f"{microtex}/src/{folder}/*.cpp")


def embed_math_fonts(target, source, env):
    with Path(str(target[0])).open("w", encoding="utf-8", newline="\n") as output:
        output.write("#pragma once\n\n#include <cstddef>\n#include <cstdint>\n\nnamespace {\n")
        output.write("struct EmbeddedMathFont {\n\tconst char *name;\n\tconst uint8_t *data;\n\tstd::size_t size;\n};\n")
        for index, font in enumerate(source):
            data = Path(str(font)).read_bytes()
            output.write(f"constexpr uint8_t math_font_{index}[] = {{\n")
            for offset in range(0, len(data), 24):
                output.write("\t" + ", ".join(f"0x{byte:02x}" for byte in data[offset:offset + 24]) + ",\n")
            output.write("};\n")
        output.write("constexpr EmbeddedMathFont embedded_math_fonts[] = {\n")
        for index, font in enumerate(source):
            output.write(f'\t{{ "{Path(str(font)).name}", math_font_{index}, sizeof(math_font_{index}) }},\n')
        output.write("};\n}\n")


fonts = sorted(Path(microtex, "res", "fonts").rglob("*.ttf"))
if not fonts or not Path(tinyxml2, "tinyxml2.cpp").is_file():
    print("Missing MicroTeX dependencies. Run git submodule update --init --recursive.")
    Exit(1)

font_header = env.Command(
    "src/gen/microtex_fonts.gen.h", [font.as_posix() for font in fonts],
    env.Action(embed_math_fonts, "Embedding MicroTeX fonts"),
)

math_env = env.Clone()
math_env.Append(CPPPATH=[f"{microtex}/src", tinyxml2])
# MicroTeX reports parser errors using C++ exceptions.
if math_env.get("is_msvc", False):
    math_env["CPPDEFINES"] = [define for define in math_env["CPPDEFINES"] if define != ("_HAS_EXCEPTIONS", 0)]
    math_env.AppendUnique(CXXFLAGS=["/EHsc"])
else:
    math_env["CXXFLAGS"] = [flag for flag in math_env["CXXFLAGS"] if flag != "-fno-exceptions"]
    math_env.AppendUnique(CXXFLAGS=["-fexceptions"])
    if env["platform"] == "web":
        env.AppendUnique(LINKFLAGS=["-fexceptions"])

math_object = math_env.StaticObject if env["platform"] == "ios" else math_env.SharedObject
render_object = math_object(tex_render)
math_env.Depends(render_object, font_header)
sources += render_object
for source in math_sources:
    source_path = Path(str(source)).relative_to("src/thirdparty").with_suffix("")
    sources += math_object(f"src/gen/math/{source_path.as_posix()}{math_env['OBJSUFFIX']}", source)

if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "addons/docrender/bin/libdocrender.{}.{}.framework/libdocrender.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=sources,
    )
elif env["platform"] == "ios":
    if env["ios_simulator"]:
        library = env.StaticLibrary(
            "addons/docrender/bin/libdocrender.{}.{}.simulator.a".format(env["platform"], env["target"]),
            source=sources,
        )
    else:
        library = env.StaticLibrary(
            "addons/docrender/bin/libdocrender.{}.{}.a".format(env["platform"], env["target"]),
            source=sources,
        )
else:
    library = env.SharedLibrary(
        "addons/docrender/bin/docrender{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

# The GDExtension descriptor loads the packaged libraries by their existing paths.
Default(Alias("godot-docrender", library + pdfium_dll))
