#include "mermaid_label.h"

using namespace godot;

MermaidLabel::MermaidLabel() {
	set_mouse_filter(MOUSE_FILTER_IGNORE);
}

void MermaidLabel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_text", "text"), &MermaidLabel::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &MermaidLabel::get_text);
	ClassDB::bind_method(D_METHOD("set_theme_mode", "mode"), &MermaidLabel::set_theme_mode);
	ClassDB::bind_method(D_METHOD("get_theme_mode"), &MermaidLabel::get_theme_mode);
	ClassDB::bind_method(D_METHOD("get_texture"), &MermaidLabel::get_texture);
	ClassDB::bind_method(D_METHOD("get_render_error"), &MermaidLabel::get_render_error);

	BIND_ENUM_CONSTANT(THEME_AUTO);
	BIND_ENUM_CONSTANT(THEME_LIGHT);
	BIND_ENUM_CONSTANT(THEME_DARK);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "theme_mode", PROPERTY_HINT_ENUM, "Auto,Light,Dark"), "set_theme_mode", "get_theme_mode");
	ADD_SIGNAL(MethodInfo("render_finished"));
}

void MermaidLabel::_notification(int p_what) {
	switch (p_what) {
	case NOTIFICATION_ENTER_TREE:
	case NOTIFICATION_THEME_CHANGED:
		refresh_render();
		break;
	case NOTIFICATION_EXIT_TREE:
		set_process_internal(false);
		break;
	case NOTIFICATION_INTERNAL_PROCESS:
		if (render_cache.poll()) {
			refresh_render();
		}
		break;
	case NOTIFICATION_RESIZED:
		queue_redraw();
		break;
	case NOTIFICATION_DRAW:
		if (texture.is_valid()) {
			const Vector2 size = _get_minimum_size();
			draw_texture_rect(texture, Rect2((get_size() - size) * 0.5f, size), false);
		}
		break;
	}
}

void MermaidLabel::set_text(const String& p_text) {
	if (text != p_text) {
		text = p_text;
		refresh_render();
	}
}

String MermaidLabel::get_text() const {
	return text;
}

void MermaidLabel::set_theme_mode(ThemeMode p_mode) {
	ERR_FAIL_INDEX(p_mode, THEME_DARK + 1);
	if (theme_mode != p_mode) {
		theme_mode = p_mode;
		refresh_render();
	}
}

MermaidLabel::ThemeMode MermaidLabel::get_theme_mode() const {
	return theme_mode;
}

Ref<Texture2D> MermaidLabel::get_texture() const {
	return texture;
}

String MermaidLabel::get_render_error() const {
	return render_error;
}

Vector2 MermaidLabel::_get_minimum_size() const {
	return texture.is_valid() ? texture->get_size() / MermaidRenderCache::RASTER_SCALE : Vector2();
}

PackedStringArray MermaidLabel::_get_configuration_warnings() const {
	PackedStringArray warnings;
	if (!render_error.is_empty()) {
		warnings.append(render_error);
	}
	return warnings;
}

bool MermaidLabel::uses_light_theme() const {
	if (theme_mode != THEME_AUTO) {
		return theme_mode == THEME_LIGHT;
	}
	const Color font_color = has_theme_color("font_color") ? get_theme_color("font_color") : get_theme_color("font_color", "Label");
	return font_color.get_luminance() < 0.5f;
}

void MermaidLabel::refresh_render() {
	Ref<Texture2D> next_texture;
	String next_error;
	bool completed = false;
	if (is_inside_tree()) {
		HashSet<String> sources;
		if (!text.strip_edges().is_empty()) {
			if (MermaidRender::is_available()) {
				sources.insert(text);
			}
			else {
				next_error = "This build does not include the Merman rendering backend.";
			}
		}
		const auto theme_color = [this](const StringName& p_name) -> std::optional<Color> {
			return has_theme_color(p_name) ? std::optional<Color>(get_theme_color(p_name)) : std::nullopt;
			};
		MermaidRenderCache::Palette palette;
		palette.light = uses_light_theme();
		palette.node_background = theme_color("node_background_color");
		palette.node_border = theme_color("node_border_color");
		palette.node_text = theme_color("node_text_color");
		palette.line = theme_color("line_color");
		render_cache.update(sources, palette);
		if (const MermaidRenderCache::Result* result = render_cache.get_result(text)) {
			next_texture = result->texture;
			next_error = result->error;
			completed = true;
		}
		const bool pending = render_cache.has_pending();
		set_process_internal(pending);
		if (!completed && pending && texture.is_valid() && rendered_source == text) {
			return;
		}
	}

	if (texture == next_texture && render_error == next_error) {
		return;
	}
	texture = next_texture;
	rendered_source = texture.is_valid() ? text : String();
	render_error = next_error;
	update_minimum_size();
	update_configuration_warnings();
	queue_redraw();
	if (completed) {
		emit_signal("render_finished");
	}
}
