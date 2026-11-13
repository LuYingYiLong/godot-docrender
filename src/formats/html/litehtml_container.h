#ifndef DOCRENDER_LITEHTML_CONTAINER_H
#define DOCRENDER_LITEHTML_CONTAINER_H

#include "html_layout.h"

#include <godot_cpp/classes/text_line.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <litehtml/document_container.h>

#include <optional>
#include <variant>
#include <vector>

namespace godot {
	class RenderingServer;
	class StyleBox;
	class TextServer;
}

namespace docrender {
	class LiteHTMLContainer final : public litehtml::document_container {
		struct RoundedRect {
			godot::Rect2 rect;
			godot::Vector4 radii_x;
			godot::Vector4 radii_y;

			RoundedRect(const godot::Rect2& p_rect, const litehtml::border_radiuses& p_radii);
			
			RoundedRect inset(const godot::Rect2& p_rect) const;					// 返回内缩到指定矩形并相应缩小圆角后的形状
			RoundedRect spread(godot::real_t p_distance) const;						// 按阴影扩张距离调整边界和圆角
			bool has_radius() const;												// 检查是否存在有效的圆角
			bool has_point(const godot::Vector2& p_position) const;					// 检查指定坐标是否在圆角矩形内
		};

		struct Clip {
			godot::Rect2 bounds;
			RoundedRect shape;
			godot::Ref<godot::ShaderMaterial> material;
		};

		static RoundedRect get_overflow_clip(litehtml::render_item& p_item);

		struct Fill {
			godot::Rect2 rect;
			godot::Color color;
		};

		struct Text {
			godot::Ref<godot::TextLine> line;
			godot::Vector2 position;
			godot::Color color;
			int64_t offset = -1;
			int64_t length = 0;
		};

		struct TextRun {
			godot::Ref<godot::TextLine> line;
			godot::Rect2 rect;
			godot::Rect2 visible_rect;
			int64_t offset;
			int64_t length;
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
		struct Highlight {
			int64_t from = 0;
			int64_t to = 0;
			const godot::StyleBox* style = nullptr;
			godot::Color color;
		};

		struct PaintRun {
			std::optional<godot::Rect2> clip;
			godot::Ref<godot::ShaderMaterial> material;
			std::vector<Command> commands;

			void draw(
				godot::RenderingServer& p_server,
				const godot::RID& p_canvas,
				const godot::Vector2& p_offset,
				const godot::TextServer* p_text_server,
				const Highlight& p_search,
				const Highlight& p_selection
			) const;																// 将此批绘制命令及高亮提交到指定画布
		};

		LiteHTMLContainer(
			const godot::String& p_base_dir,
			const godot::Dictionary& p_textures,
			bool p_fit_images,
			const HTMLLayout::Fonts& p_fonts,
			const godot::Dictionary& p_local_stylesheets
		);

		void set_viewport(const godot::Vector2i& p_size);							// 更新排版和媒体查询使用的视口尺寸
		void record(
			litehtml::document& p_document,
			const godot::Vector2i& p_content_size
		);																			// 记录已排版文档的绘制命令并建立可选择文本的索引
		const std::vector<PaintRun>& get_paint_runs() const;						// 返回最近一次记录生成的绘制批次
		const godot::String& get_plain_text() const;								// 返回从排版结果提取的纯文本
		const litehtml::string_map* get_source_attributes(
			const litehtml::element& p_element
		) const;																	// 查找元素的原始属性表
		void set_source_attribute(
			const litehtml::element::ptr& p_element,
			const char* p_name,
			const char* p_value
		);																			// 更新记录的元素属性，传入空指针时移除该属性
		void forget_source_attributes(const litehtml::element& p_element);			// 删除指定元素的属性记录
		void reset_stylesheet_reads();												// 重置本轮外部样式表的读取计数
		void set_css_diagnostics_enabled(bool p_enabled);							// 设置是否收集 CSS 声明诊断
		void begin_css_diagnostics();												// 清空上一轮样式诊断
		void set_css_source_kind(litehtml::css_source_kind p_kind);					// 设置当前样式表来源
		godot::TypedArray<godot::Dictionary> get_css_diagnostics() const;			// 返回诊断快照
		const std::vector<int64_t>* get_element_diagnostics(const litehtml::element& p_element) const;
		const godot::Dictionary& get_css_diagnostic(int64_t p_index) const;
		void record_css_diagnostic(const litehtml::render_item& p_item, const godot::Rect2& p_box);
		const std::vector<PaintRun>& get_diagnostic_runs() const { return diagnostic_runs; }
		int64_t hit_test_text(const godot::Vector2& p_point) const;					// 查找最接近文档坐标的文本光标偏移
		godot::TypedArray<godot::Rect2> get_selection_rects(
			int64_t p_from,
			int64_t p_to
		) const;																	// 返回指定文本范围的可见选区矩形
		static bool is_hit_visible(
			const litehtml::render_item& p_item,
			const godot::Vector2& p_position
		);																			// 判断文档坐标是否通过元素及祖先的裁剪
		static bool has_visible_area(
			const litehtml::render_item& p_item,
			const godot::Rect2& p_box
		);																			// 检查指定矩形经过元素及祖先裁剪后是否仍有可见面积

