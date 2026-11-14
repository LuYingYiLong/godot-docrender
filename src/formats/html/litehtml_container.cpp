#include "litehtml_container.h"

#include "litehtml_document_adapter.h"

#include <godot_cpp/classes/translation_server.hpp>
#include <litehtml/document.h>
#include <litehtml/el_image.h>
#include <litehtml/html_tag.h>
#include <litehtml/render_image.h>

#include <algorithm>
#include <string_view>

using namespace godot;
using namespace litehtml;

namespace docrender {
	namespace {
		constexpr float CSS_DPI = 96.0f;

		class RawTextElement final : public html_tag {
			bool stylesheet;

		public:
			RawTextElement(const document::ptr& p_document, bool p_stylesheet) :
				html_tag(p_document), stylesheet(p_stylesheet) {}

			void parse_attributes() override {
				if (stylesheet) {
					std::string text;
					get_text(text);
					get_document()->add_stylesheet(text.c_str(), nullptr, get_attr("media"));
				}
			}

			void compute_styles(bool) override {
				css_w().set_display(display_none);
			}
		};

		class FittedImageRenderItem : public render_item_image {
		protected:
			rendered_width _render(pixel_t p_x, pixel_t p_y, const containing_block_context& p_block,
				formatting_context* p_context, bool p_second_pass) override {
				const rendered_width rendered = render_item_image::_render(p_x, p_y, p_block, p_context, p_second_pass);
				if (m_pos.width <= 0.0f || m_pos.height <= 0.0f) {
					return rendered;
				}
				position viewport;
				src_el()->get_document()->container()->get_viewport(viewport);
				const pixel_t max_width = std::max(0.0f, std::min<pixel_t>(p_block.width, viewport.width) - content_offset_width());
				const pixel_t max_height = std::max(0.0f, viewport.height - content_offset_height());
				const pixel_t scale = std::min({ 1.0f, max_width / m_pos.width, max_height / m_pos.height });
				m_pos.width *= scale;
				m_pos.height *= scale;
				src_el()->css_w().line_height_w().computed_value = height();
				const pixel_t fitted_width = width();
				return { fitted_width, fitted_width };
			}

		public:
			using render_item_image::render_item_image;

			std::shared_ptr<render_item> clone() override {
				return std::make_shared<FittedImageRenderItem>(src_el());
			}
		};

		class FittedImageElement : public el_image {
		public:
			using el_image::el_image;

			std::shared_ptr<render_item> create_render_item(const std::shared_ptr<render_item>& p_parent) override {
				auto item = std::make_shared<FittedImageRenderItem>(shared_from_this());
				item->parent(p_parent);
				return item;
			}
		};

	} // namespace

	LiteHTMLContainer::LiteHTMLContainer(const String& p_base_dir, const Dictionary& p_textures, bool p_fit_images,
		const HTMLFontConfig& p_fonts, const Dictionary& p_local_stylesheets) :
		resources(p_base_dir, p_textures, p_local_stylesheets),
		fit_images(p_fit_images), fonts(p_fonts) {
	}

	litehtml::element::ptr LiteHTMLContainer::create_element(const char* p_tag, const litehtml::string_map& p_attributes,
		const std::shared_ptr<litehtml::document>& p_document) {
		if (!p_attributes.empty()) {
			// The inner call selects litehtml's normal element type. The outer call applies the attributes.
			const auto element = p_document->create_element(p_tag, {});
			if (element) {
				source_attributes.insert(element.get(), SourceAttributes{ element, p_attributes });
			}
			return element;
		}
		const std::string_view tag(p_tag);
		if (tag == "style" || tag == "script") {
			return std::make_shared<RawTextElement>(p_document, tag == "style");
		}
		return fit_images && tag == "img" ? std::make_shared<FittedImageElement>(p_document) : nullptr;
	}

