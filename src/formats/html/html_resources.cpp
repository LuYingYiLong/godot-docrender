#include "html_resources.h"

#include "html_markup.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <optional>

using namespace godot;

namespace docrender {
	HTMLResources::HTMLResources(const String& p_base_dir, const Dictionary& p_textures, const Dictionary& p_local_stylesheets) :
		base_dir(p_base_dir), textures(p_textures), local_stylesheets(p_local_stylesheets) {
	}

	std::string_view HTMLResources::stylesheet_path(std::string_view p_css_base) {
		const auto separator = p_css_base.find_last_of('\n');
		return separator == std::string_view::npos ? p_css_base : p_css_base.substr(separator + 1);
	}

	void HTMLResources::reset_stylesheet_reads() {
		stylesheet_reads = 0;
	}

	String HTMLResources::get_url_dir(const char* p_css_base) const {
		if (!p_css_base || !*p_css_base) {
			return base_dir;
		}
		const auto path = stylesheet_path(p_css_base);
		return String::utf8(path.data(), path.size()).get_base_dir();
	}

	void HTMLResources::import_css(std::string& r_text, const std::string& p_url, std::string& r_css_base) {
		r_text.clear();
		const String path = resolve_html_stylesheet_path(String::utf8(p_url.c_str()), get_url_dir(r_css_base.c_str()));
		const auto parents = String::utf8(r_css_base.c_str()).split("\n", false);
		// litehtml carries this base string into nested imports and declarations.
		if (path.is_empty() || parents.has(path) || parents.size() >= 16 || stylesheet_reads >= 128) {
			return;
		}
		++stylesheet_reads;
		const Variant stored = local_stylesheets.get(path, Variant());
		const auto file = stored.get_type() == Variant::STRING ? std::optional<String>(stored) : read_html_stylesheet(path);
		if (!file) {
			return;
		}
		if (!r_css_base.empty()) {
			r_css_base += '\n';
		}
		r_css_base += path.utf8().get_data();
		r_text = file->utf8().get_data();
	}

	Ref<Texture2D> HTMLResources::load_texture(const char* p_src, const char* p_css_base) {
		const String path = resolve_html_image_path(String::utf8(p_src), get_url_dir(p_css_base));
		if (path.is_empty()) {
			return {};
		}
		if (const Ref<Texture2D>* cached = images.getptr(path)) {
			return *cached;
		}

		Ref<Texture2D> texture = textures.get(path, Variant());
		if (texture.is_valid()) {
			return texture;
		}
		ResourceLoader* loader = ResourceLoader::get_singleton();
		if (loader->exists(path, "Texture2D")) {
			texture = loader->load(path, "Texture2D");
		}
		images.insert(path, texture);
		return texture;
	}
} // namespace docrender
