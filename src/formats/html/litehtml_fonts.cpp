#include "litehtml_fonts.h"

#include <godot_cpp/classes/font_variation.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/system_font.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <utility>

using namespace godot;
using namespace litehtml;

namespace docrender {
	namespace {
		struct CaseFoldRange {
			char32_t first;
			char32_t last;
			int32_t offset;
			int step;
		};

		struct CaseFoldExpansion {
			char32_t codepoint;
			const char32_t* folded;
		};

		#include "unicode_case_folding.inc"

		String fold_font_family(const String& p_name) {
			std::u32string folded;
			folded.reserve(p_name.length());
			for (int64_t index = 0; index < p_name.length(); ++index) {
				const char32_t codepoint = p_name[index];
				const auto* range = std::lower_bound(std::begin(CASE_FOLD_RANGES), std::end(CASE_FOLD_RANGES), codepoint,
					[](const CaseFoldRange& p_range, char32_t p_value) { return p_range.last < p_value; });
				if (range != std::end(CASE_FOLD_RANGES) && codepoint >= range->first && (codepoint - range->first) % range->step == 0) {
					folded += static_cast<char32_t>(static_cast<int32_t>(codepoint) + range->offset);
					continue;
				}
				const auto* expansion = std::lower_bound(std::begin(CASE_FOLD_EXPANSIONS), std::end(CASE_FOLD_EXPANSIONS), codepoint,
					[](const CaseFoldExpansion& p_expansion, char32_t p_value) { return p_expansion.codepoint < p_value; });
				if (expansion != std::end(CASE_FOLD_EXPANSIONS) && expansion->codepoint == codepoint) {
					folded += expansion->folded;
				}
				else {
					folded += codepoint;
				}
			}
			return String(folded.c_str());
		}

	} // namespace

	LiteHTMLFonts::LiteHTMLFonts(const HTMLFontConfig& p_fonts) :
		fonts(p_fonts), default_font(p_fonts.text) {
		ThemeDB* themes = ThemeDB::get_singleton();
		const Ref<Theme> project_theme = themes->get_project_theme();
		if (default_font.is_null() && project_theme.is_valid()) {
			default_font = project_theme->get_default_font();
		}
		if (default_font.is_null()) {
			default_font = themes->get_fallback_font();
		}
	}

	HTMLTextLayout LiteHTMLFonts::FontData::shape(const String& p_text) {
		if (const Ref<TextLine>* cached = lines.getptr(p_text)) {
			return { *cached, ascent };
		}
		Ref<TextLine> line;
		line.instantiate();
		line->add_string(p_text, font, size);
		lines.insert(p_text, line);
		return { line, ascent };
	}

	Ref<Font> LiteHTMLFonts::find_installed_font(const String& p_family, int p_weight, bool p_italic) {
		if (p_family.is_empty()) {
			return {};
		}
		const String folded = fold_font_family(p_family);
		const String key = vformat("%d/%d/%s", p_weight, int(p_italic), folded);
		if (const auto* cached = installed_fonts.getptr(key)) {
			return *cached;
		}
		OS* os = OS::get_singleton();
		// Godot aliases these names on some platforms. A quoted CSS family must be a real family.
		const bool platform_alias = folded == "serif" || folded == "sans-serif" || folded == "monospace" ||
			folded == "cursive" || folded == "fantasy";
		if (!installed_font_names) {
			installed_font_names.emplace();
			for (const String& name : os->get_system_fonts()) {
				const String name_key = fold_font_family(name);
				if (!installed_font_names->has(name_key)) {
					installed_font_names->insert(name_key, name);
				}
			}
		}
		const String* listed_family = installed_font_names->getptr(folded);
		const bool exact_lookup = !platform_alias && os->get_name() == "Windows";
		Ref<Font> result;
		if (listed_family || !platform_alias) {
			Ref<SystemFont> font;
			font.instantiate();
			font->set_font_weight(std::clamp(p_weight, 100, 999));
			font->set_font_italic(p_italic);
			font->set_allow_system_fallback(false);
			PackedStringArray names;
			names.append(listed_family ? *listed_family : p_family);
			font->set_font_names(names);
			if (font->get_face_count() > 0 && (exact_lookup || (listed_family && !platform_alias) ||
				folded == fold_font_family(font->get_font_name()))) {
				result = font;
			}
		}
		installed_fonts.insert(key, result);
		return result;
	}

