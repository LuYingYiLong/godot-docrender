#pragma once

#include "html_paint_run.h"
#include "html_shaped_text.h"
#include "litehtml_box_shadow.h"

#include <godot_cpp/variant/typed_array.hpp>
#include <litehtml/background.h>
#include <litehtml/borders.h>

#include <functional>

namespace litehtml {
	class html_tag;
	struct list_marker;
}

namespace docrender {
	class HTMLPaintRecorder {
		struct Clip {
			godot::Rect2 bounds;
			RoundedRect shape;
			godot::Transform2D matrix;
			bool has_precise_clip;
		};

		struct TextRun {
			godot::Ref<godot::TextLine> line;
			godot::Rect2 rect;
			godot::Rect2 visible_rect;
			int64_t offset;
			int64_t length;
			godot::Transform2D matrix;
			godot::Transform2D inverse;
			std::vector<TransformedClip> clips;
		};

		using Fill = HTMLFill;
		using Text = HTMLText;
		using Image = HTMLImage;
		using Circle = HTMLCircle;
		using Command = HTMLPaintCommand;

		using TextShaper = std::function<HTMLTextLayout(const litehtml::element&, const godot::String&)>;

		std::vector<HTMLPaintRun> paint_runs;
		std::vector<HTMLPaintRun> diagnostic_runs;
		std::vector<Clip> clips;
		godot::Ref<godot::ShaderMaterial> clip_material;
		godot::Transform2D clip_material_matrix;
		const HTMLGeometryIndex* recording_geometry = nullptr;
		const RenderGeometry* active_geometry = nullptr;
		std::vector<const RenderGeometry*> painted_elements;
		godot::String plain_text;
		std::vector<TextRun> text_runs;
		godot::HashMap<godot::Vector2, std::vector<Text*>> painted_text;
		godot::HashMap<const litehtml::render_item*, std::vector<Text*>> painted_text_by_renderer;

		void index_text(const std::shared_ptr<litehtml::element>& p_element, const TextShaper& p_shape_text, godot::String& r_separator);
		godot::Ref<godot::ShaderMaterial> create_material(const RoundedRect* p_shape = nullptr);
		void add_command(Command p_command, godot::Ref<godot::ShaderMaterial> p_material = {});
		void add_command_to(std::vector<HTMLPaintRun>& r_runs, Command p_command, godot::Ref<godot::ShaderMaterial> p_material = {});
		void fill_rect(const godot::Rect2& p_rect, const godot::Color& p_color);
		void draw_decoration(const godot::Rect2& p_rect, litehtml::text_decoration_style p_style, const godot::Color& p_color);
		void draw_gradient(const litehtml::background_layer& p_layer, const litehtml::background_layer::gradient_base& p_gradient,
			int p_type, const godot::Vector2& p_start, const godot::Vector2& p_parameters);
		void push_clip(const RoundedRect& p_shape, const godot::Transform2D& p_matrix);

	public:
		void clear();
		void begin(const HTMLGeometryIndex& p_geometry);
		void finish(const std::shared_ptr<litehtml::element>& p_root, const TextShaper& p_shape_text);

		const std::vector<HTMLPaintRun>& get_paint_runs() const;
		const std::vector<HTMLPaintRun>& get_diagnostic_runs() const { return diagnostic_runs; }
		std::shared_ptr<litehtml::element> hit_test_element(const godot::Vector2& p_point) const;
		const godot::String& get_plain_text() const;
		int64_t hit_test_text(const godot::Vector2& p_point) const;
		godot::TypedArray<godot::Rect2> get_selection_rects(int64_t p_from, int64_t p_to) const;

		void clear_diagnostics() { diagnostic_runs.clear(); }
		void record_css_diagnostic(const RenderGeometry& p_geometry, const godot::Rect2& p_box);
		void draw_text(const HTMLShapedText& p_shaped, int64_t p_length, litehtml::web_color p_color,
			const litehtml::position& p_position, const std::vector<litehtml::box_shadow>& p_shadows);
		void draw_list_marker(const litehtml::list_marker& p_marker, const godot::Ref<godot::Texture2D>& p_texture);
		void draw_image(const litehtml::background_layer& p_layer, const godot::Ref<godot::Texture2D>& p_texture);
		void draw_solid_fill(const litehtml::background_layer& p_layer, const litehtml::web_color& p_color);
		void draw_box_shadows(const litehtml::html_tag& p_element, const litehtml::position& p_box,
			const litehtml::borders& p_borders, bool p_inset, const std::vector<litehtml::box_shadow>& p_shadows);
		void draw_borders(const litehtml::borders& p_borders, const litehtml::position& p_position);
		void draw_linear_gradient(const litehtml::background_layer& p_layer, const litehtml::background_layer::linear_gradient& p_gradient);
		void draw_radial_gradient(const litehtml::background_layer& p_layer, const litehtml::background_layer::radial_gradient& p_gradient);
		void draw_conic_gradient(const litehtml::background_layer& p_layer, const litehtml::background_layer::conic_gradient& p_gradient);

		void set_clip(const litehtml::position& p_position, const litehtml::border_radiuses& p_radii);
		void set_render_item_clip(const litehtml::position& p_position, const litehtml::border_radiuses& p_radii,
			const litehtml::render_item& p_item);
		bool begin_render_item(const litehtml::render_item& p_item, const litehtml::position*& r_clip);
		void end_render_item();
		void del_clip();
	};
}
