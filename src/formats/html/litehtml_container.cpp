#include "litehtml_container.h"

#include "html_markup.h"

#include <godot_cpp/classes/font_variation.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/classes/translation_server.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <litehtml/document.h>
#include <litehtml/el_image.h>
#include <litehtml/render_image.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace godot;
using namespace litehtml;

namespace docrender {
	namespace {
		constexpr float CSS_DPI = 96.0f;

		Rect2 to_rect(const litehtml::position& p_position) {
			return Rect2(p_position.x, p_position.y, p_position.width, p_position.height);
		}

		Color to_color(const web_color& p_color) {
			return Color(p_color.red / 255.0f, p_color.green / 255.0f, p_color.blue / 255.0f, p_color.alpha / 255.0f);
		}

		Rect2 get_background_clip(const background_layer& p_layer) {
			return to_rect(p_layer.border_box).intersection(to_rect(p_layer.clip_box));
		}

		float solid_border_width(const border& p_border, float p_maximum) {
			return p_border.style == border_style_solid ? std::clamp(p_border.width, 0.0f, p_maximum) : 0.0f;
		}

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
		const HTMLLayout::Fonts& p_fonts) :
		base_dir(p_base_dir), textures(p_textures), fit_images(p_fit_images), fonts(p_fonts), default_font(p_fonts.text) {
		ThemeDB* themes = ThemeDB::get_singleton();
		const Ref<Theme> project_theme = themes->get_project_theme();
		if (default_font.is_null() && project_theme.is_valid()) {
			default_font = project_theme->get_default_font();
		}
		if (default_font.is_null()) {
			default_font = themes->get_fallback_font();
		}
	}

	litehtml::element::ptr LiteHTMLContainer::create_element(const char* p_tag, const litehtml::string_map&,
		const std::shared_ptr<litehtml::document>& p_document) {
		return fit_images && std::string_view(p_tag) == "img" ? std::make_shared<FittedImageElement>(p_document) : nullptr;
	}

	void LiteHTMLContainer::set_viewport(const Vector2i& p_size) {
		viewport_size = p_size;
	}

	void LiteHTMLContainer::record(litehtml::document& p_document, const Vector2i& p_content_size) {
		clips.clear();
		paint_runs.clear();
		const litehtml::position bounds(0, 0, std::max(p_content_size.x, viewport_size.x),
			std::max(p_content_size.y, viewport_size.y));
		p_document.draw(0, 0, 0, &bounds);
		plain_text = String();
		text_runs.clear();
		for (PaintRun& run : paint_runs) {
			for (Command& command : run.commands) {
				if (Text* text = std::get_if<Text>(&command)) {
					const Rect2 rect(text->position, text->line->get_size());
					const Rect2 visible = run.clip ? rect.intersection(*run.clip) : rect;
					if (visible.has_area() && text->length > 0) {
						painted_text[text->position].push_back(text);
					}
				}
			}
		}
		String separator;
		index_text(p_document.root(), separator);
		// Paint order can differ from document order for positioned or floating text.
		// The DOM pass assigns offsets while the paint runs retain the exact clip geometry.
		painted_text.clear();
		for (const PaintRun& run : paint_runs) {
			for (const Command& command : run.commands) {
				if (const Text* text = std::get_if<Text>(&command); text && text->offset >= 0) {
					const Rect2 rect(text->position, text->line->get_size());
					const Rect2 visible = run.clip ? rect.intersection(*run.clip) : rect;
					text_runs.push_back({ text->line, rect, visible, text->offset, text->length });
				}
			}
		}
	}

