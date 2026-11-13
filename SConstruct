#!/usr/bin/env python
from pathlib import Path
import re

merman_sdk_option = ARGUMENTS.pop("merman_sdk", None)
pdfium_sdk_option = ARGUMENTS.pop("pdfium_sdk", None)
env = SConscript("godot-cpp/SConstruct")

env.Append(CPPPATH=["src/"])
if env.get("is_msvc", False):
    env.AppendUnique(CCFLAGS=["/utf-8", "/wd4828"])


def enable_cpp_exceptions(build_env):
    if build_env.get("is_msvc", False):
        build_env["CPPDEFINES"] = [define for define in build_env["CPPDEFINES"] if define != ("_HAS_EXCEPTIONS", 0)]
        build_env.AppendUnique(CXXFLAGS=["/EHsc"])
    else:
        build_env["CXXFLAGS"] = [flag for flag in build_env["CXXFLAGS"] if flag != "-fno-exceptions"]
        build_env.AppendUnique(CXXFLAGS=["-fexceptions"])
        if env["platform"] == "web":
            env.AppendUnique(LINKFLAGS=["-fexceptions"])


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

tex_render = File("src/formats/latex/tex_render.cpp")
mermaid_render = File("src/formats/mermaid/mermaid_render.cpp")
html_sources = Glob("src/formats/html/*.cpp")
pdfium_sources = (Glob("src/formats/pdf/reader/*.cpp") + Glob("src/formats/pdf/writer/*.cpp")
                  + Glob("src/formats/pdf/pdfium/*.cpp"))
format_sources = Glob("src/formats/*.cpp") + Glob("src/formats/*/*.cpp")
special_format_sources = [tex_render, mermaid_render] + html_sources
sources = Glob("src/*.cpp") + [source for source in format_sources if source not in special_format_sources]
sources += Glob("src/nodes/*.cpp") + Glob("src/editor/*.cpp") + Glob("src/editor/importers/*.cpp")
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

litehtml = "src/thirdparty/litehtml"
gumbo = f"{litehtml}/src/gumbo"
if not Path(litehtml, "include", "litehtml.h").is_file() or not Path(gumbo, "include", "gumbo.h").is_file():
    print("Missing litehtml or its bundled Gumbo. Run git submodule update --init --recursive.")
    Exit(1)

html_env = env.Clone()
litehtml_headers = "src/gen/html/include"


def expose_litehtml_editing(target, source, env):
    header = Path(str(source[0]))
    text = header.read_text(encoding="utf-8")
    name = header.stem
    declaration = {
        "document": "class document : public std::enable_shared_from_this<document>",
        "element": "class element : public std::enable_shared_from_this<element>",
        "html_tag": "class html_tag : public element",
    }[name] + "\n\t{"
    if text.count(declaration) != 1:
        raise ValueError(f"Update the DocRender editing adapter for {header}")
    text = text.replace(declaration, declaration + "\n\t\tfriend struct docrender_editor;")
    Path(str(target[0])).write_text(text, encoding="utf-8", newline="\n")


