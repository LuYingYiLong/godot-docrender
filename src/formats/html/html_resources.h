#pragma once

#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <string>
#include <string_view>

namespace docrender {
	class HTMLResources {
		godot::String base_dir;
		godot::Dictionary textures;
		godot::Dictionary local_stylesheets;
		godot::HashMap<godot::String, godot::Ref<godot::Texture2D>> images;
		int stylesheet_reads = 0;

		godot::String get_url_dir(const char* p_css_base) const;

	public:
		HTMLResources(const godot::String& p_base_dir, const godot::Dictionary& p_textures,
			const godot::Dictionary& p_local_stylesheets);

		static std::string_view stylesheet_path(std::string_view p_css_base);
		void reset_stylesheet_reads();
		void import_css(std::string& r_text, const std::string& p_url, std::string& r_css_base);
		godot::Ref<godot::Texture2D> load_texture(const char* p_src, const char* p_css_base);
	};
}
