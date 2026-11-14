#pragma once

#include <godot_cpp/classes/text_line.hpp>
#include <litehtml/types.h>
#include <litehtml/web_color.h>

namespace docrender {
	struct HTMLTextLayout {
		godot::Ref<godot::TextLine> line;
		float ascent;
	};

	struct HTMLShapedText {
		HTMLTextLayout layout;
		int decoration_line;
		litehtml::text_decoration_style decoration_style;
		litehtml::web_color decoration_color;
		float decoration_thickness;
	};

	void transform_html_text(litehtml::string& r_text, litehtml::text_transform p_transform);
}
