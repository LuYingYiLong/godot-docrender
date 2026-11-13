#include "html_markup.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <gumbo.h>
#include <litehtml/css_parser.h>

#include <memory>
#include <string>
#include <vector>

using namespace godot;

namespace docrender {
	namespace {
		struct GumboDeleter {
			void operator()(GumboOutput* p_output) const {
				gumbo_destroy_output(&kGumboDefaultOptions, p_output);
			}
		};

		using GumboTree = std::unique_ptr<GumboOutput, GumboDeleter>;

		const GumboNode* child(const GumboElement& p_element, size_t p_index) {
			return static_cast<const GumboNode*>(p_element.children.data[p_index]);
		}

		void add_image_path(HashSet<String>& r_paths, const char* p_src, const String& p_base_dir) {
			const String path = resolve_html_image_path(String::utf8(p_src), p_base_dir);
			if (!path.is_empty()) {
				r_paths.insert(path);
			}
		}

		void collect_css_paths(const char* p_css, const String& p_base_dir, HashSet<String>& r_paths) {
			using namespace litehtml;
			const css_token_vector tokens = tokenize(p_css);
			for (size_t i = 0; i < tokens.size(); ++i) {
				const css_token& token = tokens[i];
				if (token.type == URL) {
					add_image_path(r_paths, token.str.c_str(), p_base_dir);
				}
				else if (token.type == FUNCTION && String::utf8(token.str.c_str()).nocasecmp_to("url") == 0) {
					size_t argument = i + 1;
					while (argument < tokens.size() && tokens[argument].type == WHITESPACE) {
						++argument;
					}
					if (argument < tokens.size() && tokens[argument].type == STRING) {
						add_image_path(r_paths, tokens[argument].str.c_str(), p_base_dir);
					}
				}
			}
		}
	}

	std::optional<String> normalize_html_resource_path(const String& p_path) {
		if (!p_path.begins_with("res://")) {
			return std::nullopt;
		}
		const String relative = p_path.substr(6).replace("\\", "/");
		if (relative.begins_with("/") || relative.contains(":") || relative.contains("?") || relative.contains("#")) {
			return std::nullopt;
		}

		PackedStringArray components;
		for (const String& component : relative.split("/", false)) {
			if (component == "..") {
				if (components.is_empty()) {
					return std::nullopt;
				}
				components.remove_at(components.size() - 1);
			}
			else if (component != ".") {
				// Windows may normalize trailing spaces and periods while opening paths.
				if (component.ends_with(" ") || component.ends_with(".")) {
					return std::nullopt;
				}
				components.append(component);
			}
		}
		return "res://" + String("/").join(components);
	}

	String resolve_html_image_path(const String& p_src, const String& p_base_dir) {
		const String source = p_src.strip_edges().uri_decode().replace("\\", "/");
		if (source.is_empty() || source.begins_with("/")) {
			return {};
		}

		const bool absolute = source.begins_with("res://");
		const std::optional<String> path = normalize_html_resource_path(absolute ? source : p_base_dir.path_join(source));
		if (!path) {
			return {};
		}
		const String extension = path->get_extension().to_lower();
		if (extension != "png" && extension != "jpg" && extension != "jpeg" && extension != "webp" && extension != "svg") {
			return {};
		}
		return *path;
	}

	HTMLInlineColors parse_html_inline_colors(const String& p_style) {
		using namespace litehtml;
		HTMLInlineColors colors;
		css_parser parser(tokenize(p_style.utf8().get_data()));
		raw_declaration::vector declarations;
		raw_rule::vector rules;
		parser.consume_style_block_contents(declarations, rules);
		bool important_text = false;
		bool important_background = false;
		for (const raw_declaration& declaration : declarations) {
			const String name = String::utf8(declaration.name.c_str()).to_lower();
			if (name != "color" && name != "background-color") {
				continue;
			}
			css_token_vector value = declaration.value;
			remove_whitespace(value);
			web_color color;
			if (value.size() != 1 || !parse_color(value.front(), color, nullptr) || color.is_current_color) {
				continue;
			}
			bool& important = name == "color" ? important_text : important_background;
			if (important && !declaration.important) {
				continue;
			}
			important = declaration.important;
			auto& target = name == "color" ? colors.text : colors.background;
			target = Color(color.red / 255.0f, color.green / 255.0f, color.blue / 255.0f, color.alpha / 255.0f);
		}
		return colors;
	}

