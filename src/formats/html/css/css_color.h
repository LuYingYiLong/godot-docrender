#pragma once

namespace litehtml {
	struct css_token;
	struct web_color;

	bool parse_oklch(const css_token& token, web_color& color);
}