	void LiteHTMLContainer::index_text(const litehtml::element::ptr& p_element, String& r_separator) {
		if (!p_element || p_element->css().get_display() == display_none) {
			return;
		}
		const auto display = p_element->css().get_display();
		const bool visible = p_element->css().get_visibility() == visibility_visible;
		if (p_element->is_break()) {
			if (visible && !plain_text.is_empty()) {
				if (r_separator == " " || r_separator == "\t") {
					r_separator = String();
				}
				r_separator += "\n";
			}
			return;
		}
		if (display == display_inline_text) {
			std::shared_ptr<render_item> item;
			p_element->run_on_renderers([&](const std::shared_ptr<render_item>& p_item) {
				item = p_item;
				return false;
			});
			if (!visible || !item) {
				return;
			}
			if (p_element->is_white_space()) {
				if (r_separator.is_empty() && !plain_text.is_empty()) {
					r_separator = " ";
				}
				return;
			}
			if (item->skip() || !p_element->parent()) {
				return;
			}
			const auto font_handle = p_element->parent()->css().get_font();
			if (!font_handle) {
				return;
			}
			FontData& font = *reinterpret_cast<FontData*>(font_handle);
			std::string value;
			p_element->get_text(value);
			if (value == "\t") {
				value = "    ";
			}
			transform_text(value, p_element->css().get_text_transform());
			const Ref<TextLine> line = font.shape(value.c_str());
			position placement = item->get_placement();
			placement.round();
			const Vector2 point(placement.x, placement.y + font.ascent - line->get_line_ascent());
			if (auto* candidates = painted_text.getptr(point)) {
				for (Text* text : *candidates) {
					if (text->offset < 0 && text->line == line) {
						plain_text += r_separator;
						r_separator = String();
						text->offset = plain_text.length();
						plain_text += String::utf8(value.c_str());
						break;
					}
				}
			}
			return;
		}
		const bool block = display != display_inline && display != display_inline_block && display != display_inline_flex &&
			display != display_inline_table && display != display_table_cell;
		const int64_t start = plain_text.length();
		const String preceding_separator = r_separator;
		if (block && start > 0 && !r_separator.begins_with("\n")) {
			r_separator = "\n";
		}
		for (const auto& child : p_element->children()) {
			index_text(child, r_separator);
		}
		if (plain_text.length() > start) {
			if (block && !r_separator.begins_with("\n")) {
				r_separator = "\n";
			}
			else if (display == display_table_cell && !r_separator.begins_with("\n")) {
				r_separator = "\t";
			}
		} else if (block) {
			r_separator = preceding_separator;
		}
	}

	const std::vector<LiteHTMLContainer::PaintRun>& LiteHTMLContainer::get_paint_runs() const {
		return paint_runs;
	}

	const String& LiteHTMLContainer::get_plain_text() const {
		return plain_text;
	}

	int64_t LiteHTMLContainer::hit_test_text(const Vector2& p_point) const {
		const TextRun* closest = nullptr;
		real_t best_y = std::numeric_limits<real_t>::infinity();
		real_t best_x = best_y;
		real_t top = best_y;
		real_t bottom = -best_y;
		for (const TextRun& run : text_runs) {
			const Rect2& rect = run.visible_rect;
			const Vector2 end = rect.get_end();
			top = std::min(top, rect.position.y);
			bottom = std::max(bottom, end.y);
			const real_t dy = std::max({ rect.position.y - p_point.y, real_t(0), p_point.y - end.y });
			const real_t dx = std::max({ rect.position.x - p_point.x, real_t(0), p_point.x - end.x });
			if (dy < best_y || (dy == best_y && dx <= best_x)) {
				closest = &run;
				best_y = dy;
				best_x = dx;
			}
		}
		if (!closest || p_point.y < top) {
			return 0;
		}
		if (p_point.y > bottom) {
			return plain_text.length();
		}
		const real_t x = CLAMP(p_point.x, closest->visible_rect.position.x, closest->visible_rect.get_end().x);
	return closest->offset + CLAMP(static_cast<int64_t>(closest->line->hit_test(x - closest->rect.position.x)), int64_t(0), closest->length);
	}

	TypedArray<Rect2> LiteHTMLContainer::get_selection_rects(int64_t p_from, int64_t p_to) const {
		TypedArray<Rect2> result;
		const Ref<TextServer> text_server = TextServerManager::get_singleton()->get_primary_interface();
		if (text_server.is_null()) {
			return result;
		}
		for (const TextRun& run : text_runs) {
			const int64_t from = std::max<int64_t>(0, p_from - run.offset);
			const int64_t to = std::min(run.length, p_to - run.offset);
			if (from >= to) {
				continue;
			}
			const auto ranges = text_server->shaped_text_get_selection(run.line->get_rid(), from, to);
			for (int64_t i = 0; i < ranges.size(); ++i) {
				const Vector2 range = ranges[i];
				const Rect2 rect(run.rect.position + Vector2(range.x, 0), Vector2(range.y - range.x, run.rect.size.y));
				const Rect2 visible = rect.intersection(run.visible_rect);
				if (visible.has_area()) {
					result.push_back(visible);
				}
			}
		}
		return result;
	}