def stage_litehtml_extensions(target, source, env):
    path = Path(str(source[0]))
    text = path.read_text(encoding="utf-8")

    def replace_once(anchor, replacement):
        nonlocal text
        if text.count(anchor) != 1:
            raise ValueError(f"Update the DocRender litehtml integration for {path}: {anchor}")
        text = text.replace(anchor, replacement)

    def insert_before(anchor, addition):
        replace_once(anchor, addition + anchor)

    if path.name == "style.h":
        insert_before("#include \"web_color.h\"", '#include "formats/html/litehtml_box_shadow.h"\n')
        insert_before("#include \"web_color.h\"", '#include "formats/html/litehtml_css.h"\n')
        insert_before("\t\tcss_token_vector\n", "\t\tstd::vector<box_shadow>,\n")
        insert_before("\t\tproperty_value() {}", "\t\tstd::shared_ptr<const css_declaration_origin> diagnostic_origin;\n\n")
        insert_before("\t\tstatic std::map<string_id, string>", "\t\tcss_declaration_result* declaration_result = nullptr;\n")
        insert_before("\t\tvoid inherit_property", """
		void add_checked_property(string_id name, const css_token_vector& value, const string& baseurl,
			bool important, document_container* container, const css_source& source, const string* original_value = nullptr);
""")
        replace_once("const css_token_vector& tokens, const string& baseurl = \"\", document_container* container = nullptr);",
                     "const css_token_vector& tokens, const string& baseurl = \"\", document_container* container = nullptr, bool inline_style = false);")
        replace_once("const string& txt,              const string& baseurl = \"\", document_container* container = nullptr);",
                     "const string& txt,              const string& baseurl = \"\", document_container* container = nullptr, bool inline_style = false);")
    elif path.name == "document_container.h":
        insert_before("#include <memory>", '#include "formats/html/litehtml_css.h"\n')
        insert_before("\tstruct list_marker", "\tclass html_tag;\n\n")
        insert_before("\t\tvirtual\tvoid\t\t\t\tset_caption", """
		virtual void draw_box_shadows(const html_tag&, const position&, const borders&, bool) {}
		virtual void draw_styled_text(uint_ptr hdc, const char* text, uint_ptr font, web_color color,
			const position& pos, const element&) { draw_text(hdc, text, font, color, pos); }
		virtual css_source get_css_source(const string&, bool) const { return {}; }
		virtual void report_css_diagnostic(css_diagnostic_kind, const string&, const string&, const css_source&) {}

""")
    elif path.name == "style.cpp":
        insert_before('#include "document.h"', '#include "document_container.h"\n')
        parser = text.split("void style::add_property(string_id name, const css_token_vector&", 1)[1].split(
            "void style::add_property(string_id name, const string&", 1)[0]
        property_cases = re.findall(r"^\s*case (_\w+_):", parser, re.MULTILINE)
        if len(property_cases) < 90 or len(set(property_cases)) != len(property_cases):
            raise ValueError(f"Update the DocRender CSS property dispatch for {path}")
        insert_before("bool has_var(", """
static bool is_known_css_property(string_id name)
{
	if (name == _id("box-shadow") || name == _id("text-shadow")) return true;
	switch (name)
	{
""" + "".join(f"\t\tcase {name}:\n" for name in property_cases) + """
			return true;
		default:
			return _s(name).size() > 2 && _s(name).compare(0, 2, "--") == 0;
	}
}

""")
        replace_once("void style::add(const string& txt, const string& baseurl, document_container* container)",
                     "void style::add(const string& txt, const string& baseurl, document_container* container, bool inline_style)")
        replace_once("\tadd(tokens, baseurl, container);", "\tadd(tokens, baseurl, container, inline_style);")
        replace_once("void style::add(const css_token_vector& tokens, const string& baseurl, document_container* container)",
                     "void style::add(const css_token_vector& tokens, const string& baseurl, document_container* container, bool inline_style)")
        insert_before("\t// Parse each declaration", "\tconst auto source = container ? container->get_css_source(baseurl, inline_style) : css_source{};\n")
        replace_once("\t\tadd_property(_id(name), decl.value, baseurl, decl.important, container);", """
		if (source.kind == css_source_kind::none)
			add_property(_id(name), decl.value, baseurl, decl.important, container);
		else
			add_checked_property(_id(name), decl.value, baseurl, decl.important, container, source);""")
        insert_before("void style::inherit_property", """
void style::add_checked_property(string_id name, const css_token_vector& value, const string& baseurl,
	bool important, document_container* container, const css_source& source, const string* original_value)
{
	const auto spelling = [&] { return original_value ? *original_value : (value.empty() ? string() : get_repr(value, 0, -1, true)); };
	if (!is_known_css_property(name))
	{
		container->report_css_diagnostic(css_diagnostic_kind::unknown_property, _s(name), spelling(), source);
		add_property(name, value, baseurl, important, container);
		return;
	}
	if (has_var(value))
	{
		if (original_value)
		{
			container->report_css_diagnostic(css_diagnostic_kind::unsupported_value, _s(name), spelling(), source);
			return;
		}
		if (_s(name).compare(0, 2, "--") == 0)
		{
			add_property(name, value, baseurl, important, container);
			return;
		}
		property_value deferred(value, important, true);
		deferred.diagnostic_origin = std::make_shared<css_declaration_origin>(css_declaration_origin{ source, spelling() });
		add_parsed_property(name, deferred);
		return;
	}

	css_declaration_result result;
	{
		struct reset_result {
			css_declaration_result*& target;
			~reset_result() { target = nullptr; }
		} reset{ declaration_result };
		declaration_result = &result;
		add_property(name, value, baseurl, important, container);
	}
	if (!result.accepted)
		container->report_css_diagnostic(css_diagnostic_kind::unsupported_value, _s(name), spelling(), source);
	else if (result.not_rendered)
		container->report_css_diagnostic(css_diagnostic_kind::not_rendered, _s(name), spelling(), source);
}

""")
        insert_before("\tauto prop = m_properties.find(name);\n\tif (prop != m_properties.end())", """
	if (declaration_result)
		declaration_result->accept(name, propval);
""")
        replace_once('\t\t\tadd_property(prop.first, value, "", prop.second.m_important, el->get_document()->container());', """
			auto* container = el->get_document()->container();
			const auto origin = prop.second.diagnostic_origin;
			if (origin)
			{
				add_checked_property(prop.first, value, "", prop.second.m_important, container, origin->source, &origin->value);
			}
			else
				add_property(prop.first, value, "", prop.second.m_important, container);""")
        start = text.index("void style::parse_text_decoration(")
        end = text.index("bool style::parse_text_decoration_color", start)
        text = text[:start] + """void style::parse_text_decoration(const css_token_vector& tokens, bool important, document_container* container)
{
	text_decoration decoration;
	if (!parse_decoration(tokens, container, decoration)) return;
	add_parsed_property(_text_decoration_line_, property_value(decoration.line, important));
	add_parsed_property(_text_decoration_style_, property_value(static_cast<int>(decoration.style), important));
	add_parsed_property(_text_decoration_thickness_, property_value(decoration.thickness, important));
	add_parsed_property(_text_decoration_color_, property_value(decoration.color, important));
}

""" + text[end:]
        start = text.index("void style::parse_text_decoration_line(")
        end = text.index("void style::parse_text_emphasis(", start)
        text = text[:start] + """void style::parse_text_decoration_line(const css_token_vector& tokens, bool important)
{
	int lines;
	if (parse_decoration_line(tokens, lines))
		add_parsed_property(_text_decoration_line_, property_value(lines, important));
}

""" + text[end:]
        insert_before("\tint idx[4];\n\tweb_color clr[4];", """
	if (name == _id("box-shadow"))
	{
		std::vector<box_shadow> shadows;
		if (parse_box_shadows(value, container, shadows))
			add_parsed_property(name, property_value(shadows, important));
		return;
	}
	if (name == _id("text-shadow"))
	{
		if (ident == "unset") return inherit_property(name, important);
		std::vector<box_shadow> shadows;
		if (parse_text_shadows(value, container, shadows))
			add_parsed_property(name, property_value(shadows, important));
		return;
	}

""")
    elif path.name == "html_tag.cpp":
        replace_once('m_style.add(style, "", doc->container());', 'm_style.add(style, "", doc->container(), true);')
        replace_once('->draw_text(hdc, marker_text.c_str(), lm.font, lm.color, text_pos);',
                     '->draw_styled_text(hdc, marker_text.c_str(), lm.font, lm.color, text_pos, *this);')
        insert_before("\tif(m_css.get_display() != display_inline && m_css.get_display() != display_table_row)", """
	const auto paint_shadows = [&](position box, borders decoration, bool inset) {
		box.round();
		get_document()->container()->draw_box_shadows(*this, box, decoration, inset);
	};
""")
        insert_before("\t\tif(border_box.does_intersect(clip) || is_root())", """
		borders shadow_box = m_css.get_borders();
		shadow_box.radius = m_css.get_borders().radius.calc_percents(border_box.width, border_box.height);
		paint_shadows(border_box, shadow_box, false);
""")
        insert_before("\t\t\tborders bdr = m_css.get_borders();\n\t\t\tif(bdr.is_visible())",
                      "\t\t\tpaint_shadows(border_box, shadow_box, true);\n")
        insert_before("\t\t\t\tif(bg)\n\t\t\t\t{\n\t\t\t\t\tint num_layers", """
				borders shadow_box = bdr;
				shadow_box.radius = bdr.radius.calc_percents(box->width, box->height);
				paint_shadows(*box, shadow_box, false);
""")
        insert_before("\t\t\t\tif(bdr.is_visible())\n\t\t\t\t{\n\t\t\t\t\tborders b = bdr;",
                      "\t\t\t\tpaint_shadows(*box, shadow_box, true);\n")
    elif path.name == "el_text.cpp":
        replace_once('->draw_text(hdc, m_use_transformed', '->draw_styled_text(hdc, m_use_transformed')
        replace_once('color, pos);', 'color, pos, *el_parent);')
    elif path.name == "css_properties.h":
        insert_before('#include "background.h"', '#include "formats/html/litehtml_box_shadow.h"\n')
        insert_before("\t\tconst string& get_cursor()", "\t\tconst std::vector<box_shadow>& get_text_shadows() const { return m_text_shadows; }\n\n")
        insert_before("\t\telement_position\t\tm_el_position;", "\t\tstd::vector<box_shadow> m_text_shadows;\n")
    elif path.name == "css_properties.cpp":
        insert_before("\tm_font = doc->get_font(descr, &m_font_metrics);", """
	// Resolve lengths on the element declaring them before propagating the decoration.
	const auto unit = m_text_decoration_thickness.units();
	if (!m_text_decoration_thickness.is_predefined() && unit != css_units_percentage && unit != css_units_px && unit != css_units_none)
	{
		font_metrics metrics{};
		metrics.font_size = descr.size;
		if (unit == css_units_ex || unit == css_units_ch)
			doc->get_font(descr, &metrics);
		doc->cvt_units(m_text_decoration_thickness, metrics, 0);
		descr.decoration_thickness = m_text_decoration_thickness;
	}
""")
        insert_before("\tpixel_t font_size = get_font_size();", """
	static const std::vector<box_shadow> no_shadows;
	static const auto text_shadow_property = _id("text-shadow");
	m_text_shadows = el->get_property(text_shadow_property, true, no_shadows, offset(m_text_shadows));
	for (auto& shadow : m_text_shadows)
	{
		doc->cvt_units(shadow.x, m_font_metrics, 0);
		doc->cvt_units(shadow.y, m_font_metrics, 0);
		doc->cvt_units(shadow.blur, m_font_metrics, 0);
	}
""")
    Path(str(target[0])).write_text(text, encoding="utf-8", newline="\n")


