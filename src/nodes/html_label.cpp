#include "html_label.h"

#include <godot_cpp/classes/h_scroll_bar.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>

using namespace godot;

namespace {
	constexpr int DRAG_DEADZONE = 6;
}

void HTMLLabelCanvas::_draw() {
	if (!label) {
		return;
	}
	const Rect2 visible_rect = label->get_visible_rect();
	if (visible_rect.has_area()) {
		label->document->draw(this, Vector2(), visible_rect);
	}
}

bool HTMLLabelCanvas::_has_point(const Vector2& p_point) const {
	return label && label->get_visible_rect().has_point(p_point);
}

void HTMLLabelCanvas::_gui_input(const Ref<InputEvent>& p_event) {
	if (!label) {
		return;
	}
	const Rect2 visible_rect = label->get_visible_rect();
	const Ref<InputEventMouseMotion> motion = p_event;
	if (motion.is_valid()) {
		if (!visible_rect.has_point(motion->get_position()) ||
			(motion->get_position() - visible_rect.position).distance_squared_to(pressed_position) >
			DRAG_DEADZONE * DRAG_DEADZONE) {
			cancel_press();
		}
		update_cursor(motion->get_position());
		return;
	}

	const Ref<InputEventMouseButton> button = p_event;
	if (button.is_null() || button->get_button_index() != MOUSE_BUTTON_LEFT) {
		return;
	}
	const Vector2 point = button->get_position();
	const HTMLDocument::LinkHit hit = visible_rect.has_point(point) ? label->document->find_link_at(point) : HTMLDocument::LinkHit{};
	if (button->is_pressed()) {
		pressed_position = point - visible_rect.position;
		pressed_link = hit.id;
		return;
	}

	const bool clicked = pressed_link != 0 && pressed_link == hit.id &&
		(point - visible_rect.position).distance_squared_to(pressed_position) <= DRAG_DEADZONE * DRAG_DEADZONE;
	cancel_press();
	if (clicked && !button->is_canceled()) {
		label->emit_signal("link_pressed", hit.uri);
	}
}

void HTMLLabelCanvas::_notification(int p_what) {
	if (p_what == NOTIFICATION_MOUSE_EXIT || p_what == NOTIFICATION_EXIT_TREE) {
		cancel_press();
		set_default_cursor_shape(CURSOR_ARROW);
	}
}

void HTMLLabelCanvas::cancel_press() {
	pressed_link = 0;
}

void HTMLLabelCanvas::update_cursor(const Vector2& p_point) {
	const bool over_link = _has_point(p_point) && label->document->find_link_at(p_point).id != 0;
	set_default_cursor_shape(over_link ? CURSOR_POINTING_HAND : CURSOR_ARROW);
}

HTMLLabel::HTMLLabel() {
	document.instantiate();
	set_mouse_filter(MOUSE_FILTER_IGNORE);
}

void HTMLLabel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_text", "text"), &HTMLLabel::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &HTMLLabel::get_text);
	ClassDB::bind_method(D_METHOD("set_base_dir", "base_dir"), &HTMLLabel::set_base_dir);
	ClassDB::bind_method(D_METHOD("get_base_dir"), &HTMLLabel::get_base_dir);
	ClassDB::bind_method(D_METHOD("set_stylesheet", "stylesheet"), &HTMLLabel::set_stylesheet);
	ClassDB::bind_method(D_METHOD("get_stylesheet"), &HTMLLabel::get_stylesheet);
	ClassDB::bind_method(D_METHOD("get_content_size"), &HTMLLabel::get_content_size);
	ClassDB::bind_method(D_METHOD("get_render_error"), &HTMLLabel::get_render_error);
	ClassDB::bind_method(D_METHOD("get_scroll_offset"), &HTMLLabel::get_scroll_offset);
	ClassDB::bind_method(D_METHOD("set_scroll_offset", "offset"), &HTMLLabel::set_scroll_offset);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_dir", PROPERTY_HINT_DIR), "set_base_dir", "get_base_dir");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "stylesheet", PROPERTY_HINT_MULTILINE_TEXT), "set_stylesheet", "get_stylesheet");
	ADD_SIGNAL(MethodInfo("link_pressed", PropertyInfo(Variant::STRING, "uri")));
}

