#ifndef DOCRENDER_HTML_LAYOUT_H
#define DOCRENDER_HTML_LAYOUT_H

#include "html_element.h"

#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace godot {
	class CanvasItem;
	class StyleBox;
}

class HTMLLayout : public godot::RefCounted {
	GDCLASS(HTMLLayout, godot::RefCounted)

	friend class HTMLElement;
	friend class HTMLLabel;
	friend class HTMLLabelCanvas;

	struct State;
	std::unique_ptr<State> state;
	uint64_t parse_version = 0;

	enum class SelectorQuery {
		FIRST,
		ALL,
		SELF,
		CLOSEST
	};

	bool initialize_document(const godot::CharString& p_html);
	godot::Error reflow(const godot::Vector2i& p_viewport_size);
	godot::Ref<HTMLElement> make_element(const std::shared_ptr<litehtml::element>& p_element) const;
	std::shared_ptr<HTMLElement::DetachedTree> get_detached_tree(const litehtml::element& p_element) const;
	godot::TypedArray<HTMLElement> select_elements(const godot::String& p_selector, SelectorQuery p_query,
		const std::shared_ptr<litehtml::element>& p_scope = {}) const;
	godot::Dictionary get_element_attributes(const litehtml::element& p_element) const;
	const char* get_element_attribute(const litehtml::element& p_element, const char* p_name) const;
	godot::String serialize_children(const litehtml::element& p_element) const;
	godot::Error edit_attribute(const std::shared_ptr<litehtml::element>& p_element, const char* p_name, const char* p_value);
	godot::Error replace_children(const std::shared_ptr<litehtml::element>& p_element, const godot::String& p_content, bool p_parse_html);
	godot::Error move_element(const std::shared_ptr<litehtml::element>& p_element, const std::shared_ptr<litehtml::element>& p_parent);
	void forget_subtree(const litehtml::element& p_element);
	void invalidate_styles();
	void queue_reflow();
	static void reflow_after_edit(uint64_t p_id, uint64_t p_version);
	void set_edit_callback(const godot::Callable& p_callback);
	godot::TypedArray<godot::Rect2> get_element_rects(const std::shared_ptr<litehtml::element>& p_element) const;
	std::shared_ptr<litehtml::element> hit_test_element(const godot::Vector2& p_position) const;
	godot::Ref<HTMLElement> wrap_hit_element(std::shared_ptr<litehtml::element> p_element) const;
	bool update_hover(const std::optional<godot::Vector2>& p_position);
	std::string_view get_cursor_at(const godot::Vector2& p_position, bool& r_over_link) const;
	void record_css_diagnostics();
	godot::TypedArray<godot::Dictionary> get_css_diagnostics_for(const litehtml::element& p_element) const;
	godot::String get_css_diagnostic_tooltip(const godot::Vector2& p_position) const;
protected:
	static void _bind_methods();

public:
	void set_css_diagnostics_enabled(bool p_enabled);								// 设置是否收集 CSS 声明诊断
	bool is_css_diagnostics_enabled() const;										// 返回是否收集 CSS 声明诊断
	godot::TypedArray<godot::Dictionary> get_css_diagnostics() const;				// 返回最近一轮样式计算的诊断快照

	struct Fonts {
		godot::Ref<godot::Font> text;
		godot::Ref<godot::Font> bold;
		godot::Ref<godot::Font> italic;
		godot::Ref<godot::Font> bold_italic;
		godot::Ref<godot::Font> code;
		godot::Ref<godot::Font> code_block;
	};

	struct LinkHit {
		godot::String uri;
		uintptr_t id = 0;
	};

	HTMLLayout();
	~HTMLLayout() override;

	godot::Error parse(const godot::String& p_html,
		const godot::String& p_base_dir = "res://",
		const godot::String& p_stylesheet = ""
	);																				// 解析 HTML 和附加样式，替换当前布局的文档
	godot::Error parse_source(const godot::String& p_html,
		const godot::String& p_base_dir,
		const godot::String& p_stylesheet,
		const godot::Dictionary& p_textures,
		bool p_fit_images = false,
		const godot::String& p_default_stylesheet = {},
		const Fonts& p_fonts = {},
		const godot::Dictionary& p_local_stylesheets = {}
	);																				// 使用指定字体、样式表和资源依赖解析 HTML 并替换当前文档
	godot::Error layout(const godot::Vector2i& p_viewport_size);					// 按给定视口尺寸排版并更新几何
	godot::Vector2i get_content_size() const;										// 返回最近一次成功排版的内容尺寸
	godot::String get_plain_text() const;											// 返回排版后的可复制文本
	int64_t hit_test_text(const godot::Vector2& p_position) const;					// 返回离指定文档坐标最近的文本光标偏移
	godot::TypedArray<godot::Rect2> get_selection_rects(
		int64_t p_from,
		int64_t p_to
	) const;																		// 返回文本范围在文档坐标中的可见选区矩形
	void draw(godot::CanvasItem* p_canvas,
		const godot::Vector2& p_position = {},
		const godot::Rect2& p_clip = {}
	);																				// 在画布绘制回调中按给定位置和裁剪区域绘制 HTML
	void draw_with_highlights(
		godot::CanvasItem* p_canvas,
		const godot::Vector2& p_position,
		const godot::Rect2& p_clip,
		const godot::Vector2i& p_selection,
		const godot::StyleBox* p_selection_style,
		const godot::Color& p_selection_color,
		const godot::Vector2i& p_search = {},
		const godot::StyleBox* p_search_style = nullptr
	);																				// 在指定画布上绘制 HTML，并显示选区与搜索高亮
	godot::String get_link_at(const godot::Vector2& p_position) const;				// 返回指定文档坐标处链接的原始 href
	LinkHit find_link_at(
		const godot::Vector2& p_position,
		godot::Ref<HTMLElement>* r_element = nullptr
	) const;																		// 命中文档坐标处的链接，可同时写出命中的元素
	godot::Rect2 get_anchor_rect(const godot::String& p_id) const;					// 返回指定 HTML ID 的锚点矩形
	const godot::HashMap<godot::String, godot::Rect2>& get_anchor_rects() const;	// 返回当前布局中各 HTML ID 对应的锚点矩形
	godot::Ref<HTMLElement> create_element(const godot::String& p_tag_name);		// 创建同一布局中尚未插入文档的元素
	godot::Ref<HTMLElement> query_selector(const godot::String& p_selector) const;	// 按文档顺序返回第一个匹配元素
	godot::TypedArray<HTMLElement> query_selector_all(
		const godot::String& p_selector
	) const;																		// 按文档顺序返回所有匹配元素的去重数组
	godot::Ref<HTMLElement> get_element_by_id(const godot::String& p_id) const;		// 按区分大小写的 HTML ID 查找第一个元素
	godot::Ref<HTMLElement> get_element_at(const godot::Vector2& p_position) const; // 返回文档坐标处考虑裁剪后的最上层元素
	std::optional<godot::Rect2> get_element_scroll_bounds(
		const godot::Ref<HTMLElement>& p_element
	) const; // 获取同一布局中目标元素可用于滚动定位的边界
	godot::String get_render_error() const;											// 返回最近的解析或排版错误
	void clear();																	// 清空文档及排版结果并使元素句柄失效
};

#endif