	private:
		struct FontData {
			godot::Ref<godot::Font> font;
			int32_t size;
			float ascent;
			litehtml::font_description description;
			godot::HashMap<godot::String, godot::Ref<godot::TextLine>> lines;

			godot::Ref<godot::TextLine> shape(const char* p_text);					// 返回指定 UTF-8 文本的排版结果，优先复用缓存
		};

		struct SourceAttributes {
			std::weak_ptr<litehtml::element> element;
			litehtml::string_map values;
		};

		godot::String base_dir;
		godot::Dictionary textures;
		godot::Dictionary local_stylesheets;
		int stylesheet_reads = 0;
		bool css_diagnostics_enabled = false;
		litehtml::css_source_kind css_source_kind = litehtml::css_source_kind::none;
		std::vector<godot::Dictionary> css_diagnostics;
		godot::HashMap<godot::String, int64_t> css_diagnostic_keys;
		godot::HashMap<const litehtml::element*, std::vector<int64_t>> element_diagnostics;
		bool fit_images;
		HTMLLayout::Fonts fonts;
		godot::Vector2i viewport_size;
		godot::Ref<godot::Font> default_font;
		godot::HashMap<godot::String, godot::Ref<godot::Font>> font_stacks;
		godot::HashMap<godot::String, godot::Ref<godot::Font>> installed_fonts;
		std::optional<godot::HashMap<godot::String, godot::String>> installed_font_names;
		godot::HashMap<godot::String, godot::Ref<godot::Texture2D>> images;
		godot::HashMap<const litehtml::element*, SourceAttributes> source_attributes;
		std::vector<PaintRun> paint_runs;
		std::vector<PaintRun> diagnostic_runs;
		std::vector<Clip> clips;
		godot::String plain_text;
		std::vector<TextRun> text_runs;
		godot::HashMap<godot::Vector2, std::vector<Text*>> painted_text;

		void index_text(const litehtml::element::ptr& p_element, godot::String& r_separator);

		godot::String get_url_dir(const char* p_css_base) const;
		godot::Ref<godot::Texture2D> load_texture(const char* p_src, const char* p_css_base);
		godot::Ref<godot::ShaderMaterial> create_material(const RoundedRect* p_shape = nullptr);
		void add_command(Command p_command, godot::Ref<godot::ShaderMaterial> p_material = {});
		void add_command_to(std::vector<PaintRun>& r_runs, Command p_command, godot::Ref<godot::ShaderMaterial> p_material = {});
		void fill_rect(const godot::Rect2& p_rect, const godot::Color& p_color);
		void draw_decoration(const godot::Rect2& p_rect, litehtml::text_decoration_style p_style, const godot::Color& p_color);
		litehtml::css_source get_css_source(const litehtml::string& p_base, bool p_inline) const override;
		void report_css_diagnostic(const litehtml::css_diagnostic& p_diagnostic, const litehtml::html_tag* p_element) override;
		void draw_gradient(const litehtml::background_layer& p_layer, const litehtml::background_layer::gradient_base& p_gradient,
			int p_type, const godot::Vector2& p_start, const godot::Vector2& p_parameters);