# Stage the headers so both litehtml and DocRender see the same class declarations.
staged_headers = []
for header in Glob(f"{litehtml}/include/*.h") + Glob(f"{litehtml}/include/litehtml/*.h"):
    header_path = Path(str(header))
    relative_path = header_path.relative_to(f"{litehtml}/include")
    if header_path.stem in ("document", "element", "html_tag"):
        staged_headers += html_env.Command(f"{litehtml_headers}/{relative_path.as_posix()}",
                                          header, expose_litehtml_editing)
    elif header_path.stem in ("style", "document_container", "css_properties"):
        staged_headers += html_env.Command(f"{litehtml_headers}/{relative_path.as_posix()}",
                                          header, stage_litehtml_extensions)
    else:
        staged_headers += html_env.Install(f"{litehtml_headers}/{relative_path.parent.as_posix()}", header)

html_env.Append(CPPPATH=[litehtml_headers, f"{litehtml_headers}/litehtml", f"{litehtml}/src", f"{gumbo}/include"])
# litehtml uses C++ exceptions during HTML encoding detection.
enable_cpp_exceptions(html_env)
if html_env.get("is_msvc", False):
    html_env.AppendUnique(CXXFLAGS=["/permissive-"])
html_object = html_env.StaticObject if env["platform"] == "ios" else html_env.SharedObject
html_objects = html_object(html_sources)
sources += html_objects

