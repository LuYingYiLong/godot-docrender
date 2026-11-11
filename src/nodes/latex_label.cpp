#include "nodes/latex_label.h"

using namespace godot;

void LatexLabel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_text", "text"), &LatexLabel::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &LatexLabel::get_text);
	ClassDB::bind_method(D_METHOD("set_display_mode", "enabled"), &LatexLabel::set_display_mode);
	ClassDB::bind_method(D_METHOD("is_display_mode"), &LatexLabel::is_display_mode);
	ClassDB::bind_method(D_METHOD("get_render_error"), &LatexLabel::get_render_error);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "display_mode"), "set_display_mode", "is_display_mode");
}

void LatexLabel::_notification(int p_what) {
	switch (p_what) {
	case NOTIFICATION_ENTER_TREE:
	case NOTIFICATION_THEME_CHANGED:
		if (is_inside_tree()) {
			rebuild();
		}
		break;
	case NOTIFICATION_DRAW:
		if (render.is_valid()) {
			render->draw(this);
		}
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
	return render.is_valid() ? Vector2(render->get_size()) : Vector2();
}

void LatexLabel::rebuild() {
	if (is_inside_tree() && !text.strip_edges().is_empty()) {
		const int font_size = has_theme_font_size("font_size") ?
			get_theme_font_size("font_size") : get_theme_font_size("font_size", "Label");
		const Color color = has_theme_color("font_color") ?
			get_theme_color("font_color") : get_theme_color("font_color", "Label");
		if (render.is_null()) {
			render.instantiate();
		}
		render->parse(text, font_size, display_mode ? TeXRender::STYLE_DISPLAY : TeXRender::STYLE_TEXT, color);
	}
	else {
		render.unref();
	}
	update_minimum_size();
	queue_redraw();
}
