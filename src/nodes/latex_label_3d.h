#pragma once

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

#include "formats/latex/tex_render.h"

namespace godot {
	class Node2D;
	class SubViewport;
}

class LatexLabel3D : public godot::MeshInstance3D {
	GDCLASS(LatexLabel3D, godot::MeshInstance3D)

public:
	enum DrawFlags {
		FLAG_SHADED,
		FLAG_DOUBLE_SIDED,
		FLAG_DISABLE_DEPTH_TEST,
		FLAG_FIXED_SIZE,
		FLAG_MAX,
	};

	enum AlphaCutMode {
		ALPHA_CUT_DISABLED,
		ALPHA_CUT_DISCARD,
		ALPHA_CUT_OPAQUE_PREPASS,
		ALPHA_CUT_HASH,
	};

	LatexLabel3D();

	void set_text(const godot::String& p_text);
	godot::String get_text() const;
	void set_display_mode(bool p_enabled);
	bool is_display_mode() const;
	void set_font_size(int p_size);
	int get_font_size() const;
	void set_modulate(const godot::Color& p_color);
	godot::Color get_modulate() const;
	void set_horizontal_alignment(godot::HorizontalAlignment p_alignment);
	godot::HorizontalAlignment get_horizontal_alignment() const;
	void set_vertical_alignment(godot::VerticalAlignment p_alignment);
	godot::VerticalAlignment get_vertical_alignment() const;
	void set_pixel_size(float p_size);
	float get_pixel_size() const;
	void set_offset(const godot::Vector2& p_offset);
	godot::Vector2 get_offset() const;

	void set_billboard_mode(godot::BaseMaterial3D::BillboardMode p_mode);
	godot::BaseMaterial3D::BillboardMode get_billboard_mode() const;
	void set_draw_flag(DrawFlags p_flag, bool p_enabled);
	bool get_draw_flag(DrawFlags p_flag) const;
	void set_alpha_cut_mode(AlphaCutMode p_mode);
	AlphaCutMode get_alpha_cut_mode() const;
	void set_alpha_scissor_threshold(float p_threshold);
	float get_alpha_scissor_threshold() const;
	void set_alpha_hash_scale(float p_scale);
	float get_alpha_hash_scale() const;
	void set_render_scale(float p_scale);
	float get_render_scale() const;
	void set_texture_filter(godot::BaseMaterial3D::TextureFilter p_filter);
	godot::BaseMaterial3D::TextureFilter get_texture_filter() const;
	void set_render_priority(int p_priority);
	int get_render_priority() const;

	godot::String get_render_error() const;
	godot::PackedStringArray _get_configuration_warnings() const override;

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _validate_property(godot::PropertyInfo& p_property) const;

private:
	godot::String text;
	bool display_mode = true;
	int font_size = 32;
	godot::HorizontalAlignment horizontal_alignment = godot::HORIZONTAL_ALIGNMENT_CENTER;
	godot::VerticalAlignment vertical_alignment = godot::VERTICAL_ALIGNMENT_CENTER;
	float pixel_size = 0.005f;
	float render_scale = 1.0f;
	godot::Vector2 offset;

	godot::Ref<TeXRender> render;
	godot::Ref<godot::QuadMesh> quad;
	godot::Ref<godot::StandardMaterial3D> material;
	godot::SubViewport* viewport = nullptr;
	godot::Node2D* canvas = nullptr;
	bool rebuild_queued = false;

	void queue_rebuild();
	void rebuild();
	void ensure_viewport();
	void update_texture();
	void update_quad();
	void draw_formula();
};

VARIANT_ENUM_CAST(LatexLabel3D::DrawFlags);
VARIANT_ENUM_CAST(LatexLabel3D::AlphaCutMode);