void HTMLLabel::_notification(int p_what) {
	switch (p_what) {
	case NOTIFICATION_POST_ENTER_TREE:
		ensure_controls();
		queue_refresh();
		break;
	case NOTIFICATION_RESIZED:
	case NOTIFICATION_THEME_CHANGED:
		queue_refresh();
		break;
	}
}

void HTMLLabel::set_text(const String& p_text) {
	if (text != p_text) {
		text = p_text;
		source_changed();
	}
}

String HTMLLabel::get_text() const {
	return text;
}

void HTMLLabel::set_base_dir(const String& p_base_dir) {
	if (base_dir != p_base_dir) {
		base_dir = p_base_dir;
		source_changed();
	}
}

String HTMLLabel::get_base_dir() const {
	return base_dir;
}

void HTMLLabel::set_stylesheet(const String& p_stylesheet) {
	if (stylesheet != p_stylesheet) {
		stylesheet = p_stylesheet;
		source_changed();
	}
}

String HTMLLabel::get_stylesheet() const {
	return stylesheet;
}

Vector2i HTMLLabel::get_content_size() const {
	return content_size;
}

String HTMLLabel::get_render_error() const {
	return render_error;
}

Vector2i HTMLLabel::get_scroll_offset() const {
	return scroll_container ? Vector2i(scroll_container->get_h_scroll(), scroll_container->get_v_scroll()) : Vector2i();
}

void HTMLLabel::set_scroll_offset(const Vector2i& p_offset) {
	if (!is_inside_tree() || !scroll_container || refresh_queued || pending_scroll_offset) {
		pending_scroll_offset = p_offset;
		return;
	}
	apply_scroll_offset(p_offset);
}

PackedStringArray HTMLLabel::_get_configuration_warnings() const {
	PackedStringArray warnings;
	if (!render_error.is_empty()) {
		warnings.append(render_error);
	}
	return warnings;
}

void HTMLLabel::ensure_controls() {
	if (scroll_container) {
		return;
	}

	scroll_container = memnew(ScrollContainer);
	scroll_container->set_name("_HTMLScroll");
	scroll_container->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_SHOW_NEVER);
	scroll_container->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_SHOW_NEVER);
	scroll_container->set_deadzone(DRAG_DEADZONE);
	// Keep the visible rectangle aligned with ScrollContainer's scrollbar page sizes.
	scroll_container->add_theme_constant_override("scrollbar_h_separation", 0);
	scroll_container->add_theme_constant_override("scrollbar_v_separation", 0);
	add_child(scroll_container, false, INTERNAL_MODE_BACK);
	scroll_container->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	scroll_container->connect("resized", callable_mp(this, &HTMLLabel::queue_refresh));
	scroll_container->connect("sort_children", callable_mp(this, &HTMLLabel::on_content_sorted));
	scroll_container->get_h_scroll_bar()->connect("value_changed", callable_mp(this, &HTMLLabel::on_scroll_changed));
	scroll_container->get_v_scroll_bar()->connect("value_changed", callable_mp(this, &HTMLLabel::on_scroll_changed));

	canvas = memnew(HTMLLabelCanvas);
	canvas->set_name("_HTMLCanvas");
	canvas->label = this;
	canvas->set_mouse_filter(MOUSE_FILTER_PASS);
	canvas->set_h_size_flags(SIZE_EXPAND_FILL);
	canvas->set_v_size_flags(SIZE_EXPAND_FILL);
	scroll_container->add_child(canvas, false, INTERNAL_MODE_BACK);
}

void HTMLLabel::queue_refresh() {
	if (canvas && is_inside_tree() && !refresh_queued) {
		refresh_queued = true;
		callable_mp(this, &HTMLLabel::refresh).call_deferred();
	}
}

void HTMLLabel::source_changed() {
	source_dirty = true;
	pending_scroll_offset = Vector2i();
	if (canvas) {
		canvas->cancel_press();
	}
	queue_refresh();
}

