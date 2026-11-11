#pragma once

#include "core/mermaid_render.h"

#include <godot_cpp/classes/control.hpp>

class MermaidLabel : public godot::Control {
	GDCLASS(MermaidLabel, godot::Control)

public:
	enum ThemeMode {
		THEME_AUTO,
		THEME_LIGHT,
		THEME_DARK,
	};

	MermaidLabel();

	void set_text(const godot::String& p_text);
	godot::String get_text() const;
	void set_theme_mode(ThemeMode p_mode);
	ThemeMode get_theme_mode() const;
	godot::Ref<godot::Texture2D> get_texture() const;
	godot::String get_render_error() const;
	godot::Vector2 _get_minimum_size() const override;
	godot::PackedStringArray _get_configuration_warnings() const override;

protected:
	static void _bind_methods();
	void _notification(int p_what);

private:
	godot::String text;
	ThemeMode theme_mode = THEME_AUTO;
	MermaidRenderCache render_cache;
	godot::Ref<godot::Texture2D> texture;
	godot::String rendered_source;
	godot::String render_error;

	bool uses_light_theme() const;
	void refresh_render();
};

VARIANT_ENUM_CAST(MermaidLabel::ThemeMode);