	const litehtml::string_map* LiteHTMLContainer::get_source_attributes(const litehtml::element& p_element) const {
		const auto* source = source_attributes.getptr(&p_element);
		return source && !source->element.expired() ? &source->values : nullptr;
	}

	void LiteHTMLContainer::set_source_attribute(const litehtml::element::ptr& p_element, const char* p_name, const char* p_value) {
		auto* source = source_attributes.getptr(p_element.get());
		if (p_value) {
			if (!source || source->element.expired()) {
				source_attributes.insert(p_element.get(), SourceAttributes{ p_element, {} });
				source = source_attributes.getptr(p_element.get());
			}
			source->values[p_name] = p_value;
		}
		else if (source) {
			source->values.erase(p_name);
		}
	}

	void LiteHTMLContainer::forget_source_attributes(const litehtml::element& p_element) {
		source_attributes.erase(&p_element);
	}

	void LiteHTMLContainer::reset_stylesheet_reads() {
		resources.reset_stylesheet_reads();
	}

	void LiteHTMLContainer::set_css_diagnostics_enabled(bool p_enabled) {
		css_diagnostics_enabled = p_enabled;
		begin_css_diagnostics();
	}

	void LiteHTMLContainer::begin_css_diagnostics() {
		css_diagnostics.clear();
		css_diagnostic_keys.clear();
		element_diagnostics.clear();
		paint.clear_diagnostics();
	}

	void LiteHTMLContainer::set_css_source_kind(litehtml::css_source_kind p_kind) {
		css_source_kind = p_kind;
	}

	TypedArray<Dictionary> LiteHTMLContainer::get_css_diagnostics() const {
		TypedArray<Dictionary> result;
		for (const Dictionary& entry : css_diagnostics) {
			result.append(entry.duplicate());
		}
		return result;
	}

	const std::vector<int64_t>* LiteHTMLContainer::get_element_diagnostics(const element& p_element) const {
		return element_diagnostics.getptr(&p_element);
	}

	const Dictionary& LiteHTMLContainer::get_css_diagnostic(int64_t p_index) const {
		return css_diagnostics[p_index];
	}

	css_source LiteHTMLContainer::get_css_source(const litehtml::string& p_base, bool p_inline) const {
		if (!css_diagnostics_enabled) {
			return {};
		}
		if (p_inline) {
			return { css_source_kind::inline_style, {} };
		}
		if (css_source_kind == css_source_kind::none || p_base.empty()) {
			return { css_source_kind, {} };
		}
		return { css_source_kind::local_stylesheet, std::string(HTMLResources::stylesheet_path(p_base)) };
	}

	void LiteHTMLContainer::report_css_diagnostic(const css_diagnostic& p_diagnostic, const html_tag* p_element) {
		if (!css_diagnostics_enabled) {
			return;
		}
		static constexpr const char* kinds[] = { "unknown_property", "unsupported_value", "not_rendered" };
		static constexpr const char* sources[] = { "", "inline_style", "style_element", "stylesheet", "local_stylesheet" };
		const String kind = kinds[static_cast<int>(p_diagnostic.kind)];
		const String source = sources[static_cast<int>(p_diagnostic.source.kind)];
		const String property = String::utf8(p_diagnostic.property.c_str());
		const String value = String::utf8(p_diagnostic.value.c_str());
		const String path = String::utf8(p_diagnostic.source.path.c_str());
		const String key = vformat("%s/%s/%d/%d/%s%s%s", kind, source, property.length(), path.length(), property, path, value);
		const int64_t* existing = css_diagnostic_keys.getptr(key);
		const int64_t index = existing ? *existing : static_cast<int64_t>(css_diagnostics.size());
		if (!existing) {
			css_diagnostic_keys.insert(key, index);
			Dictionary entry;
			entry["kind"] = kind;
			entry["property"] = property;
			entry["value"] = value;
			entry["source"] = source;
			entry["path"] = path;
			css_diagnostics.push_back(entry);
		}
		if (p_element) {
			auto& entries = element_diagnostics[p_element];
			if (std::find(entries.begin(), entries.end(), index) == entries.end()) {
				entries.push_back(index);
			}
		}
	}

