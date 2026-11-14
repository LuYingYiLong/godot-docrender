#ifndef DOCRENDER_LITEHTML_CONTAINER_H
#define DOCRENDER_LITEHTML_CONTAINER_H

#include "html_paint_recorder.h"
#include "html_resources.h"
#include "litehtml_fonts.h"

#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <litehtml/document_container.h>

#include <vector>

namespace docrender {
	class LiteHTMLContainer final : public litehtml::document_container {
	public:
		LiteHTMLContainer(
			const godot::String& p_base_dir,
			const godot::Dictionary& p_textures,
			bool p_fit_images,
			const HTMLFontConfig& p_fonts,
			const godot::Dictionary& p_local_stylesheets
		);

		void set_viewport(const godot::Vector2i& p_size);							// 更新排版和媒体查询使用的视口尺寸
		void record(
			litehtml::document& p_document,
			const godot::Vector2i& p_content_size,
			const HTMLGeometryIndex& p_geometry
		);																			// 记录已排版文档的绘制命令并建立可选择文本的索引
		void clear_recording() { paint.clear(); }
		const HTMLPaintRecorder& get_paint_recorder() const { return paint; }
		void record_css_diagnostic(const RenderGeometry& p_geometry, const godot::Rect2& p_box) {
			paint.record_css_diagnostic(p_geometry, p_box);
		}
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
		const std::vector<int64_t>* get_element_diagnostics(
			const litehtml::element& p_element
		) const;
		const godot::Dictionary& get_css_diagnostic(
			int64_t p_index
		) const;
	private:
		struct SourceAttributes {
			std::weak_ptr<litehtml::element> element;
			litehtml::string_map values;
		};

		HTMLResources resources;
		bool css_diagnostics_enabled = false;
		litehtml::css_source_kind css_source_kind = litehtml::css_source_kind::none;
		std::vector<godot::Dictionary> css_diagnostics;
		godot::HashMap<godot::String, int64_t> css_diagnostic_keys;
		godot::HashMap<const litehtml::element*, std::vector<int64_t>> element_diagnostics;
		bool fit_images;
		LiteHTMLFonts fonts;
		godot::Vector2i viewport_size;
		godot::HashMap<const litehtml::element*, SourceAttributes> source_attributes;
		HTMLPaintRecorder paint;

		litehtml::css_source get_css_source(const litehtml::string& p_base, bool p_inline) const override;
		void report_css_diagnostic(const litehtml::css_diagnostic& p_diagnostic, const litehtml::html_tag* p_element) override;
		litehtml::uint_ptr create_font(const litehtml::font_description& p_description, const litehtml::document*,
			litehtml::font_metrics* r_metrics) override;
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
		void set_render_item_clip(const litehtml::position& p_position, const litehtml::border_radiuses& p_radii,
			const litehtml::render_item& p_item) override;
		bool begin_render_item(const litehtml::render_item& p_item, const litehtml::position*& r_clip) override;
		void end_render_item() override;
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
