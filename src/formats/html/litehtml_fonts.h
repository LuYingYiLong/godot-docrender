#pragma once

#include "html_font_config.h"
#include "html_shaped_text.h"

#include <godot_cpp/templates/hash_map.hpp>
#include <litehtml/font_description.h>

#include <optional>

namespace docrender {
	class LiteHTMLFonts {
		struct FontData {
			godot::Ref<godot::Font> font;
			int32_t size;
			float ascent;
			litehtml::font_description description;
			godot::HashMap<godot::String, godot::Ref<godot::TextLine>> lines;

			HTMLTextLayout shape(const godot::String& p_text);
		};

		HTMLFontConfig fonts;
		godot::Ref<godot::Font> default_font;
		godot::HashMap<godot::String, godot::Ref<godot::Font>> font_stacks;
		godot::HashMap<godot::String, godot::Ref<godot::Font>> installed_fonts;
		std::optional<godot::HashMap<godot::String, godot::String>> installed_font_names;

		godot::Ref<godot::Font> resolve_font_stack(const litehtml::font_description& p_description);
		godot::Ref<godot::Font> find_installed_font(const godot::String& p_family, int p_weight, bool p_italic);

	public:
		static constexpr int32_t DEFAULT_SIZE = 16;
		static constexpr const char* DEFAULT_FAMILY = "sans-serif";

		explicit LiteHTMLFonts(const HTMLFontConfig& p_fonts);

		litehtml::uint_ptr create_font(const litehtml::font_description& p_description, litehtml::font_metrics* r_metrics);
		void delete_font(litehtml::uint_ptr p_font);
		litehtml::pixel_t text_width(const char* p_text, litehtml::uint_ptr p_font);
		HTMLTextLayout shape_text(litehtml::uint_ptr p_font, const godot::String& p_text);
		HTMLShapedText shape_styled_text(litehtml::uint_ptr p_font, const godot::String& p_text);
	};
}