	void LiteHTMLContainer::set_viewport(const Vector2i& p_size) {
		viewport_size = p_size;
	}

	void LiteHTMLContainer::record(litehtml::document& p_document, const Vector2i& p_content_size, const HTMLGeometryIndex& p_geometry) {
		const litehtml::position bounds(0, 0, std::max(p_content_size.x, viewport_size.x),
			std::max(p_content_size.y, viewport_size.y));
		paint.begin(p_geometry);
		p_document.draw(0, 0, 0, &bounds);
		paint.finish(p_document.root(), [this](const element& p_element, const String& p_text) {
			const auto font = p_element.parent()->css().get_font();
			return font ? fonts.shape_text(font, p_text) : HTMLTextLayout{};
		});
	}

	uint_ptr LiteHTMLContainer::create_font(const font_description& p_description, const litehtml::document*,
		font_metrics* r_metrics) {
		return fonts.create_font(p_description, r_metrics);
	}

	void LiteHTMLContainer::delete_font(uint_ptr p_font) {
		fonts.delete_font(p_font);
	}

	pixel_t LiteHTMLContainer::text_width(const char* p_text, uint_ptr p_font) {
		return fonts.text_width(p_text, p_font);
	}

	void LiteHTMLContainer::draw_text(uint_ptr, const char* p_text, uint_ptr p_font, web_color p_color,
		const litehtml::position& p_position) {
		static const std::vector<box_shadow> no_shadows;
		record_text(p_text, p_font, p_color, p_position, no_shadows);
	}

	void LiteHTMLContainer::draw_styled_text(uint_ptr, const char* p_text, uint_ptr p_font, web_color p_color,
		const litehtml::position& p_position, const element& p_element) {
		record_text(p_text, p_font, p_color, p_position, p_element.css().get_text_shadows());
	}

	void LiteHTMLContainer::record_text(const char* p_text, uint_ptr p_font, web_color p_color,
		const litehtml::position& p_position, const std::vector<box_shadow>& p_shadows) {
		const String value = String::utf8(p_text);
		paint.draw_text(fonts.shape_styled_text(p_font, value), value.length(), p_color, p_position, p_shadows);
	}

	pixel_t LiteHTMLContainer::pt_to_px(float p_points) const {
		return p_points * (CSS_DPI / 72.0f);
	}

	pixel_t LiteHTMLContainer::get_default_font_size() const {
		return LiteHTMLFonts::DEFAULT_SIZE;
	}

	const char* LiteHTMLContainer::get_default_font_name() const {
		return LiteHTMLFonts::DEFAULT_FAMILY;
	}

	void LiteHTMLContainer::draw_list_marker(uint_ptr, const list_marker& p_marker) {
		Ref<Texture2D> texture;
		if (!p_marker.image.empty()) {
			texture = resources.load_texture(p_marker.image.c_str(), p_marker.baseurl);
		}
		paint.draw_list_marker(p_marker, texture);
	}

	void LiteHTMLContainer::import_css(litehtml::string& r_text, const litehtml::string& p_url, litehtml::string& r_css_base) {
		resources.import_css(r_text, p_url, r_css_base);
	}

	void LiteHTMLContainer::link(const std::shared_ptr<litehtml::document>& p_doc, const litehtml::element::ptr& p_element) {
		const char* rel = p_element->get_attr("rel");
		const char* href = p_element->get_attr("href");
		if (rel && href && std::string_view(rel) != "stylesheet" && String::utf8(rel).nocasecmp_to("stylesheet") == 0) {
			litehtml::string css;
			litehtml::string base;
			import_css(css, href, base);
			p_doc->add_stylesheet(css.c_str(), base.c_str(), p_element->get_attr("media"));
		}
	}

