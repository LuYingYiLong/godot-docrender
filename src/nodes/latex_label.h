#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label_settings.hpp>
#include <godot_cpp/classes/style_box.hpp>

#include "core/tex_render.h"

class LatexLabel : public godot::Control {
	GDCLASS(LatexLabel, godot::Control)

public:
	LatexLabel();

	void set_text(const godot::String& p_text);
	godot::String get_text() const;
	void set_label_settings(const godot::Ref<godot::LabelSettings>& p_settings);
	godot::Ref<godot::LabelSettings> get_label_settings() const;
	void set_horizontal_alignment(godot::HorizontalAlignment p_alignment);
	godot::HorizontalAlignment get_horizontal_alignment() const;
	void set_vertical_alignment(godot::VerticalAlignment p_alignment);
	godot::VerticalAlignment get_vertical_alignment() const;
	void set_clip_text(bool p_clip);
	bool is_clipping_text() const;
	void set_display_mode(bool p_enabled);
	bool is_display_mode() const;
	godot::String get_render_error() const;
	godot::Vector2 _get_minimum_size() const override;
	godot::PackedStringArray _get_configuration_warnings() const override;

protected:
	static void _bind_methods();
	void _notification(int p_what);

private:
	godot::String text;
	godot::Ref<godot::LabelSettings> label_settings;
	godot::Ref<godot::StyleBox> normal_style;
	godot::Ref<godot::StyleBox> focus_style;
	godot::HorizontalAlignment horizontal_alignment = godot::HORIZONTAL_ALIGNMENT_LEFT;
	godot::VerticalAlignment vertical_alignment = godot::VERTICAL_ALIGNMENT_TOP;
	bool clip_text = false;
	bool display_mode = true;
	godot::Ref<TeXRender> render;

	void rebuild();
	void update_styles();
	void draw_formula();
};
