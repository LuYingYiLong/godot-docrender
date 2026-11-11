#include "nodes/latex_label.h"

#include <godot_cpp/classes/rendering_server.hpp>

using namespace godot;

LatexLabel::LatexLabel() {
	set_mouse_filter(MOUSE_FILTER_IGNORE);
	set_v_size_flags(SIZE_SHRINK_CENTER);
}

void LatexLabel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_text", "text"), &LatexLabel::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &LatexLabel::get_text);
	ClassDB::bind_method(D_METHOD("set_label_settings", "settings"), &LatexLabel::set_label_settings);
	ClassDB::bind_method(D_METHOD("get_label_settings"), &LatexLabel::get_label_settings);
	ClassDB::bind_method(D_METHOD("set_horizontal_alignment", "alignment"), &LatexLabel::set_horizontal_alignment);
	ClassDB::bind_method(D_METHOD("get_horizontal_alignment"), &LatexLabel::get_horizontal_alignment);
	ClassDB::bind_method(D_METHOD("set_vertical_alignment", "alignment"), &LatexLabel::set_vertical_alignment);
	ClassDB::bind_method(D_METHOD("get_vertical_alignment"), &LatexLabel::get_vertical_alignment);
	ClassDB::bind_method(D_METHOD("set_clip_text", "enable"), &LatexLabel::set_clip_text);
	ClassDB::bind_method(D_METHOD("is_clipping_text"), &LatexLabel::is_clipping_text);
	ClassDB::bind_method(D_METHOD("set_display_mode", "enabled"), &LatexLabel::set_display_mode);
	ClassDB::bind_method(D_METHOD("is_display_mode"), &LatexLabel::is_display_mode);
	ClassDB::bind_method(D_METHOD("get_render_error"), &LatexLabel::get_render_error);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "label_settings", PROPERTY_HINT_RESOURCE_TYPE, "LabelSettings"), "set_label_settings", "get_label_settings");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "horizontal_alignment", PROPERTY_HINT_ENUM, "Left,Center,Right,Fill"), "set_horizontal_alignment", "get_horizontal_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vertical_alignment", PROPERTY_HINT_ENUM, "Top,Center,Bottom,Fill"), "set_vertical_alignment", "get_vertical_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "clip_text"), "set_clip_text", "is_clipping_text");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "display_mode"), "set_display_mode", "is_display_mode");
}

void LatexLabel::_notification(int p_what) {
	switch (p_what) {
	case NOTIFICATION_ENTER_TREE:
	case NOTIFICATION_THEME_CHANGED:
		update_styles();
		rebuild();
		break;
	case NOTIFICATION_TRANSLATION_CHANGED:
		if (is_inside_tree()) {
			rebuild();
		}
		break;
	case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
	case NOTIFICATION_RESIZED:
		queue_redraw();
		break;
	case NOTIFICATION_DRAW:
		draw_formula();
		break;
	}
}

void LatexLabel::set_text(const String& p_text) {
	if (text == p_text) {
		return;
	}
	text = p_text;
	rebuild();
}

String LatexLabel::get_text() const {
	return text;
}

void LatexLabel::set_label_settings(const Ref<LabelSettings>& p_settings) {
	if (label_settings == p_settings) {
		return;
	}
	const Callable changed = callable_mp(this, &LatexLabel::rebuild);
	if (label_settings.is_valid()) {
		label_settings->disconnect("changed", changed);
	}
	label_settings = p_settings;
	if (label_settings.is_valid()) {
		label_settings->connect("changed", changed);
	}
	rebuild();
}

Ref<LabelSettings> LatexLabel::get_label_settings() const {
	return label_settings;
}

void LatexLabel::set_horizontal_alignment(HorizontalAlignment p_alignment) {
	ERR_FAIL_INDEX(static_cast<int>(p_alignment), 4);
	if (horizontal_alignment == p_alignment) {
		return;
	}
	horizontal_alignment = p_alignment;
	queue_redraw();
}

HorizontalAlignment LatexLabel::get_horizontal_alignment() const {
	return horizontal_alignment;
}

void LatexLabel::set_vertical_alignment(VerticalAlignment p_alignment) {
	ERR_FAIL_INDEX(static_cast<int>(p_alignment), 4);
	if (vertical_alignment == p_alignment) {
		return;
	}
	vertical_alignment = p_alignment;
	queue_redraw();
}

