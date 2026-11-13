#pragma once

#include <godot_cpp/classes/text_line.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <litehtml/types.h>

#include <vector>

namespace docrender {
	struct TextDecorationMask {
		godot::Rect2 rect;
		litehtml::text_decoration_style style;
		float alpha;
	};

	struct HTMLTextMask {
		godot::Ref<godot::Texture2D> texture;
		godot::Rect2 bounds;
	};

	HTMLTextMask create_html_text_mask(const godot::TextLine& p_line, const godot::Vector2& p_position,
		const std::vector<TextDecorationMask>& p_decorations, bool p_mipmaps);
}