void HTMLLabel::refresh() {
	refresh_queued = false;
	if (!is_inside_tree()) {
		return;
	}
	canvas->cancel_press();
	if (!pending_scroll_offset) {
		pending_scroll_offset = get_scroll_offset();
	}

	if (source_dirty) {
		source_dirty = false;
		parsed = document->parse(text, base_dir, stylesheet) == OK;
	}
	if (!parsed) {
		update_view({}, {}, false, false, document->get_render_error());
		return;
	}

	const Ref<StyleBox> panel = scroll_container->get_theme_stylebox("panel");
	const Vector2 available = scroll_container->get_size() - (panel.is_valid() ? panel->get_minimum_size() : Vector2());
	const real_t vertical_bar_width = scroll_container->get_v_scroll_bar()->get_combined_minimum_size().x;
	const real_t horizontal_bar_height = scroll_container->get_h_scroll_bar()->get_combined_minimum_size().y;
	bool horizontal = false;
	bool vertical = false;

	// Adding each axis at most once bounds reflow to three layouts, even for responsive CSS.
	for (;;) {
		const Vector2 gutters(vertical ? vertical_bar_width : 0, horizontal ? horizontal_bar_height : 0);
		const Vector2i viewport((available - gutters).floor());
		if (viewport.x <= 0 || viewport.y <= 0) {
			update_view({}, {}, false, false);
			return;
		}
		if (document->layout(viewport) != OK) {
			update_view({}, {}, false, false, document->get_render_error());
			return;
		}
		const Vector2i size = document->get_content_size();
		const bool next_horizontal = horizontal || size.x > viewport.x;
		const bool next_vertical = vertical || size.y > viewport.y;
		if (next_horizontal == horizontal && next_vertical == vertical) {
			update_view(viewport, size, horizontal, vertical);
			return;
		}
		horizontal = next_horizontal;
		vertical = next_vertical;
	}
}

void HTMLLabel::update_view(const Vector2i& p_viewport, const Vector2i& p_content,
	bool p_scroll_horizontal, bool p_scroll_vertical, const String& p_error) {
	viewport_size = p_viewport;
	content_size = p_content;
	canvas->set_custom_minimum_size(Vector2(content_size));
	scroll_container->set_horizontal_scroll_mode(p_scroll_horizontal ? ScrollContainer::SCROLL_MODE_SHOW_ALWAYS : ScrollContainer::SCROLL_MODE_SHOW_NEVER);
	scroll_container->set_vertical_scroll_mode(p_scroll_vertical ? ScrollContainer::SCROLL_MODE_SHOW_ALWAYS : ScrollContainer::SCROLL_MODE_SHOW_NEVER);
	scroll_container->queue_sort();
	// Apply the requested offset after the native container updates its scrollbar ranges.
	callable_mp(this, &HTMLLabel::apply_pending_scroll).call_deferred();
	if (render_error != p_error) {
		render_error = p_error;
		update_configuration_warnings();
	}
}

Rect2 HTMLLabel::get_visible_rect() const {
	return Rect2(Vector2(get_scroll_offset()), Vector2(viewport_size));
}

void HTMLLabel::apply_scroll_offset(const Vector2i& p_offset) {
	const Vector2i max_offset = (content_size - viewport_size).max(Vector2i());
	const Vector2i offset = p_offset.clamp(Vector2i(), max_offset);
	scroll_container->set_h_scroll(offset.x);
	scroll_container->set_v_scroll(offset.y);
}

void HTMLLabel::apply_pending_scroll() {
	if (is_inside_tree() && !refresh_queued && pending_scroll_offset) {
		const Vector2i offset = *pending_scroll_offset;
		pending_scroll_offset.reset();
		apply_scroll_offset(offset);
	}
}

void HTMLLabel::on_scroll_changed(double) {
	canvas->cancel_press();
}

void HTMLLabel::on_content_sorted() {
	canvas->queue_redraw();
	canvas->update_cursor(canvas->get_local_mouse_position());
}