	String decode_html_fragment(const String& p_fragment) {
		if (!p_fragment.contains("%")) {
			return p_fragment;
		}
		const auto hex_digit = [](char p_char) {
			if (p_char >= '0' && p_char <= '9') {
				return p_char - '0';
			}
			if (p_char >= 'a' && p_char <= 'f') {
				return p_char - 'a' + 10;
			}
			return p_char >= 'A' && p_char <= 'F' ? p_char - 'A' + 10 : -1;
		};
		const CharString utf8 = p_fragment.utf8();
		std::string bytes;
		bytes.reserve(utf8.length());
		for (int64_t i = 0; i < utf8.length(); ++i) {
			if (utf8[i] == '%' && i + 2 < utf8.length()) {
				const int high = hex_digit(utf8[i + 1]);
				const int low = hex_digit(utf8[i + 2]);
				if (high >= 0 && low >= 0 && (high != 0 || low != 0)) {
					bytes.push_back(static_cast<char>(high * 16 + low));
					i += 2;
					continue;
				}
			}
			bytes.push_back(utf8[i]);
		}
		String decoded;
		return decoded.parse_utf8(bytes.data(), bytes.size()) == OK ? decoded : p_fragment;
	}

	String decode_html_entities(const String& p_text, bool p_attribute) {
		if (!p_text.contains("&")) {
			return p_text;
		}
		const String html = p_attribute ? "<span data-value=\"" + p_text.replace("\"", "&quot;") + "\"></span>" :
			"<textarea>x" + p_text.replace("<", "&lt;") + "</textarea>";
		const CharString bytes = html.utf8();
		const GumboTree tree(gumbo_parse_with_options(&kGumboDefaultOptions, bytes.get_data(), bytes.length()));
		if (!tree || !tree->root) {
			return p_text;
		}
		const GumboElement& root = tree->root->v.element;
		if (root.children.length < 2) {
			return p_text;
		}
		const GumboElement& body = child(root, 1)->v.element;
		if (body.children.length == 0) {
			return p_text;
		}
		const GumboElement& element = child(body, 0)->v.element;
		if (p_attribute) {
			const GumboAttribute* attribute = gumbo_get_attribute(&element.attributes, "data-value");
			return attribute ? String::utf8(attribute->value) : p_text;
		}
		return element.children.length ? String::utf8(child(element, 0)->v.text.text).substr(1) : p_text;
	}

	Dictionary collect_html_textures(const String& p_html, const String& p_base_dir) {
		if (p_html.is_empty()) {
			return {};
		}
		HashSet<String> paths;
		const CharString bytes = p_html.utf8();
		const GumboTree tree(gumbo_parse_with_options(&kGumboDefaultOptions, bytes.get_data(), bytes.length()));
		if (!tree || !tree->root) {
			return {};
		}
		std::vector<const GumboNode*> nodes{ tree->root };
		while (!nodes.empty()) {
			const GumboNode* node = nodes.back();
			nodes.pop_back();
			if (node->type != GUMBO_NODE_ELEMENT && node->type != GUMBO_NODE_TEMPLATE) {
				continue;
			}
			const GumboElement& element = node->v.element;
			if (element.tag == GUMBO_TAG_IMG) {
				if (const GumboAttribute* src = gumbo_get_attribute(&element.attributes, "src")) {
					add_image_path(paths, src->value, p_base_dir);
				}
			}
			if (const GumboAttribute* style = gumbo_get_attribute(&element.attributes, "style")) {
				collect_css_paths(style->value, p_base_dir, paths);
			}
			for (size_t i = 0; i < element.children.length; ++i) {
				const GumboNode* nested = child(element, i);
				if (element.tag == GUMBO_TAG_STYLE && (nested->type == GUMBO_NODE_TEXT || nested->type == GUMBO_NODE_CDATA)) {
					collect_css_paths(nested->v.text.text, p_base_dir, paths);
				}
				nodes.push_back(nested);
			}
		}

		Dictionary textures;
		ResourceLoader* loader = ResourceLoader::get_singleton();
		for (const String& path : paths) {
			if (loader->exists(path, "Texture2D")) {
				const Ref<Texture2D> texture = loader->load(path, "Texture2D");
				if (texture.is_valid()) {
					textures[path] = texture;
				}
			}
		}
		return textures;
	}
}