		litehtml::uint_ptr create_font(const litehtml::font_description& p_description, const litehtml::document*,
			litehtml::font_metrics* r_metrics) override;
		godot::Ref<godot::Font> resolve_font_stack(const litehtml::font_description& p_description);
		godot::Ref<godot::Font> find_installed_font(const godot::String& p_family, int p_weight, bool p_italic);
		void delete_font(litehtml::uint_ptr p_font) override;
		litehtml::pixel_t text_width(const char* p_text, litehtml::uint_ptr p_font) override;
		void draw_text(litehtml::uint_ptr, const char* p_text, litehtml::uint_ptr p_font,
			litehtml::web_color p_color, const litehtml::position& p_position) override;
		void draw_styled_text(litehtml::uint_ptr, const char* p_text, litehtml::uint_ptr p_font,
			litehtml::web_color p_color, const litehtml::position& p_position, const litehtml::element& p_element) override;
		void record_text(const char* p_text, litehtml::uint_ptr p_font, litehtml::web_color p_color,
			const litehtml::position& p_position, const std::vector<litehtml::box_shadow>& p_shadows);
		litehtml::pixel_t pt_to_px(float p_points) const override;
		litehtml::pixel_t get_default_font_size() const override;
		const char* get_default_font_name() const override;
		void draw_list_marker(litehtml::uint_ptr, const litehtml::list_marker& p_marker) override;
		void load_image(const char* p_src, const char* p_css_base, bool) override;
		void get_image_size(const char* p_src, const char* p_css_base, litehtml::size& r_size) override;
		void draw_image(litehtml::uint_ptr, const litehtml::background_layer& p_layer, const std::string& p_url,
			const std::string& p_css_base) override;
		void draw_solid_fill(litehtml::uint_ptr, const litehtml::background_layer& p_layer,
			const litehtml::web_color& p_color) override;
		void draw_box_shadows(const litehtml::html_tag& p_element, const litehtml::position& p_box,
			const litehtml::borders& p_borders, bool p_inset) override;
		void draw_borders(litehtml::uint_ptr, const litehtml::borders& p_borders,
			const litehtml::position& p_position, bool) override;
		void transform_text(litehtml::string& r_text, litehtml::text_transform p_transform) override;
		void set_clip(const litehtml::position& p_position, const litehtml::border_radiuses&) override;
		void del_clip() override;
		void get_viewport(litehtml::position& r_viewport) const override;
		void get_media_features(litehtml::media_features& r_media) const override;
		void get_language(litehtml::string& r_language, litehtml::string& r_culture) const override;

		void import_css(litehtml::string& r_text, const litehtml::string& p_url, litehtml::string& r_css_base) override;
		void set_base_url(const char*) override {}
		void set_caption(const char*) override {}
		void link(const std::shared_ptr<litehtml::document>& p_doc, const litehtml::element::ptr& p_element) override;
		void on_anchor_click(const char*, const litehtml::element::ptr&) override {}
		void on_mouse_event(const litehtml::element::ptr&, litehtml::mouse_event) override {}
		void set_cursor(const char*) override {}
		litehtml::element::ptr create_element(const char* p_tag, const litehtml::string_map&,
			const std::shared_ptr<litehtml::document>& p_document) override;

		void draw_linear_gradient(litehtml::uint_ptr, const litehtml::background_layer& p_layer,
			const litehtml::background_layer::linear_gradient& p_gradient) override;
		void draw_radial_gradient(litehtml::uint_ptr, const litehtml::background_layer& p_layer,
			const litehtml::background_layer::radial_gradient& p_gradient) override;
		void draw_conic_gradient(litehtml::uint_ptr, const litehtml::background_layer& p_layer,
			const litehtml::background_layer::conic_gradient& p_gradient) override;
	};
} // namespace docrender

#endif
