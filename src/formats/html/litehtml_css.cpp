#include "litehtml_css.h"

#include <litehtml/html.h>
#include <litehtml/css_parser.h>
#include <litehtml/style.h>

namespace litehtml {
	namespace {
		using alignment = alignment_keyword;

		constexpr std::pair<alignment, const char*> alignment_words[] = {
			{ alignment::normal, "normal" }, { alignment::automatic, "auto" }, { alignment::stretch, "stretch" },
			{ alignment::start, "start" }, { alignment::end, "end" }, { alignment::center, "center" },
			{ alignment::self_start, "self-start" }, { alignment::self_end, "self-end" },
			{ alignment::flex_start, "flex-start" }, { alignment::flex_end, "flex-end" },
			{ alignment::left, "left" }, { alignment::right, "right" },
			{ alignment::space_between, "space-between" }, { alignment::space_around, "space-around" },
			{ alignment::space_evenly, "space-evenly" }, { alignment::first_baseline, "baseline" },
			{ alignment::last_baseline, "baseline" },
		};

		const char* alignment_word(css_alignment p_alignment) {
			for (const auto& [keyword, word] : alignment_words)
				if (keyword == p_alignment.keyword) return word;
			return "normal";
		}
	}

	css_alignment initial_alignment(string_id p_property) {
		return { is_one_of(p_property, _justify_self_, _align_self_) ? alignment::automatic : alignment::normal };
	}

	bool parse_alignment(string_id p_property, const css_token_vector& p_tokens, css_alignment& r_alignment) {
		if (p_tokens.empty() || p_tokens.size() > 2) return false;
		const bool content = is_one_of(p_property, _justify_content_, _align_content_);
		const bool justify = is_one_of(p_property, _justify_items_, _justify_self_, _justify_content_);
		const auto word = p_tokens.back().ident();
		auto prefix = p_tokens.size() == 2 ? p_tokens.front().ident() : string();
		const auto keyword_word = prefix == "baseline" && is_one_of(word, "first", "last") ? prefix : word;
		if (keyword_word != word) prefix = word;
		const auto found = std::find_if(std::begin(alignment_words), std::end(alignment_words),
			[&](const auto& entry) { return keyword_word == entry.second; });
		if (found == std::end(alignment_words)) return false;
		auto keyword = found->first;
		auto overflow = alignment_overflow::automatic;
		if (keyword == alignment::automatic && !is_one_of(p_property, _justify_self_, _align_self_)) return false;
		if (is_one_of(keyword, alignment::self_start, alignment::self_end) && content) return false;
		if (is_one_of(keyword, alignment::left, alignment::right) && !justify) return false;
		if (is_one_of(keyword, alignment::space_between, alignment::space_around, alignment::space_evenly) && !content) return false;
		if (keyword == alignment::first_baseline && p_property == _justify_content_) return false;

		if (p_tokens.size() == 2) {
			if (keyword == alignment::first_baseline && is_one_of(prefix, "first", "last")) {
				keyword = prefix == "last" ? alignment::last_baseline : alignment::first_baseline;
			} else if (is_one_of(prefix, "safe", "unsafe") &&
				((keyword >= alignment::start && keyword <= alignment::right) ||
					(keyword == alignment::normal && is_one_of(p_property, _justify_self_, _align_self_)))) {
				overflow = prefix == "safe" ? alignment_overflow::safe : alignment_overflow::unsafe;
			} else {
				return false;
			}
		}
		r_alignment = { keyword, overflow };
		return true;
	}

	flex_align_items to_flex_item_alignment(css_alignment p_alignment) {
		int value = value_index(alignment_word(p_alignment), flex_align_items_strings);
		if (p_alignment.keyword == alignment::last_baseline) value |= flex_align_items_last;
		if (p_alignment.overflow == alignment_overflow::safe) value |= flex_align_items_safe;
		if (p_alignment.overflow == alignment_overflow::unsafe) value |= flex_align_items_unsafe;
		return static_cast<flex_align_items>(value);
	}

	flex_align_content to_flex_align_content(css_alignment p_alignment) {
		const int value = value_index(alignment_word(p_alignment), flex_align_content_strings);
		return value < 0 ? flex_align_content_stretch : static_cast<flex_align_content>(value);
	}