	void LiteHTMLContainer::PaintRun::draw(RenderingServer& p_server, const RID& p_canvas, const Vector2& p_offset,
		const TextServer* p_text_server, const Highlight& p_search, const Highlight& p_selection) const {
		for (const Command& command : commands) {
			std::visit([&](const auto& p_draw) {
				using Draw = std::decay_t<decltype(p_draw)>;
				if constexpr (std::is_same_v<Draw, Fill>) {
					const Rect2 rect(p_draw.rect.position + p_offset, p_draw.rect.size);
					p_server.canvas_item_add_rect(p_canvas, rect, p_draw.color);
				}
				else if constexpr (std::is_same_v<Draw, Text>) {
					if (p_text_server && p_draw.offset >= 0) {
						for (const Highlight* highlight : { &p_search, &p_selection }) {
							const int64_t from = std::max<int64_t>(0, highlight->from - p_draw.offset);
							const int64_t to = std::min(p_draw.length, highlight->to - p_draw.offset);
							if (from < to) {
								const auto ranges = p_text_server->shaped_text_get_selection(p_draw.line->get_rid(), from, to);
								for (int64_t i = 0; i < ranges.size(); ++i) {
									const Vector2 range = ranges[i];
									const Rect2 rect(p_draw.position + p_offset + Vector2(range.x, 0),
										Vector2(range.y - range.x, p_draw.line->get_size().y));
									if (highlight->style) {
										highlight->style->draw(p_canvas, rect);
									} else {
										p_server.canvas_item_add_rect(p_canvas, rect, highlight->color);
									}
								}
							}
						}
					}
					p_draw.line->draw(p_canvas, p_draw.position + p_offset, p_draw.color);
				}
				else if constexpr (std::is_same_v<Draw, Image>) {
					const Rect2 rect(p_draw.destination.position + p_offset, p_draw.destination.size);
					if (p_draw.repeat) {
						p_server.canvas_item_set_default_texture_repeat(p_canvas, RenderingServer::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED);
					}
					p_draw.texture->draw_rect_region(p_canvas, rect, p_draw.source, Color(1, 1, 1), false, !p_draw.repeat);
				}
				else if constexpr (std::is_same_v<Draw, Circle>) {
					const Vector2 center = p_draw.center + p_offset;
					if (p_draw.filled) {
						p_server.canvas_item_add_circle(p_canvas, center, p_draw.radius, p_draw.color, true);
					}
					else {
						PackedVector2Array points;
						constexpr int SEGMENTS = 20;
						for (int index = 0; index <= SEGMENTS; ++index) {
							const float angle = static_cast<float>(Math_TAU) * index / SEGMENTS;
							points.push_back(center + Vector2(std::cos(angle), std::sin(angle)) * p_draw.radius);
						}
						p_server.canvas_item_add_polyline(p_canvas, points, PackedColorArray({ p_draw.color }), 1.0f, true);
					}
				}
				}, command);
		}
	}

	void LiteHTMLContainer::add_command(Command p_command) {
		std::optional<Rect2> clip;
		if (!clips.empty()) {
			if (!clips.back().has_area()) {
				return;
			}
			clip = clips.back();
		}
		if (paint_runs.empty() || paint_runs.back().clip != clip) {
			paint_runs.push_back({ clip,{} });
		}
		paint_runs.back().commands.push_back(std::move(p_command));
	}

	void LiteHTMLContainer::fill_rect(const Rect2& p_rect, const Color& p_color) {
		if (p_rect.has_area() && p_color.a > 0.0f) {
			add_command(Fill{ p_rect, p_color });
		}
	}

	Ref<TextLine> LiteHTMLContainer::FontData::shape(const char* p_text) {
		const String text = String::utf8(p_text);
		if (const Ref<TextLine>* cached = lines.getptr(text)) {
			return *cached;
		}
		Ref<TextLine> line;
		line.instantiate();
		line->add_string(text, font, size);
		lines.insert(text, line);
		return line;
	}

