#pragma once

#include <litehtml/css_length.h>
#include <litehtml/string_id.h>
#include <litehtml/web_color.h>

namespace litehtml {
	struct property_value;
	struct css_font_family {
		string name;
		bool generic = false;
	};
	using css_font_families = std::vector<css_font_family>;

	bool parse_font_families(const css_token_vector& p_tokens, css_font_families& r_families);

	enum class css_source_kind {
		none,
		inline_style,
		style_element,
		stylesheet,
		local_stylesheet
	};
	enum class css_diagnostic_kind {
		unknown_property,
		unsupported_value,
		not_rendered
	};

	struct css_source {
		css_source_kind kind = css_source_kind::none;
		string path;
	};

	struct css_diagnostic {
		css_diagnostic_kind kind;
		string property;
		string value;
		css_source source;
		std::vector<std::pair<string_id, int>> drawing_values;
	};

	struct css_declaration_result {
		bool accepted = false;
		std::vector<std::pair<string_id, int>> drawing_values;

		void accept(string_id p_name, const property_value& p_value);
	};

	struct text_decoration {
		int line = text_decoration_line_none;
		text_decoration_style style = text_decoration_style_solid;
		css_length thickness = css_length::predef_value(text_decoration_thickness_auto);
		web_color color = web_color::current_color;
	};

	bool parse_decoration_line(const css_token_vector& p_tokens, int& r_line);
	bool parse_decoration(const css_token_vector& p_tokens, document_container* p_container, text_decoration& r_decoration);
}
