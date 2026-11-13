#include "html_label.h"

#include "formats/html/html_markup.h"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/h_scroll_bar.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <cmath>

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
		const Vector2i selection(std::min(selection_anchor, selection_caret), std::max(selection_anchor, selection_caret));
		const Color color = label->has_theme_color("selection_color", "HTMLLabel")
			? label->get_theme_color("selection_color", "HTMLLabel") : Color(0.25f, 0.55f, 0.95f, 0.5f);
		label->layout->draw_with_highlights(this, Vector2(), visible_rect, selection, nullptr, color);
	}
}

bool HTMLLabelCanvas::_has_point(const Vector2& p_point) const {
	return label && label->get_visible_rect().has_point(p_point);
}

void HTMLLabelCanvas::_gui_input(const Ref<InputEvent>& p_event) {
	if (!label) {
		return;
	}
	const Ref<InputEventKey> key = p_event;
	if (label->selection_enabled && key.is_valid() && key->is_pressed()) {
		if (key->is_action("ui_text_select_all", true)) {
			label->select_all();
			accept_event();
		}
		else if (key->is_action("ui_copy", true)) {
			const String text = label->get_selected_text();
			if (!text.is_empty()) {
				DisplayServer::get_singleton()->clipboard_set(text);
				accept_event();
			}
		}
		return;
	}
	const Rect2 visible_rect = label->get_visible_rect();
	const Ref<InputEventMouseMotion> motion = p_event;
	if (motion.is_valid()) {
		const Vector2 point = motion->get_position();
		const bool moved = (point - visible_rect.position).distance_squared_to(pressed_position) > DRAG_DEADZONE * DRAG_DEADZONE;
		if (!visible_rect.has_point(point) || moved) {
			pressed_link = 0;
		}
		if (selecting && (moved || selection_dragged)) {
			selection_dragged = true;
			const Vector2 clamped = point.clamp(visible_rect.position, visible_rect.get_end());
			selection_caret = label->layout->hit_test_text(clamped);
			set_process_internal(true);
			queue_redraw();
			accept_event();
		}
		update_cursor(motion->get_position());
		return;
	}

	const Ref<InputEventMouseButton> button = p_event;
	if (button.is_null() || button->get_button_index() != MOUSE_BUTTON_LEFT) {
		return;
	}
	const Vector2 point = button->get_position();
	const HTMLLayout::LinkHit hit = visible_rect.has_point(point) ? label->layout->find_link_at(point) : HTMLLayout::LinkHit{};
	if (button->is_pressed()) {
		pressed_position = point - visible_rect.position;
		pressed_link = hit.id;
		selecting = label->selection_enabled && visible_rect.has_point(point);
		selection_dragged = false;
		if (selecting) {
			selection_anchor = selection_caret = label->layout->hit_test_text(point);
			grab_focus();
			queue_redraw();
			accept_event();
		}
		return;
	}

	const bool clicked = !selection_dragged && pressed_link != 0 && pressed_link == hit.id &&
		(point - visible_rect.position).distance_squared_to(pressed_position) <= DRAG_DEADZONE * DRAG_DEADZONE;
	if (selecting) {
		if (selection_dragged && !button->is_canceled()) {
			selection_caret = label->layout->hit_test_text(point.clamp(visible_rect.position, visible_rect.get_end()));
			queue_redraw();
		}
		accept_event();
	}
	cancel_press();
	if (clicked && !button->is_canceled()) {
		if (hit.uri.begins_with("#")) {
			label->scroll_to_anchor(docrender::decode_html_fragment(hit.uri.substr(1)));
		}
		label->emit_signal("link_pressed", hit.uri);
	}
}

void HTMLLabelCanvas::_notification(int p_what) {
	if (p_what == NOTIFICATION_MOUSE_EXIT) {
		pressed_link = 0;
		set_default_cursor_shape(CURSOR_ARROW);
	}
	else if (p_what == NOTIFICATION_EXIT_TREE || p_what == NOTIFICATION_FOCUS_EXIT) {
		cancel_press();
		set_default_cursor_shape(CURSOR_ARROW);
	}
	else if (p_what == NOTIFICATION_INTERNAL_PROCESS && selecting && selection_dragged) {
		const Rect2 visible = label->get_visible_rect();
		const Vector2 point = get_local_mouse_position();
		const Vector2 clamped = point.clamp(visible.position, visible.get_end());
		const Vector2 outside = point - clamped;
		if (outside != Vector2()) {
			const Vector2i offset = label->get_scroll_offset();
			const Vector2 step = outside * std::min(1.0, get_process_delta_time() * 12.0);
			label->apply_scroll_offset(offset + Vector2i(step.round()));
			const Vector2i scrolled = label->get_scroll_offset() - offset;
			if (scrolled != Vector2i()) {
				selection_caret = label->layout->hit_test_text(clamped + Vector2(scrolled));
				queue_redraw();
			}
		}
	}
}

