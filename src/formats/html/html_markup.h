#pragma once

#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <optional>

namespace docrender {
	struct HTMLInlineColors {
		std::optional<godot::Color> text;
		std::optional<godot::Color> background;
	};

	struct HTMLAssets {
		godot::Dictionary textures;
		godot::Dictionary stylesheets;
	};

	HTMLInlineColors parse_html_inline_colors(const godot::String& p_style);
	godot::String decode_html_entities(const godot::String& p_text, bool p_attribute = false);
	godot::String decode_html_fragment(const godot::String& p_fragment);
	std::optional<godot::String> normalize_html_resource_path(const godot::String& p_path);
	godot::String resolve_html_image_path(const godot::String& p_src, const godot::String& p_base_dir);
	godot::String resolve_html_stylesheet_path(const godot::String& p_src, const godot::String& p_base_dir);
	std::optional<godot::String> read_html_stylesheet(const godot::String& p_path);
	HTMLAssets collect_html_assets(const godot::String& p_html, const godot::String& p_base_dir);
}