	void LiteHTMLContainer::load_image(const char* p_src, const char* p_css_base, bool) {
		resources.load_texture(p_src, p_css_base);
	}

	void LiteHTMLContainer::get_image_size(const char* p_src, const char* p_css_base, litehtml::size& r_size) {
		const Ref<Texture2D> texture = resources.load_texture(p_src, p_css_base);
		const Vector2 size = texture.is_valid() ? texture->get_size() : Vector2();
		r_size = litehtml::size(size.x, size.y);
	}

	void LiteHTMLContainer::draw_image(uint_ptr, const background_layer& p_layer, const std::string& p_url, const std::string& p_css_base) {
		paint.draw_image(p_layer, resources.load_texture(p_url.c_str(), p_css_base.c_str()));
	}

	void LiteHTMLContainer::draw_solid_fill(uint_ptr, const background_layer& p_layer, const web_color& p_color) {
		paint.draw_solid_fill(p_layer, p_color);
	}

	void LiteHTMLContainer::draw_linear_gradient(uint_ptr, const background_layer& p_layer, const background_layer::linear_gradient& p_gradient) {
		paint.draw_linear_gradient(p_layer, p_gradient);
	}

	void LiteHTMLContainer::draw_radial_gradient(uint_ptr, const background_layer& p_layer, const background_layer::radial_gradient& p_gradient) {
		paint.draw_radial_gradient(p_layer, p_gradient);
	}

	void LiteHTMLContainer::draw_conic_gradient(uint_ptr, const background_layer& p_layer, const background_layer::conic_gradient& p_gradient) {
		paint.draw_conic_gradient(p_layer, p_gradient);
	}

	void LiteHTMLContainer::draw_box_shadows(const html_tag& p_element, const litehtml::position& p_box,
		const borders& p_borders, bool p_inset) {
		if (const auto* shadows = docrender_editor::get_box_shadows(p_element); shadows && !shadows->empty()) {
			paint.draw_box_shadows(p_element, p_box, p_borders, p_inset, *shadows);
		}
	}

	void LiteHTMLContainer::draw_borders(uint_ptr, const borders& p_borders, const litehtml::position& p_position, bool) {
		paint.draw_borders(p_borders, p_position);
	}

	void LiteHTMLContainer::transform_text(litehtml::string& r_text, text_transform p_transform) {
		transform_html_text(r_text, p_transform);
	}

	void LiteHTMLContainer::set_clip(const litehtml::position& p_position, const border_radiuses& p_radii) {
		paint.set_clip(p_position, p_radii);
	}

	void LiteHTMLContainer::set_render_item_clip(const litehtml::position& p_position, const border_radiuses& p_radii,
		const render_item& p_item) {
		paint.set_render_item_clip(p_position, p_radii, p_item);
	}

	bool LiteHTMLContainer::begin_render_item(const render_item& p_item, const litehtml::position*& r_clip) {
		return paint.begin_render_item(p_item, r_clip);
	}

	void LiteHTMLContainer::end_render_item() {
		paint.end_render_item();
	}

	void LiteHTMLContainer::del_clip() {
		paint.del_clip();
	}

	void LiteHTMLContainer::get_viewport(litehtml::position& r_viewport) const {
		r_viewport = litehtml::position(0, 0, viewport_size.x, viewport_size.y);
	}

	void LiteHTMLContainer::get_media_features(media_features& r_media) const {
		r_media.type = media_type_screen;
		r_media.width = r_media.device_width = viewport_size.x;
		r_media.height = r_media.device_height = viewport_size.y;
		r_media.color = 8;
		r_media.resolution = CSS_DPI;
	}

	void LiteHTMLContainer::get_language(litehtml::string& r_language, litehtml::string& r_culture) const {
		const String locale = TranslationServer::get_singleton()->get_locale();
		r_language = locale.get_slice("_", 0).utf8().get_data();
		r_culture = locale.get_slice("_", 1).utf8().get_data();
	}
} // namespace docrender