	flex_justify_content to_flex_justify_content(css_alignment p_alignment) {
		return static_cast<flex_justify_content>(value_index(alignment_word(p_alignment), flex_justify_content_strings));
	}

	bool parse_font_families(const css_token_vector& p_tokens, css_font_families& r_families) {
		const auto is_generic = [](const string& p_name) {
			return is_one_of(p_name, "serif", "sans-serif", "monospace", "cursive", "fantasy", "system-ui", "math",
				"ui-serif", "ui-sans-serif", "ui-monospace", "ui-rounded");
		};
		const auto is_reserved = [&](const string& p_name) {
			return is_generic(p_name) || value_in_list(p_name, font_system_family_name_strings) ||
				is_one_of(p_name, "default", "initial", "inherit", "unset", "revert", "revert-layer");
		};
		css_font_families families;
		for (const auto& tokens : parse_comma_separated_list(p_tokens)) {
			if (tokens.empty()) {
				return false;
			}
			const auto& first = tokens.front();
			if (tokens.size() == 1) {
				if (first.type == STRING) {
					families.push_back({ first.str });
					continue;
				}
				if (first.type == CV_FUNCTION && lowcase(first.name) == "generic" && first.value.size() == 1) {
					const auto keyword = first.value.front().ident();
					if (is_one_of(keyword, "fangsong", "kai", "khmer-mul", "nastaliq")) {
						families.push_back({ "generic(" + keyword + ")", true });
						continue;
					}
				}
				const auto keyword = first.ident();
				if (is_generic(keyword)) {
					families.push_back({ keyword, true });
					continue;
				}
			}
			string name;
			for (const auto& token : tokens) {
				if (token.type != IDENT || is_reserved(lowcase(token.name))) {
					return false;
				}
				if (!name.empty()) {
					name += ' ';
				}
				name += token.name;
			}
			families.push_back({ std::move(name) });
		}
		r_families = std::move(families);
		return true;
	}

	void css_declaration_result::accept(string_id p_name, const property_value& p_value) {
		accepted = true;
		switch (p_name) {
			case _border_top_style_:
			case _border_right_style_:
			case _border_bottom_style_:
			case _border_left_style_:
				if (p_value.is<int>() && p_value.get<int>() >= border_style_groove) {
					drawing_values.emplace_back(p_name, p_value.get<int>());
				}
				break;
			case _text_emphasis_style_:
				if (p_value.is<string>() && lowcase(p_value.get<string>()) != "none") {
					drawing_values.emplace_back(p_name, 0);
				}
				break;
			default:
				break;
		}
	}

	bool parse_decoration_line(const css_token_vector& p_tokens, int& r_line) {
		if (p_tokens.size() == 1 && p_tokens.front().ident() == "none") {
			r_line = text_decoration_line_none;
			return true;
		}
		int lines = 0;
		for (const auto& token : p_tokens) {
			const int index = value_index(token.ident(), style_text_decoration_line_strings);
			if (index <= 0) {
				return false;
			}
			const int line = 1 << (index - 1);
			if (lines & line) {
				return false;
			}
			lines |= line;
		}
		r_line = lines;
		return lines != 0;
	}

	bool parse_decoration(const css_token_vector& p_tokens, document_container* p_container, text_decoration& r_decoration) {
		css_token_vector line_tokens;
		bool has_style = false;
		bool has_thickness = false;
		bool has_color = false;
		for (const auto& token : p_tokens) {
			const int style = value_index(token.ident(), style_text_decoration_style_strings);
			if (!has_style && style >= 0) {
				r_decoration.style = static_cast<text_decoration_style>(style);
				has_style = true;
			}
			else if (!has_thickness && r_decoration.thickness.from_token(token,
					f_length_percentage | f_positive, style_text_decoration_thickness_strings)) {
				has_thickness = true;
			}
			else if (!has_color && parse_color(token, r_decoration.color, p_container)) {
				has_color = true;
			}
			else {
				line_tokens.push_back(token);
			}
		}
		return !p_tokens.empty() && (line_tokens.empty() || parse_decoration_line(line_tokens, r_decoration.line));
	}
}
