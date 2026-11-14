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
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string_view>
#include <utility>

using namespace godot;

namespace {
	constexpr int DRAG_DEADZONE = 6;
	constexpr int HOVER_PASSES = 2;
	constexpr const char* BUTTON_STYLES = R"CSS(
button[type="button" i] {
	color: #1f2937;
	background-color: #f3f4f6;
	border: 1px solid #64748b;
	border-radius: 4px;
	padding: 4px 10px;
	cursor: pointer;
}
button[type="button" i]:active { background-color: #cbd5e1; }
button[type="button" i]:focus-visible { box-shadow: inset 0 0 0 2px #2563eb; }
button[type="button" i]:disabled {
	color: #4b5563;
	background-color: #e5e7eb;
	border-color: #9ca3af;
	cursor: default;
}
button[type="button" i][hidden] { display: none; }
)CSS";

	std::optional<bool> tab_direction(const InputEventKey& p_key) {
		if (p_key.is_action_pressed("ui_focus_prev", true, true)) {
			return true;
		}
		if (p_key.is_action_pressed("ui_focus_next", true, true)) {
			return false;
		}
		return std::nullopt;
	}

	std::optional<Control::CursorShape> get_css_cursor(std::string_view p_cursor) {
		static constexpr std::pair<std::string_view, Control::CursorShape> shapes[] = {
			{ "default", Control::CURSOR_ARROW },
			{ "pointer", Control::CURSOR_POINTING_HAND },
			{ "text", Control::CURSOR_IBEAM },
			{ "crosshair", Control::CURSOR_CROSS },
			{ "move", Control::CURSOR_MOVE },
			{ "wait", Control::CURSOR_WAIT },
			{ "progress", Control::CURSOR_BUSY },
			{ "help", Control::CURSOR_HELP },
			{ "not-allowed", Control::CURSOR_FORBIDDEN },
			{ "ew-resize", Control::CURSOR_HSIZE },
			{ "ns-resize", Control::CURSOR_VSIZE },
		};
		for (const auto& [keyword, shape] : shapes) {
			if (std::equal(p_cursor.begin(), p_cursor.end(), keyword.begin(), keyword.end(), [](char p_char, char p_lowercase) {
				return p_char == p_lowercase || (p_char >= 'A' && p_char <= 'Z' && p_char - 'A' + 'a' == p_lowercase);
			})) {
				return shape;
			}
		}
		return std::nullopt;
	}
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

String HTMLLabelCanvas::_get_tooltip(const Vector2& p_position) const {
	return !selecting && _has_point(p_position) ? label->layout->get_css_diagnostic_tooltip(p_position) : String();
}

Object* HTMLLabelCanvas::_make_custom_tooltip(const String& p_for_text) const {
	if (p_for_text.is_empty() || !label || !label->is_css_diagnostics_enabled()) {
		return nullptr;
	}
	const Vector2 viewport_size = get_viewport_rect().size;
	const int width = std::max(1, int(std::min<real_t>(380.0, viewport_size.x * 0.8)));
	const int max_height = std::max(1, int(std::min<real_t>(320.0, viewport_size.y * 0.6)));
	const String css = R"CSS(
		body { margin: 0; padding: 8px 12px; background: #23272e; color: #f5f5f5; font-size: 13px; }
		.element { font-weight: bold; padding-bottom: 6px; }
		.diagnostic { border-top: 1px solid #51565f; padding: 6px 0; }
		.kind { color: #ffba65; }
		.source { color: #b8bdc7; font-size: 12px; }
	)CSS";
	const String html = "<!doctype html>" + p_for_text;
	// Size the tooltip from its HTML before Godot shrinks the popup to its minimum size.
	Ref<HTMLLayout> measure;
	measure.instantiate();
	if (measure->parse(html, "res://", css) != OK || measure->layout(Vector2i(width, max_height)) != OK) {
		return nullptr;
	}
	HTMLLabel* panel = memnew(HTMLLabel);
	panel->set_name("CSSDiagnostics");
	panel->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	panel->set_selection_enabled(false);
	panel->set_stylesheet(css);
	panel->set_text(html);
	panel->set_custom_minimum_size(Vector2(width, std::clamp(measure->get_content_size().y, 1, max_height)));
	diagnostic_panel_id = ObjectID(panel->get_instance_id());
	diagnostic_panel_text = p_for_text;
	return panel;
}

void HTMLLabelCanvas::refresh_diagnostic_tooltip(const std::optional<Vector2>& p_point) {
	if (diagnostic_panel_id == ObjectID()) {
		return;
	}
	auto* panel = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(diagnostic_panel_id));
	if (panel && p_point && _get_tooltip(*p_point) == diagnostic_panel_text) {
		return;
	}
	diagnostic_panel_id = ObjectID();
	diagnostic_panel_text = String();
	// Godot owns the popup. Hide it when its contents change without mouse motion.
	if (panel) {
		if (auto* popup = Object::cast_to<Window>(panel->get_parent())) {
			popup->hide();
		}
	}
}

void HTMLLabelCanvas::_gui_input(const Ref<InputEvent>& p_event) {
	if (!label) {
		return;
	}
	const Rect2 visible_rect = label->get_visible_rect();
	const Ref<InputEventMouseMotion> motion = p_event;
	if (motion.is_valid()) {
		const Vector2 point = motion->get_position();
		const bool moved = (point - visible_rect.position).distance_squared_to(pressed_position) > DRAG_DEADZONE * DRAG_DEADZONE;
		if (!visible_rect.has_point(point) || (selecting && moved)) {
			cancel_click();
		}
		else {
			if (moved) {
				pressed_link = 0;
				pressed_element.unref();
			}
			if (pressed_button.is_valid() &&
				label->layout->get_button_ancestor(label->layout->get_element_at(point)) != pressed_button) {
				cancel_button_press();
			}
		}
		if (selecting && (moved || selection_dragged)) {
			selection_dragged = true;
			const Vector2 clamped = point.clamp(visible_rect.position, visible_rect.get_end());
			selection_caret = label->layout->hit_test_text(clamped);
			set_process_internal(true);
			queue_redraw();
			accept_event();
		}
		label->request_hover_check();
		return;
	}

	const Ref<InputEventMouseButton> button = p_event;
	if (button.is_null() || button->get_button_index() != MOUSE_BUTTON_LEFT) {
		return;
	}
	const Vector2 point = button->get_position();
	Ref<HTMLElement> element;
	const HTMLLayout::LinkHit hit = visible_rect.has_point(point)
		? label->layout->find_link_at(point, &element) : HTMLLayout::LinkHit{};
	const Ref<HTMLElement> target_button = label->layout->get_button_ancestor(element);
	if (button->is_pressed()) {
		cancel_button_press();
		cancel_key_press();
		pressed_position = point - visible_rect.position;
		pressed_link = hit.id;
		pressed_element = element;
		selecting = label->selection_enabled && visible_rect.has_point(point);
		selection_dragged = false;
		if (label->layout->is_enabled_button(target_button)) {
			label->grab_focus();
			set_focused_button(target_button);
			pressed_button = target_button;
			set_button_pseudo(pressed_button, "active", true);
		}
		else if (target_button.is_null()) {
			set_focused_button({});
		}
		if (selecting) {
			selection_anchor = selection_caret = label->layout->hit_test_text(point);
			if (target_button.is_null()) {
				label->grab_focus();
			}
			queue_redraw();
		}
		if (selecting || target_button.is_valid()) {
			accept_event();
		}
		return;
	}

	const bool clicked = !selection_dragged && !button->is_canceled() &&
		(point - visible_rect.position).distance_squared_to(pressed_position) <= DRAG_DEADZONE * DRAG_DEADZONE;
	const bool link_clicked = clicked && pressed_link != 0 && pressed_link == hit.id;
	const bool element_clicked = clicked && element.is_valid() && pressed_element == element;
	const bool button_clicked = !selection_dragged && !button->is_canceled() &&
		pressed_button == target_button && label->layout->is_enabled_button(target_button);
	if (selecting) {
		if (selection_dragged && !button->is_canceled()) {
			selection_caret = label->layout->hit_test_text(point.clamp(visible_rect.position, visible_rect.get_end()));
			queue_redraw();
		}
	}
	if (selecting || pressed_button.is_valid()) {
		accept_event();
	}
	const bool was_dragging = selection_dragged;
	cancel_press();
	if (was_dragging) {
		label->request_hover_check();
	}
	const ObjectID label_id(label->get_instance_id());
	if (link_clicked) {
		if (hit.uri.begins_with("#")) {
			label->scroll_to_anchor(docrender::decode_html_fragment(hit.uri.substr(1)));
		}
		label->emit_signal("link_pressed", hit.uri);
	}
	if (button_clicked) {
		if (auto* target = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(label_id));
			target && target->canvas) {
			target->canvas->activate_button(target_button);
		}
	}
	if (element_clicked && element->is_valid()) {
		if (auto* target = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(label_id))) {
			target->emit_signal("element_clicked", element);
		}
	}
}

void HTMLLabelCanvas::activate_button(const Ref<HTMLElement>& p_button) {
	if (label->is_visible_in_tree() && label->layout->is_enabled_button(p_button)) {
		label->emit_signal("button_pressed", p_button);
	}
}

void HTMLLabel::_input(const Ref<InputEvent>& p_event) {
	if (!canvas || has_focus() || canvas->tab_buttons.empty() || !is_visible_in_tree()) {
		return;
	}
	const Ref<InputEventKey> key = p_event;
	if (key.is_valid()) {
		if (const auto direction = tab_direction(*key.ptr())) {
			// Godot moves focus after _input, and FOCUS_ENTER has no input event.
			canvas->tab_entry_backward = direction;
			callable_mp(canvas, &HTMLLabelCanvas::clear_tab_entry).call_deferred();
		}
	}
}

void HTMLLabel::_gui_input(const Ref<InputEvent>& p_event) {
	const Ref<InputEventKey> key = p_event;
	if (canvas && key.is_valid() && !canvas->handle_button_key(*key.ptr())) {
		canvas->handle_selection_key(*key.ptr());
	}
}

void HTMLLabelCanvas::handle_selection_key(const InputEventKey& p_key) {
	if (!label->selection_enabled || !p_key.is_pressed()) {
		return;
	}
	if (p_key.is_action("ui_text_select_all", true)) {
		label->select_all();
		label->accept_event();
	}
	else if (p_key.is_action("ui_copy", true)) {
		const String text = label->get_selected_text();
		if (!text.is_empty()) {
			DisplayServer::get_singleton()->clipboard_set(text);
			label->accept_event();
		}
	}
}

void HTMLLabelCanvas::clear_tab_entry() {
	tab_entry_backward.reset();
}

void HTMLLabelCanvas::focus_tab_endpoint(bool p_backward) {
	if (label->has_focus() && focused_button.is_null() && !tab_buttons.empty()) {
		set_focused_button(p_backward ? tab_buttons.back() : tab_buttons.front(), true);
	}
}

bool HTMLLabelCanvas::handle_button_key(const InputEventKey& p_key) {
	if (const auto direction = tab_direction(p_key); direction && !tab_buttons.empty()) {
		const bool backwards = *direction;
		auto button = std::find(tab_buttons.begin(), tab_buttons.end(), focused_button);
		if (button != tab_buttons.end()) {
			if (backwards && button != tab_buttons.begin()) {
				--button;
			}
			else if (!backwards && std::next(button) != tab_buttons.end()) {
				++button;
			}
			else {
				const auto* next = backwards ? label->find_prev_valid_focus() : label->find_next_valid_focus();
				if (next && next != label) {
					return false;
				}
				button = backwards ? std::prev(tab_buttons.end()) : tab_buttons.begin();
			}
		}
		else {
			button = backwards ? std::prev(tab_buttons.end()) : tab_buttons.begin();
		}
		accept_event();
		set_focused_button(*button, true);
		return true;
	}

	const Key key = p_key.get_keycode() != KEY_NONE ? p_key.get_keycode() : p_key.get_physical_keycode();
	if (key != KEY_ENTER && key != KEY_KP_ENTER && key != KEY_SPACE) {
		return false;
	}
	if (!label->layout->is_enabled_button(focused_button)) {
		cancel_key_press();
		return false;
	}
	if (p_key.is_alt_pressed() || p_key.is_ctrl_pressed() || p_key.is_meta_pressed()) {
		cancel_key_press();
		return false;
	}
		label->accept_event();
		cancel_click();
	if (p_key.is_echo()) {
		return true;
	}
	if (p_key.is_pressed() && activation_key == KEY_NONE && pressed_button.is_null()) {
		activation_key = key;
		set_button_pseudo(focused_button, "active", true);
		if (key != KEY_SPACE) {
			const Ref<HTMLElement> target = focused_button;
			activate_button(target);
		}
	}
	else if (!p_key.is_pressed() && activation_key == key) {
		const Ref<HTMLElement> target = focused_button;
		cancel_key_press();
		if (key == KEY_SPACE && !p_key.is_canceled()) {
			activate_button(target);
		}
	}
	return true;
}

void HTMLLabelCanvas::set_focused_button(const Ref<HTMLElement>& p_button, bool p_keyboard) {
	if (focused_button != p_button) {
		cancel_key_press();
		set_button_pseudo(focused_button, "focus", false);
		set_button_pseudo(focused_button, "focus-visible", false);
		focused_button = p_button;
		set_button_pseudo(focused_button, "focus", true);
	}
	set_button_pseudo(focused_button, "focus-visible", p_keyboard);
	label->reveal_focused_button = focused_button.is_valid() && p_keyboard;
	if (label->reveal_focused_button) {
		label->queue_interaction();
	}
}

void HTMLLabelCanvas::set_button_pseudo(const Ref<HTMLElement>& p_button, const char* p_name, bool p_enabled) {
	if (label->layout->set_interaction_pseudo(p_button, p_name, p_enabled)) {
		label->queue_button_style_refresh();
	}
}

void HTMLLabelCanvas::update_focus_mode() {
	label->set_focus_mode(!tab_buttons.empty() ? FOCUS_ALL :
		(label->selection_enabled || has_enabled_buttons ? FOCUS_CLICK : FOCUS_NONE));
	label->set_process_input(!tab_buttons.empty());
}

void HTMLLabelCanvas::_notification(int p_what) {
	if (!label) {
		return;
	}
	if (p_what == NOTIFICATION_MOUSE_ENTER || p_what == NOTIFICATION_MOUSE_EXIT) {
		if (p_what == NOTIFICATION_MOUSE_EXIT) {
			cancel_click();
		}
		if (!label->updating_gui_hover) {
			label->request_hover_check();
		}
	}
	else if (p_what == NOTIFICATION_EXIT_TREE) {
		cancel_press();
		set_focused_button({});
		tab_buttons.clear();
		has_enabled_buttons = false;
		clear_tab_entry();
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
	cancel_click();
	cancel_key_press();
	selecting = false;
	selection_dragged = false;
	set_process_internal(false);
}

void HTMLLabelCanvas::cancel_click() {
	pressed_link = 0;
	pressed_element.unref();
	cancel_button_press();
}

void HTMLLabelCanvas::cancel_button_press() {
	if (pressed_button.is_valid()) {
		set_button_pseudo(pressed_button, "active", false);
		pressed_button.unref();
	}
}

void HTMLLabelCanvas::cancel_key_press() {
	if (activation_key != KEY_NONE) {
		activation_key = KEY_NONE;
		set_button_pseudo(focused_button, "active", false);
	}
}

void HTMLLabelCanvas::update_cursor(const std::optional<Vector2>& p_point) {
	if (!p_point) {
		set_default_cursor_shape(CURSOR_ARROW);
		return;
	}
	if (selection_dragged) {
		set_default_cursor_shape(CURSOR_IBEAM);
		return;
	}
	bool over_link = false;
	const auto css_cursor = get_css_cursor(label->layout->get_cursor_at(*p_point, over_link));
	set_default_cursor_shape(css_cursor.value_or(over_link ? CURSOR_POINTING_HAND :
		(label->selection_enabled ? CURSOR_IBEAM : CURSOR_ARROW)));
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
	ClassDB::bind_method(D_METHOD("set_css_diagnostics_enabled", "enabled"), &HTMLLabel::set_css_diagnostics_enabled);
	ClassDB::bind_method(D_METHOD("is_css_diagnostics_enabled"), &HTMLLabel::is_css_diagnostics_enabled);
	ClassDB::bind_method(D_METHOD("get_css_diagnostics"), &HTMLLabel::get_css_diagnostics);
	ClassDB::bind_method(D_METHOD("get_scroll_offset"), &HTMLLabel::get_scroll_offset);
	ClassDB::bind_method(D_METHOD("set_scroll_offset", "offset"), &HTMLLabel::set_scroll_offset);
	ClassDB::bind_method(D_METHOD("scroll_to_anchor", "id", "align"), &HTMLLabel::scroll_to_anchor, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("create_element", "tag_name"), &HTMLLabel::create_element);
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
	ADD_GROUP("Debug", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "css_diagnostics_enabled"), "set_css_diagnostics_enabled", "is_css_diagnostics_enabled");
	ADD_SIGNAL(MethodInfo("link_pressed", PropertyInfo(Variant::STRING, "uri")));
	ADD_SIGNAL(MethodInfo("element_clicked", PropertyInfo(Variant::OBJECT, "element", PROPERTY_HINT_NONE,
		String(), PROPERTY_USAGE_DEFAULT, HTMLElement::get_class_static())));
	ADD_SIGNAL(MethodInfo("button_pressed", PropertyInfo(Variant::OBJECT, "element", PROPERTY_HINT_NONE,
		String(), PROPERTY_USAGE_DEFAULT, HTMLElement::get_class_static())));
	ADD_SIGNAL(MethodInfo("layout_changed"));
}

void HTMLLabel::_notification(int p_what) {
	switch (p_what) {
	case NOTIFICATION_POST_ENTER_TREE:
		ensure_controls();
		queue_refresh();
		break;
	case NOTIFICATION_RESIZED:
		if (canvas) {
			canvas->cancel_click();
		}
		queue_refresh();
		break;
	case NOTIFICATION_THEME_CHANGED:
		queue_refresh();
		break;
	case NOTIFICATION_FOCUS_ENTER:
		if (canvas && canvas->tab_entry_backward) {
			canvas->focus_tab_endpoint(*canvas->tab_entry_backward);
			canvas->clear_tab_entry();
		}
		break;
	case NOTIFICATION_FOCUS_EXIT:
		if (canvas) {
			const bool was_dragging = canvas->selection_dragged;
			canvas->cancel_press();
			canvas->set_focused_button({});
			if (was_dragging) {
				request_hover_check();
			}
		}
		break;
	case NOTIFICATION_VISIBILITY_CHANGED:
		if (is_visible_in_tree()) {
			gui_hover_dirty = true;
			request_hover_check();
		}
		else if (canvas) {
			canvas->cancel_press();
			canvas->set_focused_button({});
			hover_passes_remaining = 0;
			if (layout->update_hover(std::nullopt)) {
				queue_refresh();
			}
			queue_interaction();
		}
		break;
	case NOTIFICATION_EXIT_TREE:
		layout->update_hover(std::nullopt);
		hover_passes_remaining = 0;
		hover_reflow = false;
		view_update_pending = false;
		scroll_sort_pending = false;
		layout_signal_pending = false;
		reveal_focused_button = false;
		break;
	case NOTIFICATION_WM_WINDOW_FOCUS_OUT:
		if (canvas) {
			canvas->cancel_press();
		}
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
		document->connect("changed", callable_mp(this, &HTMLLabel::document_changed).bind(document->get_instance_id()));
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

void HTMLLabel::set_css_diagnostics_enabled(bool p_enabled) {
	if (layout->is_css_diagnostics_enabled() == p_enabled) {
		return;
	}
	layout->set_css_diagnostics_enabled(p_enabled);
	if (p_enabled) {
		queue_refresh();
	}
	else if (canvas) {
		canvas->queue_redraw();
		canvas->refresh_diagnostic_tooltip(std::nullopt);
	}
}

bool HTMLLabel::is_css_diagnostics_enabled() const {
	return layout->is_css_diagnostics_enabled();
}

TypedArray<Dictionary> HTMLLabel::get_css_diagnostics() const {
	return layout->get_css_diagnostics();
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

Ref<HTMLElement> HTMLLabel::create_element(const String& p_tag_name) {
	parse_pending_source();
	return layout->create_element(p_tag_name);
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
		canvas->update_focus_mode();
		if (!p_enabled) {
			canvas->cancel_press();
			clear_selection();
		}
		request_hover_check();
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
	canvas->update_focus_mode();
	canvas->set_h_size_flags(SIZE_EXPAND_FILL);
	canvas->set_v_size_flags(SIZE_EXPAND_FILL);
	scroll_container->add_child(canvas, false, INTERNAL_MODE_BACK);
}

void HTMLLabel::queue_refresh() {
	hover_reflow = false;
	hover_passes_remaining = HOVER_PASSES;
	if (canvas && is_inside_tree() && !refresh_queued) {
		refresh_queued = true;
		callable_mp(this, &HTMLLabel::refresh).call_deferred();
	}
}

void HTMLLabel::queue_button_style_refresh() {
	button_styles_dirty = true;
	queue_refresh();
}

void HTMLLabel::source_changed() {
	source_dirty = true;
	layout->clear();
	pending_scroll_offset = Vector2i();
	if (canvas) {
		canvas->refresh_diagnostic_tooltip(std::nullopt);
		canvas->cancel_press();
		canvas->set_focused_button({});
		canvas->tab_buttons.clear();
		canvas->has_enabled_buttons = false;
		canvas->update_focus_mode();
		clear_selection();
	}
	queue_refresh();
}

void HTMLLabel::document_changed(uint64_t p_document_id) {
	if (document.is_valid() && document->get_instance_id() == p_document_id) {
		source_changed();
	}
}

void HTMLLabel::parse_pending_source() {
	if (!source_dirty) {
		return;
	}
	source_dirty = false;
	layout_updated = true;
	if (document.is_valid()) {
		document->parse_into(*layout.ptr(), BUTTON_STYLES);
		parsed = layout->get_render_error().is_empty();
	}
	else {
		parsed = layout->parse_source(text, base_dir, stylesheet, Dictionary(), false, BUTTON_STYLES) == OK;
	}
	layout->set_edit_callback(callable_mp(this, &HTMLLabel::on_layout_edited));
}

void HTMLLabel::on_layout_edited() {
	layout_updated = true;
	if (canvas) {
		canvas->cancel_press();
		clear_selection();
	}
	queue_refresh();
}

void HTMLLabel::refresh() {
	refresh_queued = false;
	if (!is_inside_tree()) {
		return;
	}
	if (!hover_reflow && !button_styles_dirty) {
		canvas->cancel_click();
	}
	if (!pending_scroll_offset) {
		pending_scroll_offset = get_scroll_offset();
	}

	parse_pending_source();
	const bool button_style_changed = std::exchange(button_styles_dirty, false);
	if (button_style_changed && parsed) {
		layout->mark_styles_pending();
	}
	const bool layout_changed = layout_updated || hover_reflow || button_style_changed;
	layout_updated = false;
	if (!parsed) {
		update_view({}, {}, false, false, layout_changed, layout->get_render_error());
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
			update_view({}, {}, false, false, layout_changed);
			return;
		}
		if (layout->layout(viewport) != OK) {
			update_view({}, {}, false, false, layout_changed, layout->get_render_error());
			return;
		}
		const Vector2i size = layout->get_content_size();
		const bool next_horizontal = horizontal || size.x > viewport.x;
		const bool next_vertical = vertical || size.y > viewport.y;
		if (next_horizontal == horizontal && next_vertical == vertical) {
			update_view(viewport, size, horizontal, vertical, layout_changed);
			return;
		}
		horizontal = next_horizontal;
		vertical = next_vertical;
	}
}

void HTMLLabel::update_view(const Vector2i& p_viewport, const Vector2i& p_content,
	bool p_scroll_horizontal, bool p_scroll_vertical, bool p_layout_changed, const String& p_error) {
	view_update_pending = true;
	if (p_viewport == Vector2i()) {
		layout->update_hover(std::nullopt);
		hover_passes_remaining = 0;
		canvas->tab_buttons.clear();
		canvas->has_enabled_buttons = false;
		canvas->cancel_press();
		canvas->set_focused_button({});
	}
	else {
		canvas->tab_buttons = layout->get_tab_buttons(canvas->has_enabled_buttons);
		if (canvas->focused_button.is_valid() && !layout->is_enabled_button(canvas->focused_button)) {
			canvas->set_focused_button({});
		}
		if (canvas->pressed_button.is_valid() && !layout->is_enabled_button(canvas->pressed_button)) {
			canvas->cancel_button_press();
		}
	}
	canvas->update_focus_mode();
	layout_signal_pending |= p_layout_changed || viewport_size != p_viewport ||
		content_size != p_content || render_error != p_error;
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
	// Scrollbar values may change before the container moves the content canvas.
	return Rect2(Vector2(sorted_scroll_offset), Vector2(viewport_size));
}

void HTMLLabel::apply_scroll_offset(const Vector2i& p_offset) {
	const Vector2i max_offset = (content_size - viewport_size).max(Vector2i());
	const Vector2i offset = p_offset.clamp(Vector2i(), max_offset);
	scroll_container->set_h_scroll(offset.x);
	scroll_container->set_v_scroll(offset.y);
}

void HTMLLabel::reveal_button(const Rect2& p_target) {
	const Vector2i offset = get_scroll_offset();
	const Vector2 start = p_target.position;
	const Vector2 end = p_target.get_end();
	const auto reveal = [](int p_offset, int p_size, double p_start, double p_end) {
		if (p_start < p_offset || p_end - p_start >= p_size) {
			return std::floor(p_start);
		}
		return std::max<double>(p_offset, std::ceil(p_end - p_size));
	};
	const Vector2i max_offset = (content_size - viewport_size).max(Vector2i());
	const Vector2i next(
		static_cast<int>(std::clamp(reveal(offset.x, viewport_size.x, start.x, end.x), 0.0, double(max_offset.x))),
		static_cast<int>(std::clamp(reveal(offset.y, viewport_size.y, start.y, end.y), 0.0, double(max_offset.y))));
	apply_scroll_offset(next);
}

void HTMLLabel::apply_pending_scroll() {
	if (!is_inside_tree() || refresh_queued) {
		return;
	}
	if (pending_scroll_offset) {
		const Vector2i offset = *pending_scroll_offset;
		pending_scroll_offset.reset();
		apply_scroll_offset(offset);
	}
	view_update_pending = false;
	queue_interaction();
}

void HTMLLabel::on_scroll_changed(double) {
	canvas->cancel_click();
	canvas->queue_redraw();
	scroll_sort_pending = true;
	if (!hover_reflow) {
		hover_passes_remaining = HOVER_PASSES;
	}
}

void HTMLLabel::on_content_sorted() {
	sorted_scroll_offset = get_scroll_offset();
	scroll_sort_pending = false;
	gui_hover_dirty = true;
	canvas->queue_redraw();
	queue_interaction();
}

void HTMLLabel::request_hover_check() {
	hover_passes_remaining = HOVER_PASSES;
	queue_interaction();
}

void HTMLLabel::queue_interaction() {
	if (canvas && is_inside_tree() && !interaction_queued) {
		interaction_queued = true;
		callable_mp(this, &HTMLLabel::update_interaction).call_deferred();
	}
}

void HTMLLabel::update_interaction() {
	interaction_queued = false;
	if (!is_inside_tree() || refresh_queued || view_update_pending || scroll_sort_pending) {
		return;
	}

	if (std::exchange(reveal_focused_button, false)) {
		if (const auto bounds = layout->get_element_scroll_bounds(canvas->focused_button)) {
			reveal_button(*bounds);
			if (scroll_sort_pending) {
				return;
			}
		}
	}

	std::optional<Vector2> point;
	if (is_visible_in_tree() && viewport_size != Vector2i()) {
		Viewport* viewport = get_viewport();
		if (gui_hover_dirty) {
			gui_hover_dirty = false;
			// Refresh Godot's hovered Control after moving the canvas or its scrollbars.
			updating_gui_hover = true;
			viewport->update_mouse_cursor_state();
			updating_gui_hover = false;
			if (!is_inside_tree() || refresh_queued || scroll_sort_pending) {
				return;
			}
		}
		const Vector2 mouse = canvas->get_local_mouse_position();
		if (viewport->gui_get_hovered_control() == canvas && get_visible_rect().has_point(mouse)) {
			point = mouse;
		}
	}

	if (!canvas->selection_dragged && hover_passes_remaining > 0 && layout->update_hover(point)) {
		// A hover rule can move or hide its own target. Allow one correction after sorting.
		--hover_passes_remaining;
		hover_reflow = true;
		refresh();
		return;
	}

	hover_passes_remaining = 0;
	hover_reflow = false;
	canvas->update_cursor(point);
	canvas->refresh_diagnostic_tooltip(point);
	if (layout_signal_pending && !refresh_queued) {
		layout_signal_pending = false;
		emit_signal("layout_changed");
	}
}