VerticalAlignment LatexLabel::get_vertical_alignment() const {
	return vertical_alignment;
}

void LatexLabel::set_clip_text(bool p_clip) {
	if (clip_text == p_clip) {
		return;
	}
	clip_text = p_clip;
	update_minimum_size();
	queue_redraw();
}

bool LatexLabel::is_clipping_text() const {
	return clip_text;
}

void LatexLabel::set_display_mode(bool p_enabled) {
	if (display_mode == p_enabled) {
		return;
	}
	display_mode = p_enabled;
	rebuild();
}

bool LatexLabel::is_display_mode() const {
	return display_mode;
}

String LatexLabel::get_render_error() const {
	return render.is_valid() ? render->get_render_error() : String();
}

Vector2 LatexLabel::_get_minimum_size() const {
	Vector2 size = render.is_valid() ? Vector2(render->get_size()) : Vector2();
	if (clip_text) {
		size.x = 1.0f;
	}
	return normal_style.is_valid() ? size + normal_style->get_minimum_size() : size;
}

PackedStringArray LatexLabel::_get_configuration_warnings() const {
	PackedStringArray warnings;
	const String error = get_render_error();
	if (!error.is_empty()) {
		warnings.push_back(error);
	}
	return warnings;
}

void LatexLabel::update_styles() {
	normal_style = has_theme_stylebox("normal") ? get_theme_stylebox("normal") : get_theme_stylebox("normal", "Label");
	focus_style = has_theme_stylebox("focus") ? get_theme_stylebox("focus") : get_theme_stylebox("focus", "Label");
}

void LatexLabel::draw_formula() {
	const RID canvas = get_canvas_item();
	RenderingServer::get_singleton()->canvas_item_set_clip(canvas, clip_text || is_clipping_contents());
	const Vector2 size = get_size();
	const Ref<StyleBox>& background = has_focus() && focus_style.is_valid() ? focus_style : normal_style;
	if (background.is_valid()) {
		background->draw(canvas, Rect2(Vector2(), size));
	}
	if (render.is_null() || !render->is_valid() || render->get_foreground().a <= 0.0f) {
		return;
	}

	const Vector2 formula_size = Vector2(render->get_size());
	const Vector2 leading = normal_style.is_valid() ? normal_style->get_offset() : Vector2();
	const Vector2 margins = normal_style.is_valid() ? normal_style->get_minimum_size() : Vector2();
	const Vector2 remaining = size - margins - formula_size;
	Vector2 position = leading;

	if (horizontal_alignment == HORIZONTAL_ALIGNMENT_CENTER) {
		position.x = (size.x - formula_size.x) * 0.5f;
	}
	else if ((horizontal_alignment == HORIZONTAL_ALIGNMENT_RIGHT && !is_layout_rtl()) ||
		(horizontal_alignment == HORIZONTAL_ALIGNMENT_LEFT && is_layout_rtl())) {
		position.x += remaining.x;
	}

	if (vertical_alignment == VERTICAL_ALIGNMENT_CENTER) {
		position.y += remaining.y * 0.5f;
	}
	else if (vertical_alignment == VERTICAL_ALIGNMENT_BOTTOM) {
		position.y += remaining.y;
	}
	render->draw(this, position);
}

void LatexLabel::rebuild() {
	const String formula = atr(text);
	if (is_inside_tree() && !formula.strip_edges().is_empty()) {
		const int font_size = label_settings.is_valid() ? label_settings->get_font_size() :
			(has_theme_font_size("font_size") ? get_theme_font_size("font_size") : get_theme_font_size("font_size", "Label"));
		const Color color = label_settings.is_valid() ? label_settings->get_font_color() :
			(has_theme_color("font_color") ? get_theme_color("font_color") : get_theme_color("font_color", "Label"));
		if (render.is_null()) {
			render.instantiate();
		}
		render->parse(formula, font_size, display_mode ? TeXRender::STYLE_DISPLAY : TeXRender::STYLE_TEXT, color);
	}
	else {
		render.unref();
	}
	update_minimum_size();
	update_configuration_warnings();
	queue_redraw();
}