void HTMLLabelCanvas::cancel_press() {
	pressed_link = 0;
	selecting = false;
	selection_dragged = false;
	set_process_internal(false);
}

void HTMLLabelCanvas::update_cursor(const Vector2& p_point) {
	const bool over_link = _has_point(p_point) && label->layout->find_link_at(p_point).id != 0;
	set_default_cursor_shape(over_link && !selection_dragged ? CURSOR_POINTING_HAND :
		(label->selection_enabled ? CURSOR_IBEAM : CURSOR_ARROW));
}

HTMLLabel::HTMLLabel() {
	layout.instantiate();
	set_mouse_filter(MOUSE_FILTER_IGNORE);
}

void HTMLLabel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_document", "document"), &HTMLLabel::set_document);
	ClassDB::bind_method(D_METHOD("get_document"), &HTMLLabel::get_document);
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
	ClassDB::bind_method(D_METHOD("scroll_to_anchor", "id", "align"), &HTMLLabel::scroll_to_anchor, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("query_selector", "selector"), &HTMLLabel::query_selector);
	ClassDB::bind_method(D_METHOD("query_selector_all", "selector"), &HTMLLabel::query_selector_all);
	ClassDB::bind_method(D_METHOD("get_element_by_id", "id"), &HTMLLabel::get_element_by_id);
	ClassDB::bind_method(D_METHOD("get_element_at", "position"), &HTMLLabel::get_element_at);
	ClassDB::bind_method(D_METHOD("scroll_to_element", "element", "align"), &HTMLLabel::scroll_to_element, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("set_selection_enabled", "enabled"), &HTMLLabel::set_selection_enabled);
	ClassDB::bind_method(D_METHOD("is_selection_enabled"), &HTMLLabel::is_selection_enabled);
	ClassDB::bind_method(D_METHOD("select_all"), &HTMLLabel::select_all);
	ClassDB::bind_method(D_METHOD("clear_selection"), &HTMLLabel::clear_selection);
	ClassDB::bind_method(D_METHOD("get_selected_text"), &HTMLLabel::get_selected_text);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_dir", PROPERTY_HINT_DIR), "set_base_dir", "get_base_dir");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "document", PROPERTY_HINT_RESOURCE_TYPE, "HTMLDocument"), "set_document", "get_document");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "stylesheet", PROPERTY_HINT_MULTILINE_TEXT), "set_stylesheet", "get_stylesheet");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selection_enabled"), "set_selection_enabled", "is_selection_enabled");
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

void HTMLLabel::set_document(const Ref<HTMLDocument>& p_document) {
	if (document == p_document) {
		return;
	}
	if (document.is_valid()) {
		document->disconnect("changed", callable_mp(this, &HTMLLabel::document_changed).bind(document->get_instance_id()));
	}
	document = p_document;
	if (document.is_valid()) {
		document->connect("changed", callable_mp(this, &HTMLLabel::document_changed).bind(document->get_instance_id()), CONNECT_DEFERRED);
	}
	source_changed();
}

Ref<HTMLDocument> HTMLLabel::get_document() const {
	return document;
}

void HTMLLabel::set_text(const String& p_text) {
	if (text != p_text) {
		text = p_text;
		if (document.is_null()) {
			source_changed();
		}
	}
}

String HTMLLabel::get_text() const {
	return text;
}

void HTMLLabel::set_base_dir(const String& p_base_dir) {
	if (base_dir != p_base_dir) {
		base_dir = p_base_dir;
		if (document.is_null()) {
			source_changed();
		}
	}
}

String HTMLLabel::get_base_dir() const {
	return base_dir;
}