gumbo_env = env.Clone()
gumbo_env.Append(CPPPATH=[f"{gumbo}/include", f"{gumbo}/include/gumbo"])
if gumbo_env.get("is_msvc", False):
    gumbo_env.Append(CPPPATH=[f"{gumbo}/visualc/include"])
    gumbo_env.AppendUnique(CCFLAGS=["/wd4244", "/wd4267"])
else:
    gumbo_env.AppendUnique(CFLAGS=["-std=c99"])
gumbo_object = gumbo_env.StaticObject if env["platform"] == "ios" else gumbo_env.SharedObject

for build_object, library_sources in ((html_object, Glob(f"{litehtml}/src/*.cpp")),
                                     (gumbo_object, Glob(f"{gumbo}/*.c"))):
    for source in library_sources:
        source_path = Path(str(source)).relative_to(litehtml).with_suffix("")
        if source_path.as_posix() in ("src/style", "src/html_tag", "src/css_properties", "src/el_text"):
            source = html_env.Command(f"src/gen/html/{source_path.as_posix()}.cpp",
                                      source, stage_litehtml_extensions)[0]
        objects = build_object(f"src/gen/html/{source_path.as_posix()}{env['OBJSUFFIX']}", source)
        if build_object is html_object:
            html_objects += objects
        sources += objects
html_env.Depends(html_objects, staged_headers)

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
enable_cpp_exceptions(math_env)

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
