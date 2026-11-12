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
	uintptr_t pressed_link = 0;

	void cancel_press();
	void update_cursor(const godot::Vector2& p_point);

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
	godot::ScrollContainer* scroll_container = nullptr;
	HTMLLabelCanvas* canvas = nullptr;
	godot::String text;
	godot::String base_dir = "res://";
	godot::String stylesheet;
	godot::Vector2i viewport_size;
	godot::Vector2i content_size;
	godot::String render_error;
	std::optional<godot::Vector2i> pending_scroll_offset;
	bool source_dirty = true;
	bool parsed = false;
	bool refresh_queued = false;

	void ensure_controls();
	void queue_refresh();
	void source_changed();
	void refresh();
	void update_view(const godot::Vector2i& p_viewport, const godot::Vector2i& p_content,
		bool p_scroll_horizontal, bool p_scroll_vertical, const godot::String& p_error = {});
	godot::Rect2 get_visible_rect() const;
	void apply_scroll_offset(const godot::Vector2i& p_offset);
	void apply_pending_scroll();
	void on_scroll_changed(double p_value);
	void on_content_sorted();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	HTMLLabel();

	void set_text(const godot::String& p_text);
	godot::String get_text() const;
	void set_base_dir(const godot::String& p_base_dir);
	godot::String get_base_dir() const;
	void set_stylesheet(const godot::String& p_stylesheet);
	godot::String get_stylesheet() const;
	godot::Vector2i get_content_size() const;
	godot::String get_render_error() const;
	godot::Vector2i get_scroll_offset() const;
	void set_scroll_offset(const godot::Vector2i& p_offset);
	godot::PackedStringArray _get_configuration_warnings() const override;
};
