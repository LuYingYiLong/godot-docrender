#pragma once

#include <godot_cpp/classes/control.hpp>

#include "tex_render.h"

class LatexLabel : public godot::Control {
	GDCLASS(LatexLabel, godot::Control)

public:
	void set_text(const godot::String& p_text);
	godot::String get_text() const;
	void set_display_mode(bool p_enabled);
	bool is_display_mode() const;
	godot::String get_render_error() const;
	godot::Vector2 _get_minimum_size() const override;

protected:
	static void _bind_methods();
	void _notification(int p_what);

private:
	godot::String text;
	bool display_mode = true;
	godot::Ref<TeXRender> render;

	void rebuild();
};