	Ref<Font> LiteHTMLFonts::resolve_font_stack(const font_description& p_description) {
		const bool bold = p_description.weight >= 600;
		const bool italic = p_description.style != font_style_normal;
		const auto with_style = [&](const Ref<Font>& p_font) -> Ref<Font> {
			const bool embolden = bold && p_font->get_font_weight() < 600;
			const bool slant = italic && !(p_font->get_font_style() & TextServer::FONT_ITALIC);
			if (!embolden && !slant) {
				return p_font;
			}
			Ref<FontVariation> variation;
			variation.instantiate();
			variation->set_base_font(p_font);
			if (embolden) {
				variation->set_variation_embolden(0.6f);
			}
			if (slant) {
				variation->set_variation_transform(Transform2D(1.0f, 0.0f, -0.2f, 1.0f, 0.0f, 0.0f));
			}
			variation->set_fallbacks(p_font->get_fallbacks());
			return variation;
		};
		const Ref<Font>& themed_variant = bold && italic ? fonts.bold_italic : (bold ? fonts.bold : fonts.italic);
		const Ref<Font> text_font = (bold || italic) && themed_variant.is_valid() ? themed_variant : with_style(default_font);
		if (p_description.families.empty()) {
			return text_font;
		}

		TypedArray<Font> stack;
		const auto append = [&](const Ref<Font>& p_font) {
			if (p_font.is_valid() && !stack.has(p_font)) {
				stack.append(p_font);
			}
		};
		const auto append_installed = [&](const char* p_names) {
			for (const String& name : String(p_names).split("|")) {
				const auto font = find_installed_font(name, p_description.weight, italic);
				if (font.is_valid()) {
					append(with_style(font));
					break;
				}
			}
		};
		const String text_name = fold_font_family(default_font->get_font_name());
		const String code_name = fonts.code.is_valid() ? fold_font_family(fonts.code->get_font_name()) : String();
		const String block_name = fonts.code_block.is_valid() ? fold_font_family(fonts.code_block->get_font_name()) : String();
		for (const auto& family : p_description.families) {
			const String name = String::utf8(family.name.c_str());
			if (!family.generic) {
				const String folded = fold_font_family(name);
				if (folded.is_empty()) {
					continue;
				}
				if (folded == "docrender-code-block" && fonts.code_block.is_valid()) {
					append(with_style(fonts.code_block));
				}
				else if (folded == text_name) {
					append(text_font);
				}
				else if (folded == code_name) {
					append(with_style(fonts.code));
				}
				else if (folded == block_name) {
					append(with_style(fonts.code_block));
				}
				else if (const auto font = find_installed_font(name, p_description.weight, italic); font.is_valid()) {
					append(with_style(font));
				}
				continue;
			}
			if (name == "sans-serif") {
				append(text_font);
			}
			else if (name == "system-ui" || name == "ui-sans-serif") {
				append_installed("Segoe UI|.AppleSystemUIFont|Helvetica Neue|Roboto|Cantarell|Noto Sans");
			}
			else if (name == "monospace" && fonts.code.is_valid()) {
				append(with_style(fonts.code));
			}
			else {
				static constexpr std::pair<const char*, const char*> generic_fonts[] = {
				{ "serif", "Times New Roman|Noto Serif|DejaVu Serif|Liberation Serif|Times|Droid Serif" },
				{ "monospace", "Courier New|Menlo|Noto Sans Mono|DejaVu Sans Mono|Liberation Mono|Roboto Mono|Droid Sans Mono" },
					{ "cursive", "Comic Sans MS|Apple Chancery|URW Chancery L|Dancing Script" },
					{ "fantasy", "Gabriola|Papyrus|Impact|Copperplate" },
					{ "math", "Cambria Math|STIX Two Math|Latin Modern Math|DejaVu Math TeX Gyre|Noto Sans Math" },
					{ "ui-serif", "New York" },
					{ "ui-monospace", "Cascadia Mono|SF Mono" },
					{ "ui-rounded", "SF Pro Rounded" },
					{ "generic(fangsong)", "FangSong|STFangsong|仿宋" },
					{ "generic(kai)", "KaiTi|STKaiti|Kaiti TC|楷体" },
					{ "generic(khmer-mul)", "Khmer OS Muol|Khmer OS Muol Light" },
					{ "generic(nastaliq)", "Noto Nastaliq Urdu|Jameel Noori Nastaleeq|Urdu Typesetting" },
				};
				for (const auto& [generic, candidates] : generic_fonts) {
					if (name == generic) {
						append_installed(candidates);
						break;
					}
				}
			}
		}
		append(text_font);
		const Ref<Font> primary = stack[0];
		if (stack.size() == 1) {
			return primary;
		}
		Ref<FontVariation> combined;
		combined.instantiate();
		combined->set_base_font(primary);
		stack.remove_at(0);
		const TypedArray<Font> primary_fallbacks = primary->get_fallbacks();
		for (int64_t index = 0; index < primary_fallbacks.size(); ++index) {
			const Ref<Font> fallback = primary_fallbacks[index];
			append(fallback);
		}
		combined->set_fallbacks(stack);
		return combined;
	}

