#ifndef DOCRENDER_LITEHTML_CONTAINER_H
#define DOCRENDER_LITEHTML_CONTAINER_H

#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/text_line.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <litehtml/document_container.h>

#include <optional>
#include <variant>
#include <vector>

namespace godot {
	class RenderingServer;
}

namespace docrender {
	class LiteHTMLContainer final : public litehtml::document_container {
		struct Fill {
			godot::Rect2 rect;
			godot::Color color;
		};

		struct Text {
			godot::Ref<godot::TextLine> line;
			godot::Vector2 position;
			godot::Color color;
		};

		struct Image {
			godot::Ref<godot::Texture2D> texture;
			godot::Rect2 destination;
			godot::Rect2 source;
			bool repeat;
		};

		struct Circle {
			godot::Vector2 center;
			float radius;
			godot::Color color;
			bool filled;
		};

		using Command = std::variant<Fill, Text, Image, Circle>;

	public:
		struct PaintRun {
			std::optional<godot::Rect2> clip;
			std::vector<Command> commands;

			void draw(godot::RenderingServer& p_server, const godot::RID& p_canvas, const godot::Vector2& p_offset) const;
		};

		explicit LiteHTMLContainer(const godot::String& p_base_dir);
		static std::optional<godot::String> normalize_resource_path(const godot::String& p_path);
		void set_viewport(const godot::Vector2i& p_size);
		void record(litehtml::document& p_document, const godot::Vector2i& p_content_size);
		const std::vector<PaintRun>& get_paint_runs() const;

	private:
		struct FontData {
			godot::Ref<godot::Font> font;
			int32_t size;
			float ascent;
			litehtml::font_description description;
			godot::HashMap<godot::String, godot::Ref<godot::TextLine>> lines;

			godot::Ref<godot::TextLine> shape(const char* p_text);
		};

		godot::String base_dir;
		godot::Vector2i viewport_size;
		godot::Ref<godot::Font> default_font;
		godot::HashMap<godot::String, godot::Ref<godot::Texture2D>> images;
		std::vector<PaintRun> paint_runs;
		std::vector<godot::Rect2> clips;

		godot::Ref<godot::Texture2D> load_texture(const char* p_src);
		void add_command(Command p_command);
		void fill_rect(const godot::Rect2& p_rect, const godot::Color& p_color);

		litehtml::uint_ptr create_font(const litehtml::font_description& p_description, const litehtml::document*,
			litehtml::font_metrics* r_metrics) override;
		void delete_font(litehtml::uint_ptr p_font) override;
		litehtml::pixel_t text_width(const char* p_text, litehtml::uint_ptr p_font) override;
		void draw_text(litehtml::uint_ptr, const char* p_text, litehtml::uint_ptr p_font,
			litehtml::web_color p_color, const litehtml::position& p_position) override;
		litehtml::pixel_t pt_to_px(float p_points) const override;
		litehtml::pixel_t get_default_font_size() const override;
		const char* get_default_font_name() const override;
		void draw_list_marker(litehtml::uint_ptr, const litehtml::list_marker& p_marker) override;
		void load_image(const char* p_src, const char*, bool) override;
		void get_image_size(const char* p_src, const char*, litehtml::size& r_size) override;
		void draw_image(litehtml::uint_ptr, const litehtml::background_layer& p_layer, const std::string& p_url,
			const std::string&) override;
		void draw_solid_fill(litehtml::uint_ptr, const litehtml::background_layer& p_layer,
			const litehtml::web_color& p_color) override;
		void draw_borders(litehtml::uint_ptr, const litehtml::borders& p_borders,
			const litehtml::position& p_position, bool) override;
		void transform_text(litehtml::string& r_text, litehtml::text_transform p_transform) override;
		void set_clip(const litehtml::position& p_position, const litehtml::border_radiuses&) override;
		void del_clip() override;
		void get_viewport(litehtml::position& r_viewport) const override;
		void get_media_features(litehtml::media_features& r_media) const override;
		void get_language(litehtml::string& r_language, litehtml::string& r_culture) const override;

		// The standalone renderer draws static HTML with inline and embedded CSS.
		void import_css(litehtml::string& r_text, const litehtml::string&, litehtml::string&) override { r_text.clear(); }
		void set_base_url(const char*) override {}
		void set_caption(const char*) override {}
		void link(const std::shared_ptr<litehtml::document>&, const litehtml::element::ptr&) override {}
		void on_anchor_click(const char*, const litehtml::element::ptr&) override {}
		void on_mouse_event(const litehtml::element::ptr&, litehtml::mouse_event) override {}
		void set_cursor(const char*) override {}
		litehtml::element::ptr create_element(const char*, const litehtml::string_map&,
			const std::shared_ptr<litehtml::document>&) override {
			return {};
		}

		void draw_linear_gradient(litehtml::uint_ptr, const litehtml::background_layer&,
			const litehtml::background_layer::linear_gradient&) override {
		}
		void draw_radial_gradient(litehtml::uint_ptr, const litehtml::background_layer&,
			const litehtml::background_layer::radial_gradient&) override {
		}
		void draw_conic_gradient(litehtml::uint_ptr, const litehtml::background_layer&,
			const litehtml::background_layer::conic_gradient&) override {
		}
	};
} // namespace docrender

#endif
