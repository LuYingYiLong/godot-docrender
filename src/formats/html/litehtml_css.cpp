#include "litehtml_css.h"

#include <litehtml/html.h>
#include <litehtml/style.h>

namespace litehtml {
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