	uint_ptr LiteHTMLFonts::create_font(const font_description& p_description, font_metrics* r_metrics) {
		const double requested_size = std::round(static_cast<double>(p_description.size));
		const int32_t size = std::isfinite(requested_size) && requested_size <= std::numeric_limits<int32_t>::max()
			? static_cast<int32_t>(std::max(1.0, requested_size))
			: DEFAULT_SIZE;
		String key = vformat("%d/%d", p_description.weight, int(p_description.style));
		for (const auto& family : p_description.families) {
			const String name = String::utf8(family.name.c_str());
			key += vformat("/%d/%d/%s", int(family.generic), name.length(), name);
		}
		const auto* cached = font_stacks.getptr(key);
		const Ref<Font> font = cached ? *cached : resolve_font_stack(p_description);
		if (!cached) {
			font_stacks.insert(key, font);
		}

		const float ascent = font->get_ascent(size);
		auto* data = new FontData{ font, size, ascent, p_description,{} };
		r_metrics->font_size = size;
		r_metrics->ascent = ascent;
		r_metrics->descent = font->get_descent(size);
		r_metrics->height = r_metrics->ascent + r_metrics->descent;
		r_metrics->x_height = r_metrics->ascent * 0.65f;
		r_metrics->ch_width = data->shape(String("0")).line->get_line_width();
		r_metrics->draw_spaces = p_description.decoration_line != text_decoration_line_none;
		r_metrics->sub_shift = size * 0.2f;
		r_metrics->super_shift = size * 0.35f;
		return reinterpret_cast<uint_ptr>(data);
	}

	void LiteHTMLFonts::delete_font(uint_ptr p_font) {
		delete reinterpret_cast<FontData*>(p_font);
	}

	pixel_t LiteHTMLFonts::text_width(const char* p_text, uint_ptr p_font) {
		return reinterpret_cast<FontData*>(p_font)->shape(String::utf8(p_text)).line->get_line_width();
	}

	HTMLTextLayout LiteHTMLFonts::shape_text(uint_ptr p_font, const String& p_text) {
		return reinterpret_cast<FontData*>(p_font)->shape(p_text);
	}

	HTMLShapedText LiteHTMLFonts::shape_styled_text(uint_ptr p_font, const String& p_text) {
		FontData& font = *reinterpret_cast<FontData*>(p_font);
		const HTMLTextLayout layout = font.shape(p_text);
		const auto& description = font.description;
		float thickness = 0.0f;
		if (description.decoration_line != text_decoration_line_none) {
			const auto& specified = description.decoration_thickness;
			thickness = layout.line->get_line_underline_thickness();
			if (!specified.is_predefined()) {
				thickness = specified.calc_percent(font.size);
			}
			else if (specified.predef() == text_decoration_thickness_from_font) {
				const float metric = font.font->get_underline_thickness(font.size);
				if (std::isfinite(metric) && metric > 0.0f) {
					thickness = metric;
				}
			}
			thickness = std::max(1.0f, thickness);
		}
		return { layout, description.decoration_line, description.decoration_style,
			description.decoration_color, thickness };
	}

	void transform_html_text(litehtml::string& r_text, text_transform p_transform) {
		String text = String::utf8(r_text.c_str());
		switch (p_transform) {
		case text_transform_uppercase:
			text = text.to_upper();
			break;
		case text_transform_lowercase:
			text = text.to_lower();
			break;
		case text_transform_capitalize:
			text = text.substr(0, 1).to_upper() + text.substr(1);
			break;
		default:
			return;
		}
		r_text = text.utf8().get_data();
	}
} // namespace docrender