	uint_ptr LiteHTMLContainer::create_font(const font_description& p_description, const litehtml::document*,
		font_metrics* r_metrics) {
		const double requested_size = std::round(static_cast<double>(p_description.size));
		const int32_t size = std::isfinite(requested_size) && requested_size <= std::numeric_limits<int32_t>::max()
			? static_cast<int32_t>(std::max(1.0, requested_size))
			: static_cast<int32_t>(get_default_font_size());
		Ref<Font> font = default_font;
		const bool bold = p_description.weight >= 600;
		const bool italic = p_description.style == font_style_italic;
		bool code_family = false;
		if (fonts.code.is_valid() || fonts.code_block.is_valid()) {
			for (const String& family : String::utf8(p_description.family.c_str()).to_lower().split(",")) {
				if (family == "monospace" && fonts.code.is_valid()) {
					font = fonts.code;
					code_family = true;
					break;
				}
				if (family == "docrender-code-block" && fonts.code_block.is_valid()) {
					font = fonts.code_block;
					code_family = true;
					break;
				}
				if (family == "sans-serif" || family == "serif") {
					break;
				}
			}
		}
		Ref<Font> variant;
		if (!code_family && (bold || italic)) {
			variant = bold && italic ? fonts.bold_italic : (bold ? fonts.bold : fonts.italic);
		}
		if (variant.is_valid()) {
			font = variant;
		}
		else if (bold || italic) {
			Ref<FontVariation> variation;
			variation.instantiate();
			variation->set_base_font(font);
			if (bold) {
				variation->set_variation_embolden(0.6f);
			}
			if (italic) {
				variation->set_variation_transform(Transform2D(1.0f, 0.0f, -0.2f, 1.0f, 0.0f, 0.0f));
			}
			font = variation;
		}

		const float ascent = font->get_ascent(size);
		auto* data = new FontData{ font, size, ascent, p_description,{} };
		r_metrics->font_size = size;
		r_metrics->ascent = ascent;
		r_metrics->descent = font->get_descent(size);
		r_metrics->height = r_metrics->ascent + r_metrics->descent;
		r_metrics->x_height = r_metrics->ascent * 0.65f;
		r_metrics->ch_width = data->shape("0")->get_line_width();
		r_metrics->draw_spaces = p_description.decoration_line != text_decoration_line_none;
		r_metrics->sub_shift = size * 0.2f;
		r_metrics->super_shift = size * 0.35f;
		return reinterpret_cast<uint_ptr>(data);
	}

	void LiteHTMLContainer::delete_font(uint_ptr p_font) {
		delete reinterpret_cast<FontData*>(p_font);
	}

	pixel_t LiteHTMLContainer::text_width(const char* p_text, uint_ptr p_font) {
		return reinterpret_cast<FontData*>(p_font)->shape(p_text)->get_line_width();
	}

	void LiteHTMLContainer::draw_text(uint_ptr, const char* p_text, uint_ptr p_font, web_color p_color,
		const litehtml::position& p_position) {
		FontData& font = *reinterpret_cast<FontData*>(p_font);
		const Ref<TextLine> line = font.shape(p_text);
		const Vector2 top_left(p_position.x, p_position.y + font.ascent - line->get_line_ascent());
		const Color color = to_color(p_color);
		add_command(Text{ line, top_left, color, -1, String::utf8(p_text).length() });

		const int decoration = font.description.decoration_line;
		if (decoration == text_decoration_line_none) {
			return;
		}
		const float thickness = std::max(1.0f, line->get_line_underline_thickness());
		const float width = line->get_line_width();
		const float baseline = top_left.y + line->get_line_ascent();
		const web_color css_color = font.description.decoration_color;
		const Color decoration_color = css_color.is_current_color ? color : to_color(css_color);
		if (decoration & text_decoration_line_underline) {
			fill_rect(Rect2(top_left.x, baseline + line->get_line_underline_position(), width, thickness), decoration_color);
		}
		if (decoration & text_decoration_line_overline) {
			fill_rect(Rect2(top_left.x, top_left.y, width, thickness), decoration_color);
		}
		if (decoration & text_decoration_line_line_through) {
			fill_rect(Rect2(top_left.x, baseline - line->get_line_ascent() / 3.0f, width, thickness), decoration_color);
		}
	}

	pixel_t LiteHTMLContainer::pt_to_px(float p_points) const {
		return p_points * (CSS_DPI / 72.0f);
	}

	pixel_t LiteHTMLContainer::get_default_font_size() const {
		return 16.0f;
	}

	const char* LiteHTMLContainer::get_default_font_name() const {
		return "sans-serif";
	}

	void LiteHTMLContainer::draw_list_marker(uint_ptr, const list_marker& p_marker) {
		const Rect2 rect = to_rect(p_marker.pos);
		const Color color = to_color(p_marker.color);
		if (!p_marker.image.empty()) {
			const Ref<Texture2D> texture = load_texture(p_marker.image.c_str());
			if (texture.is_valid()) {
				add_command(Image{ texture, rect, Rect2(Vector2(), texture->get_size()), false });
				return;
			}
		}
		if (p_marker.marker_type == list_style_type_square) {
			fill_rect(rect, color);
		}
		else if (p_marker.marker_type == list_style_type_disc || p_marker.marker_type == list_style_type_circle) {
			const float radius = std::min(rect.size.x, rect.size.y) / 2.0f;
			add_command(Circle{ rect.get_center(), radius, color, p_marker.marker_type == list_style_type_disc });
		}
	}