void HTMLLabel::set_stylesheet(const String& p_stylesheet) {
	if (stylesheet != p_stylesheet) {
		stylesheet = p_stylesheet;
		if (document.is_null()) {
			source_changed();
		}
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

bool HTMLLabel::scroll_to_anchor(const String& p_id, float p_align) {
	if (!is_inside_tree() || refresh_queued || viewport_size.x <= 0 || viewport_size.y <= 0) {
		return false;
	}
	const Rect2* anchor = layout->get_anchor_rects().getptr(p_id);
	return anchor && scroll_to_rect(*anchor, p_align);
}

Ref<HTMLElement> HTMLLabel::query_selector(const String& p_selector) const {
	return source_dirty ? Ref<HTMLElement>() : layout->query_selector(p_selector);
}

TypedArray<HTMLElement> HTMLLabel::query_selector_all(const String& p_selector) const {
	return source_dirty ? TypedArray<HTMLElement>() : layout->query_selector_all(p_selector);
}

Ref<HTMLElement> HTMLLabel::get_element_by_id(const String& p_id) const {
	return source_dirty ? Ref<HTMLElement>() : layout->get_element_by_id(p_id);
}

Ref<HTMLElement> HTMLLabel::get_element_at(const Vector2& p_position) const {
	if (!is_inside_tree() || refresh_queued || !p_position.is_finite() || !Rect2(Vector2(), get_size()).has_point(p_position)) {
		return {};
	}
	const Vector2 local = (canvas->get_global_transform().affine_inverse() * get_global_transform()).xform(p_position);
	return get_visible_rect().has_point(local) ? layout->get_element_at(local) : Ref<HTMLElement>();
}

bool HTMLLabel::scroll_to_element(const Ref<HTMLElement>& p_element, float p_align) {
	if (!is_inside_tree() || refresh_queued || viewport_size.x <= 0 || viewport_size.y <= 0) {
		return false;
	}
	const auto bounds = layout->get_element_scroll_bounds(p_element);
	return bounds && scroll_to_rect(*bounds, p_align);
}

bool HTMLLabel::scroll_to_rect(const Rect2& p_target, float p_align) {
	const Vector2i max_offset = (content_size - viewport_size).max(Vector2i());
	const float align = std::isfinite(p_align) ? std::clamp(p_align, 0.0f, 1.0f) : 0.0f;
	Vector2i offset = get_scroll_offset();
	if (p_target.position.x < offset.x || p_target.position.x >= static_cast<double>(offset.x) + viewport_size.x) {
		offset.x = static_cast<int32_t>(std::clamp<double>(p_target.position.x, 0.0, max_offset.x));
	}
	offset.y = static_cast<int32_t>(std::clamp<double>(p_target.position.y - viewport_size.y * align, 0.0, max_offset.y));
	set_scroll_offset(offset);
	return true;
}

void HTMLLabel::set_selection_enabled(bool p_enabled) {
	selection_enabled = p_enabled;
	if (canvas) {
		canvas->set_focus_mode(p_enabled ? FOCUS_CLICK : FOCUS_NONE);
		if (!p_enabled) {
			canvas->cancel_press();
			clear_selection();
		}
	}
}

bool HTMLLabel::is_selection_enabled() const {
	return selection_enabled;
}

void HTMLLabel::select_all() {
	if (selection_enabled && canvas) {
		canvas->selection_anchor = 0;
		canvas->selection_caret = layout->get_plain_text().length();
		canvas->queue_redraw();
	}
}

void HTMLLabel::clear_selection() {
	if (canvas) {
		canvas->selection_anchor = canvas->selection_caret = 0;
		canvas->queue_redraw();
	}
}

String HTMLLabel::get_selected_text() const {
	if (!canvas) {
		return {};
	}
	const int64_t from = std::min(canvas->selection_anchor, canvas->selection_caret);
	const int64_t to = std::max(canvas->selection_anchor, canvas->selection_caret);
	return layout->get_plain_text().substr(from, to - from);
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
	canvas->set_focus_mode(selection_enabled ? FOCUS_CLICK : FOCUS_NONE);
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
	layout->clear();
	pending_scroll_offset = Vector2i();
	if (canvas) {
		canvas->cancel_press();
		clear_selection();
	}
	queue_refresh();
}

void HTMLLabel::document_changed(uint64_t p_document_id) {
	if (document.is_valid() && document->get_instance_id() == p_document_id) {
		source_changed();
	}
}

void HTMLLabel::refresh() {
	refresh_queued = false;
	if (!is_inside_tree()) {
		return;
	}
	canvas->pressed_link = 0;
	if (!pending_scroll_offset) {
		pending_scroll_offset = get_scroll_offset();
	}

	if (source_dirty) {
		source_dirty = false;
		if (document.is_valid()) {
			layout = document->create_layout();
			parsed = layout->get_render_error().is_empty();
		}
		else {
			parsed = layout->parse(text, base_dir, stylesheet) == OK;
		}
	}
	if (!parsed) {
		update_view({}, {}, false, false, layout->get_render_error());
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
		if (layout->layout(viewport) != OK) {
			update_view({}, {}, false, false, layout->get_render_error());
			return;
		}
		const Vector2i size = layout->get_content_size();
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
	const int64_t text_length = layout->get_plain_text().length();
	canvas->selection_anchor = std::min(canvas->selection_anchor, text_length);
	canvas->selection_caret = std::min(canvas->selection_caret, text_length);
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
	canvas->pressed_link = 0;
	canvas->queue_redraw();
}

void HTMLLabel::on_content_sorted() {
	canvas->queue_redraw();
	canvas->update_cursor(canvas->get_local_mouse_position());
}
