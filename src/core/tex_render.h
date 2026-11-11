#pragma once

#include <memory>

#include <godot_cpp/classes/ref_counted.hpp>

namespace godot {
	class CanvasItem;
}

namespace tex {
	class TeXRender;
}

class TeXRender : public godot::RefCounted {
	GDCLASS(TeXRender, godot::RefCounted)

public:
	enum Style {
		STYLE_DISPLAY,
		STYLE_DISPLAY_CRAMPED,
		STYLE_TEXT,
		STYLE_TEXT_CRAMPED,
		STYLE_SCRIPT,
		STYLE_SCRIPT_CRAMPED,
		STYLE_SCRIPT_SCRIPT,
		STYLE_SCRIPT_SCRIPT_CRAMPED,
	};

	TeXRender();
	~TeXRender() override;

	godot::Error parse(const godot::String& p_text, float p_text_size = 16.0f,
		Style p_style = STYLE_DISPLAY, const godot::Color& p_foreground = godot::Color(1, 1, 1));
	void clear();
	bool is_valid() const;
	godot::String get_render_error() const;

	float get_text_size() const;
	void set_text_size(float p_text_size);
	godot::Color get_foreground() const;
	void set_foreground(const godot::Color& p_color);

	godot::Vector2i get_size() const;
	int get_width() const;
	int get_height() const;
	int get_depth() const;
	float get_baseline() const;
	float get_baseline_offset() const;
	godot::Vector4i get_insets() const;
	void set_insets(const godot::Vector4i& p_insets, bool p_true_values = false);
	void set_width(int p_width, godot::HorizontalAlignment p_alignment = godot::HORIZONTAL_ALIGNMENT_LEFT);
	void set_height(int p_height, godot::VerticalAlignment p_alignment = godot::VERTICAL_ALIGNMENT_TOP);

	void draw(godot::CanvasItem* p_canvas, const godot::Vector2& p_position = godot::Vector2()) const;

	static void shutdown_renderer();

protected:
	static void _bind_methods();

private:
	std::unique_ptr<tex::TeXRender> render;
	godot::String render_error;
	godot::Color foreground = godot::Color(1, 1, 1);
	godot::Vector2i minimum_size;
	godot::HorizontalAlignment horizontal_alignment = godot::HORIZONTAL_ALIGNMENT_LEFT;
	godot::VerticalAlignment vertical_alignment = godot::VERTICAL_ALIGNMENT_TOP;

	godot::Vector2 get_layout_offset() const;
};

VARIANT_ENUM_CAST(TeXRender::Style);
