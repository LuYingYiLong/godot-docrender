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
    if name == "element":
        anchor = "\t\t\tcss().get_display() == display_flex ||"
        if text.count(anchor) != 1:
            raise ValueError(f"Update the DocRender Grid block classification for {header}")
        text = text.replace(anchor, anchor + "\n\t\t\tcss().get_display() == display_grid ||")
    if name == "html_tag":
        anchor = "\t\telse if (inherited || value.is<inherit>())"
        if text.count(anchor) != 1:
            raise ValueError(f"Update the DocRender CSS property getter for {header}")
        text = text.replace(anchor, "\t\telse if ((inherited && !value.is<initial_value>()) || value.is<inherit>())")
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
        insert_before('#include "web_color.h"', '#include "formats/html/litehtml_grid_tracks.h"\n')
        insert_before("\t\tcss_token_vector\n", "\t\tgrid_track_list,\n")
        insert_before("#include \"web_color.h\"", '#include "formats/html/litehtml_box_shadow.h"\n')
        insert_before("#include \"web_color.h\"", '#include "formats/html/litehtml_css.h"\n')
        insert_before("\t\tcss_token_vector\n", "\t\tstd::vector<box_shadow>,\n")
        insert_before("\t\tcss_token_vector\n", "\t\tcss_font_families,\n")
        insert_before("\tstruct property_value :", "\tstruct initial_value {};\n\n")
        insert_before("\t\tinherit,\n", "\t\tinitial_value,\n")
        insert_before("\t\tproperty_value() {}", """\
		struct deferred_declaration
		{
			string_id name;
			css_token_vector tokens;
			string baseurl;
			css_source source;
		};
		std::shared_ptr<const deferred_declaration> deferred;

""")
        insert_before('#include "web_color.h"', '#include "formats/html/litehtml_css_variables.h"\n')
        insert_before("\t\tstatic std::map<string_id, string>", "\t\tcss_computed_variables computed_variables;\n\t\tprops_map computed_declarations;\n")
        insert_before("\t\tvoid subst_vars(", "\t\tconst css_token_vector* get_custom_property(string_id name, bool& local) const;\n")
        replace_once("\t\tbool m_has_var   = false; // css_token_vector, parsing is delayed because of var()\n", "")
        replace_once("bool important, bool has_var = false)\n\t\t\t: base(val), m_important(important), m_has_var(has_var)",
                     "bool important)\n\t\t\t: base(val), m_important(important)")
        insert_before("\t\tstatic std::map<string_id, string>", """\
		css_declaration_result* declaration_result = nullptr;
		std::shared_ptr<std::vector<css_diagnostic>> diagnostics;
""")
        insert_before("\t\tvoid subst_vars(", """\
		const std::vector<css_diagnostic>* get_diagnostics() const { return diagnostics.get(); }
		void snapshot_default_font_family();
""")
        insert_before("\t\t\tm_properties.clear();", "\t\t\tdiagnostics.reset();\n\t\t\tdefault_font_family.reset();\n\t\t\tcomputed_variables.clear();\n\t\t\tcomputed_declarations.clear();\n")
        insert_before("\t\tstatic std::map<string_id, string>", "\t\tstd::shared_ptr<const property_value> default_font_family;\n")
        insert_before("\t\tvoid inherit_property", """
		void add_checked_property(string_id name, const css_token_vector& value, const string& baseurl,
			bool important, document_container* container, const css_source& source, const html_tag* element);
		void report_diagnostic(document_container& container, css_diagnostic_kind kind, string_id name,
			const string& value, const css_source& source, const html_tag* element,
			std::vector<std::pair<string_id, int>> drawing_values = {});
		void remember_diagnostic(const css_diagnostic& diagnostic);
		void add_deferred_property(string_id name, const css_token_vector& value, const string& baseurl, bool important,
			const css_source& source = {});
		void add_gap_property(string_id name, const css_token_vector& value, bool important);
""")
        replace_once("const css_token_vector& tokens, const string& baseurl = \"\", document_container* container = nullptr);",
                     "const css_token_vector& tokens, const string& baseurl = \"\", document_container* container = nullptr, const html_tag* element = nullptr);")
        replace_once("const string& txt,              const string& baseurl = \"\", document_container* container = nullptr);",
                     "const string& txt,              const string& baseurl = \"\", document_container* container = nullptr, const html_tag* element = nullptr);")
    elif path.name == "document_container.h":
        insert_before("#include <memory>", '#include "formats/html/litehtml_css.h"\n')
        insert_before("\tstruct list_marker", "\tclass html_tag;\n\n")
        insert_before("\t\tvirtual\tvoid\t\t\t\tset_caption", """
		virtual void draw_box_shadows(const html_tag&, const position&, const borders&, bool) {}
		virtual void draw_styled_text(uint_ptr hdc, const char* text, uint_ptr font, web_color color,
			const position& pos, const element&) { draw_text(hdc, text, font, color, pos); }
		virtual css_source get_css_source(const string&, bool) const { return {}; }
		virtual void report_css_diagnostic(const css_diagnostic&, const html_tag* = nullptr) {}

""")
    elif path.name == "font_description.h":
        insert_before('#include "types.h"', '#include "formats/html/litehtml_css.h"\n')
        replace_once("std::string\t\t\t\tfamily;", "css_font_families\t\tfamilies;")
        replace_once("\t\t\tout += family;", """\
			for (const auto& family : families)
			{
				out += family.generic ? 'G' : 'N';
				out += std::to_string(family.name.size()) + ':' + family.name;
			}""")
    elif path.name == "types.h":
        replace_once('flex;inline-flex"', 'flex;inline-flex;grid"')
        replace_once("\t\tdisplay_inline_flex,\n", "\t\tdisplay_inline_flex,\n\t\tdisplay_grid,\n")
        insert_before("\t\t\tsize_mode_content = 0x04,\n", """\
			size_mode_grid_stretch_width = 0x08,
			size_mode_grid_stretch_height = 0x10,
			size_mode_grid_measure_height = 0x20,
			size_mode_grid_measure_width = 0x40,
""")
        replace_once('font_style_strings\t\t"normal;italic"', 'font_style_strings\t\t"normal;italic;oblique"')
        replace_once("\t\tfont_style_italic\n", "\t\tfont_style_italic,\n\t\tfont_style_oblique\n")
    elif path.name == "css_tokenizer.h":
        insert_before("\tstring repr;", "\tbool spaced_operator = false;\n")
        replace_once("str(token.str), repr(token.repr)", "str(token.str), spaced_operator(token.spaced_operator), repr(token.repr)")
    elif path.name == "css_parser.cpp":
        insert_before("static const size_t kLargeSize = 50;", """\
static void remember_math_whitespace(css_token_vector& tokens)
{
	for (size_t index = 1; index + 1 < tokens.size(); ++index)
	{
		auto& token = tokens[index];
		if ((token.ch == '+' || token.ch == '-') && tokens[index - 1].type == WHITESPACE &&
			tokens[index + 1].type == WHITESPACE)
			token.spaced_operator = true;
	}
}

""")
        for function in ("remove_whitespace_large", "remove_whitespace_small"):
            anchor = f"void {function}(css_token_vector& tokens, keep_whitespace_fn keep_whitespace)\n{{\n"
            replace_once(anchor, anchor + "\tremember_math_whitespace(tokens);\n")
    elif path.name == "css_length.h":
        insert_before("namespace litehtml", "#include <memory>\n\n")
        insert_before("\t// from_token options", "\tstruct css_math_expression;\n\n")
        insert_before("\t\tf_positive   = 16", "\t\tf_math       = 32, // CSS length math functions\n")
        insert_before("\t\tunion\n", "\t\tstd::shared_ptr<const css_math_expression> m_expression;\n")
        insert_before("\t\tpixel_t\t\tcalc_percent", """\
		const auto& expression() const { return m_expression; }
		void set_expression(std::shared_ptr<const css_math_expression> p_expression);
		pixel_t evaluate_expression(pixel_t basis) const;
""")
        for signature in ("css_length&\tcss_length::operator=(float val)", "void css_length::predef(int val)",
                          "void css_length::set_value(float val, css_units units)"):
            start = text.index(signature)
            brace = text.index("\n", text.index("{", start))
            text = text[:brace] + "\n\t\tm_expression.reset();" + text[brace:]
        replace_once("inline pixel_t css_length::calc_percent(pixel_t width) const\n\t{",
                     "inline pixel_t css_length::calc_percent(pixel_t width) const\n\t{\n\t\tif (m_expression) return evaluate_expression(width);")
    elif path.name == "css_length.cpp":
        insert_before('#include "css_length.h"', '#include "formats/html/litehtml_css_math.h"\n')
        insert_before("bool css_length::from_token", """\
void css_length::set_expression(std::shared_ptr<const css_math_expression> p_expression)
{
	set_value(0, p_expression->has_percentage ? css_units_percentage : css_units_px);
	m_expression = std::move(p_expression);
}

pixel_t css_length::evaluate_expression(pixel_t basis) const
{
	return static_cast<pixel_t>(m_expression->evaluate(basis));
}

""")
        anchor = "bool css_length::from_token(const css_token& token, int options, const string& keywords)\n{\n"
        replace_once(anchor, anchor + """\
	if (token.type == CV_FUNCTION && (options & f_math))
	{
		if (auto expression = parse_css_math(token, options))
		{
			set_expression(std::move(expression));
			return true;
		}
		return false;
	}
	m_expression.reset();
""")
        replace_once("string css_length::to_string() const\n{", "string css_length::to_string() const\n{\n\tif (m_expression) return m_expression->to_string();")
    elif path.name == "document.cpp":
        insert_before('#include "document.h"', '#include "formats/html/litehtml_css_math.h"\n')
        anchor = "void document::cvt_units( css_length& val, const font_metrics& metrics, pixel_t size ) const\n{\n"
        replace_once(anchor, anchor + """\
	if (const auto& expression = val.expression())
	{
		auto computed = compute_css_math(*expression, *this, metrics);
		if (computed->has_percentage)
			val.set_expression(std::move(computed));
		else
			val.set_value(static_cast<float>(computed->evaluate(0)), css_units_px);
		return;
	}
""")
    elif path.name == "element.cpp":
        insert_before('#include "render_flex.h"', '#include "formats/html/litehtml_render_grid.h"\n')
        insert_before("\tif(ret)\n", """\
	if (css().get_display() == display_grid)
		ret = std::make_shared<render_item_grid>(shared_from_this());
""")
        insert_before("\tif(\tm_css.get_display() == display_inline_block ||", """\
	if (m_css.get_display() == display_grid)
		return true;
	if (const auto p = parent(); p && p->css().get_display() == display_grid && m_css.get_display() != display_inline_text)
		return true;
""")
    elif path.name == "render_item.cpp":
        # Carry the distinction between a known zero basis and an auto height through sizing.
        replace_once("const css_length& len, pixel_t percent_base, containing_block_context::typed_pixel& out_value",
                     "const css_length& len, containing_block_context::typed_pixel percent_base, containing_block_context::typed_pixel& out_value")
        insert_before("\t\tif(len.units() == litehtml::css_units_percentage)", """\
		if (len.expression() && len.units() == css_units_percentage &&
			is_one_of(percent_base.type, containing_block_context::cbc_value_type_auto, containing_block_context::cbc_value_type_none))
			return;
""")
        replace_once("calc_cb_length(*width, cb_context.width, ret.width);",
                     "calc_cb_length(*width, width_basis, ret.width);")
        for limit in ("min_width", "max_width"):
            replace_once(f"calc_cb_length(src_el()->css().get_{limit}(), cb_context.width, ret.{limit});",
                         f"calc_cb_length(src_el()->css().get_{limit}(), width_basis, ret.{limit});")
        insert_before("\t// Calculate width if css property is not auto", """\
	auto width_basis = cb_context.width;
	if (!(cb_context.size_mode & containing_block_context::size_mode_content))
		width_basis.type = containing_block_context::cbc_value_type_absolute;

""")
        # A mixed calculation resolved against a definite size follows the explicit-size path.
        replace_once("out_value.type = litehtml::containing_block_context::cbc_value_type_percentage;",
                     "out_value.type = len.expression() ? containing_block_context::cbc_value_type_absolute : containing_block_context::cbc_value_type_percentage;")
        insert_before("\t// Calculate width if css property is not auto", """\
	const bool stretch_width = cb_context.size_mode & containing_block_context::size_mode_grid_stretch_width;
	const bool stretch_height = cb_context.size_mode & containing_block_context::size_mode_grid_stretch_height;
""")
        for axis in ("width", "height"):
            replace_once(f"if(cb_context.size_mode & containing_block_context::size_mode_exact_{axis})\n\t\t{{\n\t\t\tret.{axis}.value = cb_context.{axis};",
                         f"if((cb_context.size_mode & containing_block_context::size_mode_exact_{axis}) || stretch_{axis})\n"
                         f"\t\t{{\n\t\t\tret.{axis}.value = stretch_{axis} ? std::max<pixel_t>(0, cb_context.{axis} - render_offset_{axis}()) : cb_context.{axis};")
        insert_before("\t\tif (len.expression() && len.units() == css_units_percentage &&",
                      "\t\tconst bool grid_sizing = css().get_display() == display_grid || is_grid_item();\n")
        replace_once("if (len.expression() && len.units() == css_units_percentage &&",
                     "if ((len.expression() || grid_sizing) && len.units() == css_units_percentage &&")
        replace_once("out_value.type = len.expression() ? containing_block_context::cbc_value_type_absolute",
                     "out_value.type = (len.expression() || grid_sizing) ? containing_block_context::cbc_value_type_absolute")
        replace_once("if ((src_el()->css().get_display() == display_block || src_el()->css().get_display() == display_table) &&",
                     "if ((src_el()->css().get_display() == display_block || src_el()->css().get_display() == display_table ||\n"
                     "            src_el()->css().get_display() == display_grid || is_grid_item()) &&")
        replace_once("if(src_el()->is_block_formatting_context() || ! fmt_ctx)",
                     "if(src_el()->is_block_formatting_context() || is_grid_item() || ! fmt_ctx)")
        replace_once("\tcalc_outlines(containing_block_size.width);",
                     "\tcalc_outlines(containing_block_size.size_mode & containing_block_context::size_mode_grid_measure_width ?\n"
                     "\t\t0 : containing_block_size.width.value);")
        insert_before("\t\t\t\tfor(const auto& el : m_children)\n\t\t\t\t{\n\t\t\t\t\tel->calc_document_size(child_size", "\t\t\t\tchild_size = layout_overflow();\n")
        insert_before("\t\t\t\t\tfor(const auto& el : m_children)\n\t\t\t\t\t{\n\t\t\t\t\t\tel->calc_document_size(sz", """\
					if (css().get_display() == display_grid && css().get_position() != element_position_fixed)
					{
						const auto overflow_size = layout_overflow();
						sz.width = std::max(sz.width, x + m_pos.x + overflow_size.width);
						sz.height = std::max(sz.height, y + m_pos.y + overflow_size.height);
					}
""")
    elif path.name == "render_item.h":
        replace_once("const css_length& len, pixel_t percent_base, containing_block_context::typed_pixel& out_value",
                     "const css_length& len, containing_block_context::typed_pixel percent_base, containing_block_context::typed_pixel& out_value")
        insert_before("\t\tvirtual rendered_width", "\t\tvirtual size layout_overflow() const { return {}; }\n")
        insert_before("\t\tbool is_flex_item() const", """\
		bool is_grid_item() const
		{
			auto p = parent();
			return p && p->css().get_display() == display_grid && src_el()->in_normal_flow();
		}

""")
        for edge in ("top", "bottom"):
            insert_before(f"                   m_margins.{edge} >= 0 &&", """\
                   css().get_display() != display_grid &&
                   !is_grid_item() &&
""")
    elif path.name == "flex_item.cpp":
        insert_before("\n\tif (flex_basis_predefined)\n\t{\n\t\tif(predef == flex_basis_auto && el->css().get_height()", """\

	if (!flex_basis_predefined && el->css().get_flex_basis().expression() && el->css().get_flex_basis().units() == css_units_percentage &&
		self_size.height.type == containing_block_context::cbc_value_type_auto)
	{
		flex_basis_predefined = true;
		predef = flex_basis_fit_content;
	}
""")
    elif path.name == "string_id.h":
        insert_before("\t_flex_flow_,", "\t_gap_,\n\t_row_gap_,\n\t_column_gap_,\n")
        insert_before("\t_flex_flow_,", "\t_grid_template_columns_,\n\t_grid_template_rows_,\n")
    elif path.name == "style.cpp":
        insert_before('#include <set>', '#include <cmath>\n')
        insert_before('#include "document.h"', '#include "document_container.h"\n')
        for anchor in (
            'return add_length_property(name, val, "", f_length_percentage|f_positive, important);',
            'return add_length_property(name, val, "auto", f_length_percentage, important);',
            'return add_length_property(name, val, "auto", f_length_percentage|f_positive, important);',
            'return add_length_property(name, val, "none", f_length_percentage|f_positive, important);',
            'add_length_property(name, val, flex_basis_strings, f_length_percentage|f_positive, important);',
        ):
            replace_once(anchor, anchor.replace(", important);", "|f_math, important);"))
        replace_once('parse_1234_lengths(value, len, f_length_percentage, "auto")',
                     'parse_1234_lengths(value, len, f_length_percentage | f_math, "auto")')
        replace_once('parse_1234_lengths(value, len, f_length_percentage | f_positive))\n\t\t\tadd_four_properties(_padding_top_',
                     'parse_1234_lengths(value, len, f_length_percentage | f_positive | f_math))\n\t\t\tadd_four_properties(_padding_top_')
        replace_once('m_basis.from_token(tok, f_length_percentage | f_positive, flex_basis_strings)',
                     'm_basis.from_token(tok, f_length_percentage | f_positive | f_math, flex_basis_strings)')
        insert_before("\t{ _font_,", """\
	{ _gap_, {_row_gap_, _column_gap_}},
	{ _background_position_, {_background_position_x_, _background_position_y_}},
	{ _background_image_, {_background_image_, _background_image_baseurl_}},
	{ _list_style_image_, {_list_style_image_, _list_style_image_baseurl_}},
	{ _border_spacing_, {__litehtml_border_spacing_x_, __litehtml_border_spacing_y_}},
	{ _border_radius_, {_border_top_left_radius_x_, _border_top_left_radius_y_, _border_top_right_radius_x_, _border_top_right_radius_y_,
		_border_bottom_left_radius_x_, _border_bottom_left_radius_y_, _border_bottom_right_radius_x_, _border_bottom_right_radius_y_}},
	{ _border_top_left_radius_, {_border_top_left_radius_x_, _border_top_left_radius_y_}},
	{ _border_top_right_radius_, {_border_top_right_radius_x_, _border_top_right_radius_y_}},
	{ _border_bottom_left_radius_, {_border_bottom_left_radius_x_, _border_bottom_left_radius_y_}},
	{ _border_bottom_right_radius_, {_border_bottom_right_radius_x_, _border_bottom_right_radius_y_}},
""")
        replace_once("bool parse_font_family(const css_token_vector& tokens, string& font_family);\n", "")
        start = text.index("// https://drafts.csswg.org/css-fonts/#propdef-font-family")
        end = text.index("// https://developer.mozilla.org/en-US/docs/Web/CSS/font", start)
        text = text[:start] + text[end:]
        replace_once("\tcase _font_family_:\n\t\tif (parse_font_family(value, str))\n\t\t\tadd_parsed_property(name, property_value(str, important));\n\t\tbreak;", """\
	case _font_family_:
	{
		css_font_families families;
		if (ident == "unset") return inherit_property(name, important);
		if (is_one_of(ident, "revert", "revert-layer"))
		{
			add_parsed_property(name, property_value(ident, important));
			break;
		}
		if (ident == "initial" || parse_font_families(value, families))
			add_parsed_property(name, property_value(families, important));
		break;
	}""")
        replace_once("string font_family; // this argument is mandatory, no need to set initial value", "css_font_families font_family;")
        replace_once('if(tokens.size() == 1 && (tokens[0].type == STRING || tokens[0].type == IDENT) && value_in_list(tokens[0].str, font_system_family_name_strings))\n\t{\n\t\tfont_family = tokens[0].str;',
                     'if (tokens.size() == 1 && value_in_list(tokens[0].ident(), font_system_family_name_strings))\n\t{\n\t\tfont_family.push_back({ "system-ui", true });')
        replace_once("if (!parse_font_family(tokens, font_family))", "if (!parse_font_families(tokens, font_family))")
        insert_before("\tcase _order_:", """\
	case _grid_template_columns_:
	case _grid_template_rows_:
	{
		grid_track_list tracks;
		if (ident == "initial" || ident == "unset" || parse_grid_tracks(value, tracks))
			add_parsed_property(name, property_value(std::move(tracks), important));
		break;
	}
""")
        parser = text.split("void style::add_property(string_id name, const css_token_vector&", 1)[1].split(
            "void style::add_property(string_id name, const string&", 1)[0]
        property_cases = re.findall(r"^\s*case (_\w+_):", parser, re.MULTILINE)
        if len(property_cases) < 90 or len(set(property_cases)) != len(property_cases):
            raise ValueError(f"Update the DocRender CSS property dispatch for {path}")
        start = text.index("bool has_var(")
        end = text.index("void style::inherit_property", start)
        text = text[:start] + text[end:]
        insert_before("void style::inherit_property", """
static bool is_known_css_property(string_id name)
{
	if (name == _id("box-shadow") || name == _id("text-shadow")) return true;
	switch (name)
	{
		case _gap_:
		case _row_gap_:
		case _column_gap_:
""" + "".join(f"\t\tcase {name}:\n" for name in property_cases) + """
			return true;
		default:
			return _s(name).size() > 2 && _s(name).compare(0, 2, "--") == 0;
	}
}

""")
        replace_once("void style::add(const string& txt, const string& baseurl, document_container* container)",
                     "void style::add(const string& txt, const string& baseurl, document_container* container, const html_tag* element)")
        replace_once("\tadd(tokens, baseurl, container);", "\tadd(tokens, baseurl, container, element);")
        replace_once("void style::add(const css_token_vector& tokens, const string& baseurl, document_container* container)",
                     "void style::add(const css_token_vector& tokens, const string& baseurl, document_container* container, const html_tag* element)")
        insert_before("\t// Parse each declaration", "\tconst auto source = container ? container->get_css_source(baseurl, element != nullptr) : css_source{};\n")
        replace_once("\t\tadd_property(_id(name), decl.value, baseurl, decl.important, container);", """
		if (source.kind == css_source_kind::none)
			add_property(_id(name), decl.value, baseurl, decl.important, container);
		else
			add_checked_property(_id(name), decl.value, baseurl, decl.important, container, source, element);""")
        insert_before("void style::inherit_property", """
void style::snapshot_default_font_family()
{
	const auto& property = get_property(_font_family_);
	if (!property.is<invalid>()) default_font_family = std::make_shared<property_value>(property);
}

void style::report_diagnostic(document_container& container, css_diagnostic_kind kind, string_id name,
	const string& value, const css_source& source, const html_tag* element,
	std::vector<std::pair<string_id, int>> drawing_values)
{
	css_diagnostic diagnostic{ kind, _s(name), value, source, std::move(drawing_values) };
	container.report_css_diagnostic(diagnostic, kind == css_diagnostic_kind::not_rendered ? nullptr : element);
	if (!element || kind == css_diagnostic_kind::not_rendered) remember_diagnostic(diagnostic);
}

void style::remember_diagnostic(const css_diagnostic& diagnostic)
{
	if (!diagnostics) diagnostics = std::make_shared<std::vector<css_diagnostic>>();
	const auto same_declaration = [&](const css_diagnostic& other) {
		return diagnostic.kind == other.kind && diagnostic.property == other.property && diagnostic.value == other.value &&
			diagnostic.source.kind == other.source.kind && diagnostic.source.path == other.source.path &&
			diagnostic.drawing_values == other.drawing_values;
	};
	if (std::none_of(diagnostics->begin(), diagnostics->end(), same_declaration))
		diagnostics->push_back(diagnostic);
}

void style::add_checked_property(string_id name, const css_token_vector& value, const string& baseurl,
	bool important, document_container* container, const css_source& source, const html_tag* element)
{
	const auto spelling = [&] { return value.empty() ? string() : get_repr(value, 0, -1, true); };
	const bool custom = _s(name).compare(0, 2, "--") == 0;
	if (!is_known_css_property(name))
	{
		report_diagnostic(*container, css_diagnostic_kind::unknown_property, name, spelling(), source, element);
		add_property(name, value, baseurl, important, container);
		return;
	}
	const auto variables = inspect_css_variables(value);
	if (variables == css_variable_syntax::invalid)
	{
		report_diagnostic(*container, css_diagnostic_kind::unsupported_value, name, spelling(), source, element);
		return;
	}
	if (variables == css_variable_syntax::valid)
	{
		if (custom) add_property(name, value, baseurl, important, container);
		else add_deferred_property(name, value, baseurl, important, source);
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
		report_diagnostic(*container, css_diagnostic_kind::unsupported_value, name, spelling(), source, element);
	else if (!result.drawing_values.empty())
		report_diagnostic(*container, css_diagnostic_kind::not_rendered, name, spelling(), source, element,
			std::move(result.drawing_values));
}

void style::add_deferred_property(string_id name, const css_token_vector& value, const string& baseurl, bool important,
	const css_source& source)
{
	property_value deferred(invalid{}, important);
	deferred.deferred = std::make_shared<property_value::deferred_declaration>(property_value::deferred_declaration{ name, value, baseurl, source });
	const auto longhands = shorthands.find(name);
	if (longhands != shorthands.end())
		for (const auto longhand : longhands->second) add_parsed_property(longhand, deferred);
	else
		add_parsed_property(name, deferred);
}

void style::add_gap_property(string_id name, const css_token_vector& value, bool important)
{
	const bool shorthand = name == _gap_;
	if (value.empty() || value.size() > (shorthand ? 2 : 1)) return;

	const string keyword = value.size() == 1 ? value.front().ident() : string();
	property_value row;
	property_value column;
	if (keyword == "inherit")
	{
		row = column = property_value(inherit(), important);
	}
	else if (keyword == "initial" || keyword == "unset")
	{
		row = column = property_value(css_length(), important);
	}
	else
	{
		const auto parse_length = [](const css_token& token, css_length& gap) {
			return gap.from_token(token, f_length_percentage | f_positive | f_math, "normal") &&
				(gap.is_predefined() || std::isfinite(gap.val()));
		};
		css_length row_length;
		css_length column_length;
		if (!parse_length(value.front(), row_length) || !parse_length(value.back(), column_length)) return;
		row = property_value(row_length, important);
		column = property_value(column_length, important);
	}
	if (name != _column_gap_) add_parsed_property(_row_gap_, row);
	if (name != _row_gap_) add_parsed_property(_column_gap_, column);
}

""")
        insert_before("\tauto prop = m_properties.find(name);\n\tif (prop != m_properties.end())", """
	if (declaration_result)
		declaration_result->accept(name, propval);
""")
        insert_before("\tfor (const auto& property : src.m_properties)", """\
	if (src.diagnostics)
	{
		for (const auto& diagnostic : *src.diagnostics)
			remember_diagnostic(diagnostic);
	}
""")
        replace_once("\tauto it = m_properties.find(name);\n\tif (it != m_properties.end())\n\t{\n\t\treturn it->second;\n\t}", """\
	const auto computed = computed_declarations.find(name);
	const auto specified = m_properties.find(name);
	const auto* value = computed != computed_declarations.end() ? &computed->second :
		(specified != m_properties.end() ? &specified->second : nullptr);
	if (value)
	{
		// Author rules share one unlayered cascade. Roll back to default styles or inheritance.
		if (name == _font_family_ && value->is<string>())
		{
			static const property_value inherited{ inherit(), false };
			return default_font_family ? *default_font_family : inherited;
		}
		return *value;
	}""")
        start = text.index("// var( <custom-property-name>")
        text = text[:start] + """\
const css_token_vector* style::get_custom_property(string_id name, bool& local) const
{
	const auto it = computed_variables.find(name);
	local = it != computed_variables.end();
	return local && it->second ? &*it->second : nullptr;
}

void style::subst_vars(const html_tag* el)
{
	computed_declarations.clear();
	css_custom_properties custom_properties;
	bool has_deferred = false;
	for (const auto& [name, property] : m_properties)
	{
		has_deferred |= bool(property.deferred);
		if (_s(name).compare(0, 2, "--") != 0) continue;
		if (property.is<css_token_vector>())
			custom_properties.emplace(name, &property.get<css_token_vector>());
		else if (!property.is<inherit>())
			custom_properties.emplace(name, nullptr);
	}
	if (custom_properties.empty() && !has_deferred)
	{
		computed_variables.clear();
		return;
	}
	const auto parent = el->parent();
	css_variable_resolver resolver(dynamic_cast<const html_tag*>(parent.get()), custom_properties);
	auto* container = el->get_document()->container();
	std::map<const property_value::deferred_declaration*, props_map> parsed_declarations;
	for (const auto& [name, property] : m_properties)
	{
		if (!property.deferred) continue;
		const auto& declaration = *property.deferred;
		auto [entry, inserted] = parsed_declarations.try_emplace(&declaration);
		if (inserted)
		{
			style parsed;
			css_declaration_result result;
			parsed.declaration_result = &result;
			if (const auto value = resolver.substitute(declaration.tokens))
			{
				const auto keyword = value->size() == 1 ? value->front().ident() : string();
				if (keyword == "initial" || keyword == "unset")
				{
					result.accepted = true;
					if (keyword == "initial")
					{
						const auto longhands = shorthands.find(declaration.name);
						if (longhands != shorthands.end())
							for (const auto longhand : longhands->second) parsed.m_properties[longhand] = property_value(initial_value{}, property.m_important);
						else
							parsed.m_properties[declaration.name] = property_value(initial_value{}, property.m_important);
					}
				}
				else
					parsed.add_property(declaration.name, *value, declaration.baseurl, property.m_important, container);
			}
			entry->second = std::move(parsed.m_properties);
			if (declaration.source.kind != css_source_kind::none && (!result.accepted || !result.drawing_values.empty()))
			{
				const auto kind = result.accepted ? css_diagnostic_kind::not_rendered : css_diagnostic_kind::unsupported_value;
				container->report_css_diagnostic({ kind, _s(declaration.name), get_repr(declaration.tokens, 0, -1, true),
					declaration.source, std::move(result.drawing_values) }, el);
			}
		}
		if (const auto it = entry->second.find(name); it != entry->second.end())
			computed_declarations.emplace(name, it->second);
	}
	computed_variables = resolver.take_computed();
}

} // namespace litehtml
"""
        replace_once("\tif (has_var(value))\n\t\treturn add_parsed_property(name, property_value(value, important, true));", """\
	const auto variables = inspect_css_variables(value);
	if (variables == css_variable_syntax::invalid) return;
	if (_s(name).compare(0, 2, "--") == 0 && _s(name).size() > 2)
	{
		const auto keyword = value.size() == 1 ? value.front().ident() : string();
		if (is_one_of(keyword, "inherit", "unset", "revert", "revert-layer")) return inherit_property(name, important);
		if (keyword == "initial") return add_parsed_property(name, property_value(invalid{}, important));
		if (value.empty() || is_declaration_value(value)) add_parsed_property(name, property_value(value, important));
		return;
	}
	if (variables == css_variable_syntax::valid)
		return add_deferred_property(name, value, baseurl, important);
	if (is_one_of(name, _gap_, _row_gap_, _column_gap_))
		return add_gap_property(name, value, important);""")
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
        replace_once('m_style.add(style, "", doc->container());', 'm_style.add(style, "", doc->container(), this);')
        start = text.index("bool html_tag::get_custom_property(")
        end = text.index("void litehtml::html_tag::compute_styles", start)
        text = text[:start] + """\
bool html_tag::get_custom_property(string_id name, css_token_vector& result) const
{
	for (auto tag = this; tag;)
	{
		bool local = false;
		if (const auto* value = tag->m_style.get_custom_property(name, local))
		{
			result = *value;
			return true;
		}
		if (local) return false;
		tag = dynamic_cast<const html_tag*>(tag->parent().get());
	}
	return false;
}

""" + text[end:]
        insert_before("\n\tm_css.compute(this, doc);", """
	if (const auto* diagnostics = m_style.get_diagnostics())
	{
		for (const auto& diagnostic : *diagnostics)
		{
			const auto remains_unsupported = [&](const auto& item) {
				const auto& value = m_style.get_property(item.first);
				return item.first == _text_emphasis_style_
					? value.is<string>() && lowcase(value.get<string>()) != "none"
					: value.is<int>() && value.get<int>() == item.second;
			};
			if (diagnostic.drawing_values.empty() ||
				std::any_of(diagnostic.drawing_values.begin(), diagnostic.drawing_values.end(), remains_unsupported))
			{
				doc->container()->report_css_diagnostic(diagnostic, this);
			}
		}
	}
""")
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
        insert_before('#include "background.h"', '#include "formats/html/litehtml_grid_tracks.h"\n')
        insert_before("\t\tflex_wrap get_flex_wrap() const;", """\
		const grid_track_list& get_grid_template_columns() const { return m_grid_template_columns; }
		const grid_track_list& get_grid_template_rows() const { return m_grid_template_rows; }
""")
        insert_before("\t\tflex_wrap\t\t\t\tm_flex_wrap;", "\t\tgrid_track_list m_grid_template_columns, m_grid_template_rows;\n")
        insert_before('#include "background.h"', '#include "formats/html/litehtml_box_shadow.h"\n')
        insert_before('#include "background.h"', '#include "formats/html/litehtml_css.h"\n')
        replace_once("string\t\t\t\t\tm_font_family;", "css_font_families\t\tm_font_families;")
        insert_before("\t\tflex_wrap get_flex_wrap() const;", """
		const css_length& get_row_gap() const { return m_row_gap; }
		const css_length& get_column_gap() const { return m_column_gap; }
""")
        insert_before("\t\tflex_wrap\t\t\t\tm_flex_wrap;", "\t\tcss_length m_row_gap, m_column_gap;\n")
        insert_before("\t\tconst string& get_cursor()", "\t\tconst std::vector<box_shadow>& get_text_shadows() const { return m_text_shadows; }\n\n")
        insert_before("\t\telement_position\t\tm_el_position;", "\t\tstd::vector<box_shadow> m_text_shadows;\n")
    elif path.name == "css_properties.cpp":
        insert_before("\tcompute_flex(el, doc);", """\
	m_grid_template_columns = el->get_property(_grid_template_columns_, false, grid_track_list(), offset(m_grid_template_columns));
	m_grid_template_rows = el->get_property(_grid_template_rows_, false, grid_track_list(), offset(m_grid_template_rows));
	for (auto* tracks : {&m_grid_template_columns, &m_grid_template_rows})
		for (auto& track : *tracks)
			for (auto* breadth : {&track.minimum, &track.maximum})
				if (breadth->kind == grid_breadth_kind::length) doc->cvt_units(breadth->length, m_font_metrics, 0);

	if (const auto p = el->parent(); p && p->css().get_display() == display_grid && el->in_normal_flow())
	{
		m_float = float_none;
		m_clear = clear_none;
		switch (m_display)
		{
			case display_inline_table: m_display = display_table; break;
			case display_inline_flex: m_display = display_flex; break;
			case display_inline:
			case display_inline_block:
			case display_table_caption:
			case display_table_cell:
			case display_table_row:
			case display_table_column:
			case display_table_column_group:
			case display_table_row_group:
			case display_table_header_group:
			case display_table_footer_group: m_display = display_block; break;
			default: break;
		}
	}

""")
        replace_once("m_font_family\t\t=              el->get_property<string>(    _font_family_,\t\ttrue, doc->container()->get_default_font_name(),\toffset(m_font_family));",
                     "m_font_families = el->get_property(_font_family_, true, css_font_families(), offset(m_font_families));")
        replace_once("descr.family \t\t\t\t= m_font_family;", "descr.families\t\t\t\t= m_font_families;")
        insert_before("\tif (m_display == display_flex || m_display == display_inline_flex)\n", """
	// Keep computed values on all elements so explicit inheritance works through block ancestors.
	m_row_gap = el->get_property(_row_gap_, false, css_length(), offset(m_row_gap));
	m_column_gap = el->get_property(_column_gap_, false, css_length(), offset(m_column_gap));
	doc->cvt_units(m_row_gap, m_font_metrics, 0);
	doc->cvt_units(m_column_gap, m_font_metrics, 0);

""")
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
    elif path.name == "flex_line.h":
        insert_before("\t\tpixel_t cross_start;", "\t\tpixel_t main_gap = 0;\n")
        replace_once("void init(pixel_t container_main_size, bool fit_container, bool is_row_direction,",
                     "void init(pixel_t container_main_size, bool is_row_direction,")
        insert_before("\t\tvoid distribute_free_space(pixel_t container_main_size);", """\
		pixel_t total_main_gap() const
		{
			return items.empty() ? 0 : main_gap * static_cast<pixel_t>(items.size() - 1);
		}
""")
    elif path.name == "flex_line.cpp":
        insert_before("\tif (main_size < container_main_size)", """\
	// Reserve gutters before distributing the remaining space among the items.
	const pixel_t available_item_size = container_main_size - total_main_gap();
""")
        for mode in ("grow", "shrink"):
            replace_once(f"\t\tdistribute_free_space_{mode}(container_main_size);",
                         f"\t\tdistribute_free_space_{mode}(available_item_size);")
        # A gap alone can exhaust the available space even when every item has a zero flex basis.
        insert_before("\t\tif (sum_flex_shrink_factor < 1000)", """\
		if (sum_scaled_flex_shrink_factor <= 0) break;

""")
        replace_once("void litehtml::flex_line::init(pixel_t container_main_size, bool fit_container, bool is_row_direction,",
                     "void litehtml::flex_line::init(pixel_t container_main_size, bool is_row_direction,")
        replace_once("\tif(!fit_container)\n\t{\n\t\tdistribute_free_space(container_main_size);\n\t}",
                     "\tdistribute_free_space(container_main_size);")
        replace_once("\tmain_size = 0;", "\tmain_size = total_main_gap();")
        start = text.index("\tif(free_main_size > 0 && (num_auto_margin_main_start || num_auto_margin_main_end))")
        end = text.index("\nvoid litehtml::flex_line::init(", start)
        text = text[:start] + """\
	const int margin_count = num_auto_margin_main_start + num_auto_margin_main_end;
	if (free_main_size <= 0 || margin_count == 0) return false;

	const pixel_t share = free_main_size / static_cast<pixel_t>(margin_count);
	for (auto& item : items)
	{
		if (!item->auto_margin_main_start.is_default()) item->auto_margin_main_start = share;
		if (!item->auto_margin_main_end.is_default()) item->auto_margin_main_end = share;
	}
	main_size += free_main_size;
	return true;
}
""" + text[end:]
        replace_once("\tdistribute_main_auto_margins(free_main_size);\n\tfree_main_size = container_main_size - main_size;",
                     "\tif (distribute_main_auto_margins(free_main_size)) free_main_size = 0;")
        replace_once("\tpixel_t distribute_step = 1;\n\tfor(auto &item : items)\n\t{", """
	pixel_t distribute_step = 1;
	bool first_item = true;
	for(auto &item : items)
	{
		if (!first_item) main_pos += main_gap;
		first_item = false;""")
    elif path.name == "render_block.cpp":
        replace_once("\t\t_render_content(x, y, true, self_size.new_width(m_pos.width), fmt_ctx);", """\
		// new_width() takes the CSS width, while m_pos.width is already the content width.
		_render_content(x, y, true, self_size.new_width(m_pos.width + self_size.width - self_size.render_width), fmt_ctx);""")
        replace_once("if(!(containing_block_size.size_mode & containing_block_context::size_mode_content))",
                     "if (!(containing_block_size.size_mode & containing_block_context::size_mode_content) ||\n"
                     "\t\t((containing_block_size.size_mode & containing_block_context::size_mode_grid_measure_width) &&\n"
                     "\t\t self_size.width.type == containing_block_context::cbc_value_type_absolute))")
        insert_before("\t// Fix height with min-height attribute", """\
	if (is_grid_item() && self_size.min_height.type != containing_block_context::cbc_value_type_none &&
		self_size.max_height.type != containing_block_context::cbc_value_type_none)
		self_size.max_height.value = std::max(self_size.max_height.value, self_size.min_height.value);

""")
    elif path.name == "render_image.cpp":
        insert_before('#include "render_image.h"', '#include "formats/html/litehtml_render_grid.h"\n')
        replace_once("    calc_outlines(parent_width);\n\n", "")
        anchor = "    src_el()->get_content_size(sz, containing_block_size.width);\n"
        replace_once(anchor, anchor + """\

	if (is_grid_item())
	{
		const auto used = size_grid_image(*this, self_size, sz);
		m_pos.width = used.width;
		m_pos.height = used.height;
		m_pos.move_to(x + content_offset_left(), y + content_offset_top());
		src_el()->css_w().line_height_w().computed_value = height();
		return {width(), width()};
	}

""")
    elif path.name in ("render_flex.h", "render_flex.cpp"):
        if path.name == "render_flex.h":
            replace_once("pixel_t container_main_size, bool single_line);",
                         "pixel_t container_main_size, bool single_line, pixel_t main_gap, bool second_pass);")
        else:
            replace_once("bool /*second_pass*/", "bool second_pass")
            replace_once("bool single_line)\n{", "bool single_line, pixel_t main_gap, bool second_pass)\n{")
            start = text.index("\tbool single_line = css().get_flex_wrap()")
            end = text.index("\n\t/////////////////////////////////////////////////////////////////", start)
            text = text[:start] + """\
	const auto clamp_axis = [](pixel_t size, const auto& min_size, const auto& max_size) {
		if (min_size.type != containing_block_context::cbc_value_type_none)
			size = std::max(size, min_size.value);
		if (max_size.type != containing_block_context::cbc_value_type_none)
			size = std::min(size, max_size.value);
		return std::max<pixel_t>(0, size);
	};
	const bool definite_width = self_size.width.type != containing_block_context::cbc_value_type_auto ||
		(css().get_display() == display_flex && css().get_float() == float_none && src_el()->in_normal_flow());
	const bool definite_height = self_size.height.type == containing_block_context::cbc_value_type_absolute ||
		(self_size.height.type == containing_block_context::cbc_value_type_percentage && self_size.height > 0);
	const pixel_t content_width = clamp_axis(self_size.render_width, self_size.min_width, self_size.max_width);
	const pixel_t content_height = clamp_axis(self_size.height - box_sizing_height(), self_size.min_height, self_size.max_height);
	// Cyclic percentage gutters resolve to zero in Flexbox.
	const pixel_t row_gap = css().get_row_gap().calc_percent(definite_height ? content_height : 0);
	const pixel_t column_gap = css().get_column_gap().calc_percent(definite_width ? content_width : 0);
	const pixel_t main_gap = is_row_direction ? column_gap : row_gap;
	const pixel_t cross_gap = is_row_direction ? row_gap : column_gap;

	const bool auto_main_height = !is_row_direction && !definite_height;
	const bool has_max_height = self_size.max_height.type != containing_block_context::cbc_value_type_none;
	const bool single_line = css().get_flex_wrap() == flex_wrap_nowrap || (auto_main_height && !has_max_height);
	if (!is_row_direction)
		container_main_size = auto_main_height && has_max_height ? std::max<pixel_t>(0, self_size.max_height) : content_height;
""" + text[end:]
            replace_once("\tm_lines = get_lines(self_size, fmt_ctx, is_row_direction, container_main_size, single_line);", """
	m_lines = get_lines(self_size, fmt_ctx, is_row_direction, container_main_size, single_line, main_gap, second_pass);
	if (m_lines.empty()) return {0, 0};
	if (auto_main_height)
	{
		const auto longest = std::max_element(m_lines.begin(), m_lines.end(), [](const flex_line& a, const flex_line& b) {
			return a.main_size < b.main_size;
		});
		container_main_size = clamp_axis(longest->main_size, self_size.min_height, self_size.max_height);
	}""".lstrip("\n"))
            replace_once("\tpixel_t sum_cross_size = 0;", "\tpixel_t sum_cross_size = cross_gap * static_cast<pixel_t>(m_lines.size() - 1);")
            replace_once("\tpixel_t sum_main_size = 0;\n", "")
            replace_once("\tpixel_t ret_width = 0;", """
	// Natural row width measures the items together, including gaps between items on different lines.
	pixel_t ret_width = is_row_direction ? main_gap * static_cast<pixel_t>(m_lines.size() - 1) : 0;""".lstrip("\n"))
            replace_once("\t\tln.init(container_main_size, fit_container, is_row_direction, self_size, fmt_ctx);",
                         "\t\tln.init(container_main_size, is_row_direction, self_size, fmt_ctx);")
            replace_once("\t\tsum_main_size = std::max(sum_main_size, ln.main_size);\n", "")
            replace_once("\tif(container_main_size == 0)\n\t{\n\t\tcontainer_main_size = sum_main_size;\n\t}\n", "")
            insert_before("\t// calculate the final position", """\
	// Auto height includes the full line stack, even when items do not stretch to the line edges.
	if (is_row_direction && !definite_height)
		m_pos.height = std::max(m_pos.height, sum_cross_size);
	else if (auto_main_height)
		m_pos.height = std::max(m_pos.height, container_main_size);

""")
            replace_once("\t\tif (self_size.height.type != containing_block_context::cbc_value_type_auto)\n\t\t{\n\t\t\tpixel_t height = self_size.height - box_sizing_height();\n\t\t\tfree_cross_size = height - sum_cross_size;\n\t\t}", """
		if (definite_height)
			free_cross_size = content_height - sum_cross_size;""".lstrip("\n"))
            start = text.index("\tif(css().get_flex_align_content() == flex_align_content_stretch && free_cross_size > 0)")
            end = text.index("\n\t/// Reverse lines", start)
            text = text[:start] + """\
	if (align_content == flex_align_content_stretch && free_cross_size > 0)
	{
		const pixel_t share = free_cross_size / static_cast<pixel_t>(m_lines.size());
		for (auto& line : m_lines) line.cross_size += share;
		free_cross_size = 0;
	}
""" + text[end:]
            replace_once("\tfor(auto &ln : m_lines)\n\t{\n\t\tline_pos += add_before_line;", """
	bool first_line = true;
	for(auto &ln : m_lines)
	{
		if (!first_line) line_pos += cross_gap;
		first_line = false;
		line_pos += add_before_line;""".lstrip("\n"))
            insert_before("\t\tstd::shared_ptr<flex_item> item = nullptr;", """
		// Positioned children keep their render boxes but never occupy a flex slot.
		if (!el->src_el()->in_normal_flow())
		{
			if (!second_pass && el->css().get_display() != display_none)
			{
				const auto natural_width = el->render(0, 0, self_size, fmt_ctx).natural_width;
				if (natural_width < el->width() && el->css().get_width().is_predefined())
					el->render(0, 0, self_size.new_width(natural_width), fmt_ctx);
			}
			continue;
		}
""")
            replace_once("line.main_size + item->main_size > container_main_size",
                         "line.main_size + main_gap + item->main_size > container_main_size")
            insert_before("\t\tline.base_size += item->base_size;", """
		line.main_gap = main_gap;
		if (!line.items.empty())
		{
			line.base_size += main_gap;
			line.main_size += main_gap;
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
    elif header_path.stem in ("style", "string_id", "types", "css_length", "css_tokenizer", "document_container",
                              "css_properties", "font_description",
                              "render_flex", "render_item", "flex_line"):
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
        if source_path.as_posix() in ("src/style", "src/html_tag", "src/css_properties", "src/el_text",
                                      "src/css_parser", "src/css_length", "src/document", "src/element",
                                      "src/render_block", "src/render_flex", "src/render_image", "src/render_item",
                                      "src/flex_item", "src/flex_line"):
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
