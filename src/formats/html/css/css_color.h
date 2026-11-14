#pragma once

#include <array>

namespace litehtml {
	class document_container;
	struct css_token;
	struct web_color;

	enum class css_color_space { srgb, srgb_linear, oklab, oklch, hsl };

	web_color make_css_color(css_color_space space, std::array<double, 4> components, unsigned missing = 0);
	bool parse_oklch(const css_token& token, web_color& color);
	bool parse_color_mix(const css_token& token, web_color& color, document_container* container, unsigned depth);
}