	Ref<Texture2D> LiteHTMLContainer::load_texture(const char* p_src) {
		const String path = resolve_html_image_path(String::utf8(p_src), base_dir);
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

	void LiteHTMLContainer::load_image(const char* p_src, const char*, bool) {
		load_texture(p_src);
	}

	void LiteHTMLContainer::get_image_size(const char* p_src, const char*, litehtml::size& r_size) {
		const Ref<Texture2D> texture = load_texture(p_src);
		const Vector2 size = texture.is_valid() ? texture->get_size() : Vector2();
		r_size = litehtml::size(size.x, size.y);
	}

	void LiteHTMLContainer::draw_image(uint_ptr, const background_layer& p_layer, const std::string& p_url, const std::string&) {
		const Ref<Texture2D> texture = load_texture(p_url.c_str());
		const Rect2 tile = to_rect(p_layer.origin_box);
		if (texture.is_null() || !tile.has_area()) {
			return;
		}

		const bool repeat_x = p_layer.repeat == background_repeat_repeat || p_layer.repeat == background_repeat_repeat_x;
		const bool repeat_y = p_layer.repeat == background_repeat_repeat || p_layer.repeat == background_repeat_repeat_y;
		const Rect2 bounds = get_background_clip(p_layer);
		const Rect2 extent(repeat_x ? bounds.position.x : tile.position.x, repeat_y ? bounds.position.y : tile.position.y,
			repeat_x ? bounds.size.x : tile.size.x, repeat_y ? bounds.size.y : tile.size.y);
		const Rect2 destination = bounds.intersection(extent);
		if (destination.has_area()) {
			const Vector2 scale = texture->get_size() / tile.size;
			const Rect2 source((destination.position - tile.position) * scale, destination.size * scale);
			add_command(Image{ texture, destination, source, repeat_x || repeat_y });
		}
	}

	void LiteHTMLContainer::draw_solid_fill(uint_ptr, const background_layer& p_layer, const web_color& p_color) {
		fill_rect(get_background_clip(p_layer), to_color(p_color));
	}

	void LiteHTMLContainer::draw_borders(uint_ptr, const borders& p_borders, const litehtml::position& p_position, bool) {
		const Rect2 rect = to_rect(p_position);
		if (!rect.has_area()) {
			return;
		}
		const float top = solid_border_width(p_borders.top, rect.size.y);
		const float bottom = solid_border_width(p_borders.bottom, rect.size.y - top);
		const float left = solid_border_width(p_borders.left, rect.size.x);
		const float right = solid_border_width(p_borders.right, rect.size.x - left);
		const float inner_height = rect.size.y - top - bottom;
		const Vector2 end = rect.get_end();
		fill_rect(Rect2(rect.position, Vector2(rect.size.x, top)), to_color(p_borders.top.color));
		fill_rect(Rect2(rect.position.x, end.y - bottom, rect.size.x, bottom), to_color(p_borders.bottom.color));
		fill_rect(Rect2(rect.position.x, rect.position.y + top, left, inner_height), to_color(p_borders.left.color));
		fill_rect(Rect2(end.x - right, rect.position.y + top, right, inner_height), to_color(p_borders.right.color));
	}

	void LiteHTMLContainer::transform_text(litehtml::string& r_text, text_transform p_transform) {
		String text = String::utf8(r_text.c_str());
		switch (p_transform) {
		case text_transform_uppercase:
			text = text.to_upper();
			break;
		case text_transform_lowercase:
			text = text.to_lower();
			break;
		case text_transform_capitalize:
			text = text.substr(0, 1).to_upper() + text.substr(1);
			break;
		default:
			return;
		}
		r_text = text.utf8().get_data();
	}

	void LiteHTMLContainer::set_clip(const litehtml::position& p_position, const border_radiuses&) {
		const Rect2 rect = to_rect(p_position);
		clips.push_back(clips.empty() ? rect : clips.back().intersection(rect));
	}

	void LiteHTMLContainer::del_clip() {
		if (!clips.empty()) {
			clips.pop_back();
		}
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
