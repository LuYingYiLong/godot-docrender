#pragma once

#include <litehtml/css_length.h>
#include <litehtml/web_color.h>

#include <vector>

namespace litehtml {
	struct box_shadow {
		css_length x;
		css_length y;
		css_length blur;
		css_length spread;
		web_color color = web_color::current_color;
		bool inset = false;
	};

	bool parse_box_shadows(const css_token_vector& p_tokens, document_container* p_container, std::vector<box_shadow>& r_shadows);
	bool parse_text_shadows(const css_token_vector& p_tokens, document_container* p_container, std::vector<box_shadow>& r_shadows);
}
