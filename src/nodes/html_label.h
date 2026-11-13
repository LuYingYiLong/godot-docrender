#pragma once

#include "formats/html/html_document.h"

#include <godot_cpp/classes/control.hpp>

#include <optional>

namespace godot {
	class ScrollContainer;
}

class HTMLLabel;

class HTMLLabelCanvas : public godot::Control {
	GDCLASS(HTMLLabelCanvas, godot::Control)

	friend class HTMLLabel;

	HTMLLabel* label = nullptr;
	godot::Vector2 pressed_position;
	godot::Ref<HTMLElement> pressed_element;
	uintptr_t pressed_link = 0;
	int64_t selection_anchor = 0;
	int64_t selection_caret = 0;
	bool selecting = false;
	bool selection_dragged = false;

	void cancel_press();
	void cancel_click();
	void update_cursor(const std::optional<godot::Vector2>& p_point);

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	void _draw() override;
	bool _has_point(const godot::Vector2& p_point) const override;
	void _gui_input(const godot::Ref<godot::InputEvent>& p_event) override;
};

class HTMLLabel : public godot::Control {
	GDCLASS(HTMLLabel, godot::Control)

	friend class HTMLLabelCanvas;

	godot::Ref<HTMLDocument> document;
	godot::Ref<HTMLLayout> layout;
	godot::ScrollContainer* scroll_container = nullptr;
	HTMLLabelCanvas* canvas = nullptr;
	godot::String text;
	godot::String base_dir = "res://";
	godot::String stylesheet;
	godot::Vector2i viewport_size;
	godot::Vector2i content_size;
	godot::Vector2i sorted_scroll_offset;
	godot::String render_error;
	std::optional<godot::Vector2i> pending_scroll_offset;
	bool source_dirty = true;
	bool parsed = false;
	bool refresh_queued = false;
	bool layout_updated = false;
	bool layout_signal_pending = false;
	bool selection_enabled = true;
	bool view_update_pending = false;
	bool scroll_sort_pending = false;
	bool interaction_queued = false;
	bool gui_hover_dirty = false;
	bool updating_gui_hover = false;
	bool hover_reflow = false;
	int hover_passes_remaining = 0;

	void ensure_controls();
	void queue_refresh();
	void source_changed();
	void document_changed(uint64_t p_document_id);
	void parse_pending_source();
	void on_layout_edited();
	void refresh();
	void update_view(const godot::Vector2i& p_viewport, const godot::Vector2i& p_content,
		bool p_scroll_horizontal, bool p_scroll_vertical, bool p_layout_changed, const godot::String& p_error = {});
	godot::Rect2 get_visible_rect() const;
	bool scroll_to_rect(const godot::Rect2& p_target, float p_align);
	void apply_scroll_offset(const godot::Vector2i& p_offset);
	void apply_pending_scroll();
	void on_scroll_changed(double p_value);
	void on_content_sorted();
	void request_hover_check();
	void queue_interaction();
	void update_interaction();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	HTMLLabel();

	godot::PackedStringArray _get_configuration_warnings() const override;

	void set_document(const godot::Ref<HTMLDocument>& p_document);					// 设置用于显示的 HTML 文档资源
	godot::Ref<HTMLDocument> get_document() const;									// 返回控件当前关联的文档资源
	void set_text(const godot::String& p_text);										// 设置控件的内联 HTML 文本
	godot::String get_text() const;													// 返回控件的内联 HTML 文本
	void set_base_dir(const godot::String& p_base_dir);								// 设置内联 HTML 的相对资源路径基准目录
	godot::String get_base_dir() const;												// 返回内联 HTML 的资源基准目录
	void set_stylesheet(const godot::String& p_stylesheet);							// 设置应用于内联 HTML 的附加 CSS
	godot::String get_stylesheet() const;											// 返回内联 HTML 的附加 CSS
	godot::Vector2i get_content_size() const;										// 返回排版后的内容尺寸
	godot::String get_render_error() const;											// 返回最近的解析或排版错误
	godot::Vector2i get_scroll_offset() const;										// 返回横向和纵向的文档滚动偏移
	void set_scroll_offset(const godot::Vector2i& p_offset);						// 设置滚动偏移并限制在可滚动范围内
	bool scroll_to_anchor(const godot::String& p_id, float p_align = 0.0f);			// 将指定 HTML ID 的锚点滚到视口内的对齐位置
	godot::Ref<HTMLElement> create_element(const godot::String& p_tag_name);		// 创建可插入当前控件文档的元素
	godot::Ref<HTMLElement> query_selector(const godot::String& p_selector) const;	// 返回当前控件文档中第一个匹配元素
	godot::TypedArray<HTMLElement> query_selector_all(
		const godot::String& p_selector
	) const;																		// 返回当前控件文档中的全部匹配元素
	godot::Ref<HTMLElement> get_element_by_id(const godot::String& p_id) const;		// 按区分大小写的 HTML ID 查找当前控件文档中的第一个元素
	godot::Ref<HTMLElement> get_element_at(const godot::Vector2& p_position) const; // 将控件局部坐标换算为文档坐标并命中可见元素
	bool scroll_to_element(
		const godot::Ref<HTMLElement>& p_element,
		float p_align = 0.0f
	);																				// 将当前控件的目标元素滚到视口内的对齐位置
	void set_selection_enabled(bool p_enabled);										// 设置是否允许选择文本
	bool is_selection_enabled() const;												// 返回是否允许选择文本
	void select_all();																// 选中全部可复制文本
	void clear_selection();															// 清除当前文本选区
	godot::String get_selected_text() const;										// 返回当前选中的文本
};
