#include "nodes/latex_label_3d.h"

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

namespace {
	constexpr int texture_padding = 2;
	constexpr float max_texture_extent = 4096.0f;
	constexpr BaseMaterial3D::Transparency alpha_cut_modes[] = {
		BaseMaterial3D::TRANSPARENCY_ALPHA,
		BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR,
		BaseMaterial3D::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS,
		BaseMaterial3D::TRANSPARENCY_ALPHA_HASH,
	};
}

LatexLabel3D::LatexLabel3D() {
	material.instantiate();
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	material->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_LINEAR);
	material->set_flag(BaseMaterial3D::FLAG_BILLBOARD_KEEP_SCALE, true);
	quad.instantiate();
	quad->set_material(material);
	set_cast_shadows_setting(SHADOW_CASTING_SETTING_OFF);
	set_gi_mode(GI_MODE_DISABLED);
}

void LatexLabel3D::_bind_methods() {
	BIND_ENUM_CONSTANT(FLAG_SHADED);
	BIND_ENUM_CONSTANT(FLAG_DOUBLE_SIDED);
	BIND_ENUM_CONSTANT(FLAG_DISABLE_DEPTH_TEST);
	BIND_ENUM_CONSTANT(FLAG_FIXED_SIZE);
	BIND_ENUM_CONSTANT(FLAG_MAX);
	BIND_ENUM_CONSTANT(ALPHA_CUT_DISABLED);
	BIND_ENUM_CONSTANT(ALPHA_CUT_DISCARD);
	BIND_ENUM_CONSTANT(ALPHA_CUT_OPAQUE_PREPASS);
	BIND_ENUM_CONSTANT(ALPHA_CUT_HASH);

	ClassDB::bind_method(D_METHOD("set_text", "text"), &LatexLabel3D::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &LatexLabel3D::get_text);
	ClassDB::bind_method(D_METHOD("set_display_mode", "enabled"), &LatexLabel3D::set_display_mode);
	ClassDB::bind_method(D_METHOD("is_display_mode"), &LatexLabel3D::is_display_mode);
	ClassDB::bind_method(D_METHOD("set_font_size", "size"), &LatexLabel3D::set_font_size);
	ClassDB::bind_method(D_METHOD("get_font_size"), &LatexLabel3D::get_font_size);
	ClassDB::bind_method(D_METHOD("set_modulate", "color"), &LatexLabel3D::set_modulate);
	ClassDB::bind_method(D_METHOD("get_modulate"), &LatexLabel3D::get_modulate);
	ClassDB::bind_method(D_METHOD("set_horizontal_alignment", "alignment"), &LatexLabel3D::set_horizontal_alignment);
	ClassDB::bind_method(D_METHOD("get_horizontal_alignment"), &LatexLabel3D::get_horizontal_alignment);
	ClassDB::bind_method(D_METHOD("set_vertical_alignment", "alignment"), &LatexLabel3D::set_vertical_alignment);
	ClassDB::bind_method(D_METHOD("get_vertical_alignment"), &LatexLabel3D::get_vertical_alignment);
	ClassDB::bind_method(D_METHOD("set_pixel_size", "pixel_size"), &LatexLabel3D::set_pixel_size);
	ClassDB::bind_method(D_METHOD("get_pixel_size"), &LatexLabel3D::get_pixel_size);
	ClassDB::bind_method(D_METHOD("set_offset", "offset"), &LatexLabel3D::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &LatexLabel3D::get_offset);
	ClassDB::bind_method(D_METHOD("set_billboard_mode", "mode"), &LatexLabel3D::set_billboard_mode);
	ClassDB::bind_method(D_METHOD("get_billboard_mode"), &LatexLabel3D::get_billboard_mode);
	ClassDB::bind_method(D_METHOD("set_draw_flag", "flag", "enabled"), &LatexLabel3D::set_draw_flag);
	ClassDB::bind_method(D_METHOD("get_draw_flag", "flag"), &LatexLabel3D::get_draw_flag);
	ClassDB::bind_method(D_METHOD("set_alpha_cut_mode", "mode"), &LatexLabel3D::set_alpha_cut_mode);
	ClassDB::bind_method(D_METHOD("get_alpha_cut_mode"), &LatexLabel3D::get_alpha_cut_mode);
	ClassDB::bind_method(D_METHOD("set_alpha_scissor_threshold", "threshold"), &LatexLabel3D::set_alpha_scissor_threshold);
	ClassDB::bind_method(D_METHOD("get_alpha_scissor_threshold"), &LatexLabel3D::get_alpha_scissor_threshold);
	ClassDB::bind_method(D_METHOD("set_alpha_hash_scale", "scale"), &LatexLabel3D::set_alpha_hash_scale);
	ClassDB::bind_method(D_METHOD("get_alpha_hash_scale"), &LatexLabel3D::get_alpha_hash_scale);
	ClassDB::bind_method(D_METHOD("set_texture_filter", "filter"), &LatexLabel3D::set_texture_filter);
	ClassDB::bind_method(D_METHOD("get_texture_filter"), &LatexLabel3D::get_texture_filter);
	ClassDB::bind_method(D_METHOD("set_render_priority", "priority"), &LatexLabel3D::set_render_priority);
	ClassDB::bind_method(D_METHOD("get_render_priority"), &LatexLabel3D::get_render_priority);
	ClassDB::bind_method(D_METHOD("get_render_error"), &LatexLabel3D::get_render_error);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "display_mode"), "set_display_mode", "is_display_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "font_size", PROPERTY_HINT_RANGE, "1,256,1,or_greater,suffix:px"), "set_font_size", "get_font_size");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "modulate"), "set_modulate", "get_modulate");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "horizontal_alignment", PROPERTY_HINT_ENUM, "Left,Center,Right,Fill"), "set_horizontal_alignment", "get_horizontal_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vertical_alignment", PROPERTY_HINT_ENUM, "Top,Center,Bottom"), "set_vertical_alignment", "get_vertical_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pixel_size", PROPERTY_HINT_RANGE, "0.0001,128,0.0001,suffix:m"), "set_pixel_size", "get_pixel_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset", PROPERTY_HINT_NONE, "suffix:px"), "set_offset", "get_offset");

	ADD_GROUP("Rendering", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "billboard", PROPERTY_HINT_ENUM, "Disabled,Enabled,Y-Billboard"), "set_billboard_mode", "get_billboard_mode");
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "shaded"), "set_draw_flag", "get_draw_flag", FLAG_SHADED);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "double_sided"), "set_draw_flag", "get_draw_flag", FLAG_DOUBLE_SIDED);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "no_depth_test"), "set_draw_flag", "get_draw_flag", FLAG_DISABLE_DEPTH_TEST);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "fixed_size"), "set_draw_flag", "get_draw_flag", FLAG_FIXED_SIZE);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "alpha_cut", PROPERTY_HINT_ENUM, "Disabled,Discard,Opaque Pre-Pass,Alpha Hash"), "set_alpha_cut_mode", "get_alpha_cut_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "alpha_scissor_threshold", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_alpha_scissor_threshold", "get_alpha_scissor_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "alpha_hash_scale", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_alpha_hash_scale", "get_alpha_hash_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_filter", PROPERTY_HINT_ENUM, "Nearest,Linear,Nearest Mipmap,Linear Mipmap,Nearest Mipmap Anisotropic,Linear Mipmap Anisotropic"), "set_texture_filter", "get_texture_filter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_priority", PROPERTY_HINT_RANGE, "-128,127,1"), "set_render_priority", "get_render_priority");
}

void LatexLabel3D::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_TRANSLATION_CHANGED) {
		queue_rebuild();
	}
}

void LatexLabel3D::_validate_property(PropertyInfo& p_property) const {
	if (p_property.name == StringName("mesh")) {
		p_property.usage &= ~(PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE);
	}
}

void LatexLabel3D::set_text(const String& p_text) {
	if (text == p_text) {
		return;
	}
	text = p_text;
	queue_rebuild();
}

String LatexLabel3D::get_text() const {
	return text;
}

void LatexLabel3D::set_display_mode(bool p_enabled) {
	if (display_mode == p_enabled) {
		return;
	}
	display_mode = p_enabled;
	queue_rebuild();
}

bool LatexLabel3D::is_display_mode() const {
	return display_mode;
}

void LatexLabel3D::set_font_size(int p_size) {
	ERR_FAIL_COND_MSG(p_size <= 0, "Font size must be positive.");
	if (font_size == p_size) {
		return;
	}
	font_size = p_size;
	queue_rebuild();
}

int LatexLabel3D::get_font_size() const {
	return font_size;
}

void LatexLabel3D::set_modulate(const Color& p_color) {
	material->set_albedo(p_color);
}

Color LatexLabel3D::get_modulate() const {
	return material->get_albedo();
}

void LatexLabel3D::set_horizontal_alignment(HorizontalAlignment p_alignment) {
	ERR_FAIL_INDEX(static_cast<int>(p_alignment), 4);
	if (horizontal_alignment == p_alignment) {
		return;
	}
	horizontal_alignment = p_alignment;
	update_quad();
}

HorizontalAlignment LatexLabel3D::get_horizontal_alignment() const {
	return horizontal_alignment;
}

void LatexLabel3D::set_vertical_alignment(VerticalAlignment p_alignment) {
	ERR_FAIL_INDEX(static_cast<int>(p_alignment), 3);
	if (vertical_alignment == p_alignment) {
		return;
	}
	vertical_alignment = p_alignment;
	update_quad();
}

VerticalAlignment LatexLabel3D::get_vertical_alignment() const {
	return vertical_alignment;
}

void LatexLabel3D::set_pixel_size(float p_size) {
	ERR_FAIL_COND_MSG(!std::isfinite(p_size) || p_size <= 0.0f, "Pixel size must be a finite positive number.");
	if (pixel_size == p_size) {
		return;
	}
	pixel_size = p_size;
	update_quad();
}

float LatexLabel3D::get_pixel_size() const {
	return pixel_size;
}

void LatexLabel3D::set_offset(const Vector2& p_offset) {
	if (offset == p_offset) {
		return;
	}
	offset = p_offset;
	update_quad();
}

Vector2 LatexLabel3D::get_offset() const {
	return offset;
}

void LatexLabel3D::set_billboard_mode(BaseMaterial3D::BillboardMode p_mode) {
	ERR_FAIL_INDEX(static_cast<int>(p_mode), 3);
	material->set_billboard_mode(p_mode);
}

BaseMaterial3D::BillboardMode LatexLabel3D::get_billboard_mode() const {
	return material->get_billboard_mode();
}

void LatexLabel3D::set_draw_flag(DrawFlags p_flag, bool p_enabled) {
	ERR_FAIL_INDEX(static_cast<int>(p_flag), FLAG_MAX);
	switch (p_flag) {
	case FLAG_SHADED:
		material->set_shading_mode(p_enabled ? BaseMaterial3D::SHADING_MODE_PER_PIXEL : BaseMaterial3D::SHADING_MODE_UNSHADED);
		break;
	case FLAG_DOUBLE_SIDED:
		material->set_cull_mode(p_enabled ? BaseMaterial3D::CULL_DISABLED : BaseMaterial3D::CULL_BACK);
		break;
	case FLAG_DISABLE_DEPTH_TEST:
		material->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, p_enabled);
		break;
	case FLAG_FIXED_SIZE:
		material->set_flag(BaseMaterial3D::FLAG_FIXED_SIZE, p_enabled);
		break;
	case FLAG_MAX:
		break;
	}
}

bool LatexLabel3D::get_draw_flag(DrawFlags p_flag) const {
	ERR_FAIL_INDEX_V(static_cast<int>(p_flag), FLAG_MAX, false);
	switch (p_flag) {
	case FLAG_SHADED:
		return material->get_shading_mode() != BaseMaterial3D::SHADING_MODE_UNSHADED;
	case FLAG_DOUBLE_SIDED:
		return material->get_cull_mode() == BaseMaterial3D::CULL_DISABLED;
	case FLAG_DISABLE_DEPTH_TEST:
		return material->get_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST);
	case FLAG_FIXED_SIZE:
		return material->get_flag(BaseMaterial3D::FLAG_FIXED_SIZE);
	case FLAG_MAX:
		return false;
	}
	return false;
}

void LatexLabel3D::set_alpha_cut_mode(AlphaCutMode p_mode) {
	ERR_FAIL_INDEX(static_cast<int>(p_mode), ALPHA_CUT_HASH + 1);
	material->set_transparency(alpha_cut_modes[p_mode]);
}

LatexLabel3D::AlphaCutMode LatexLabel3D::get_alpha_cut_mode() const {
	const BaseMaterial3D::Transparency transparency = material->get_transparency();
	for (int mode = ALPHA_CUT_DISABLED; mode <= ALPHA_CUT_HASH; mode++) {
		if (alpha_cut_modes[mode] == transparency) {
			return static_cast<AlphaCutMode>(mode);
		}
	}
	return ALPHA_CUT_DISABLED;
}

void LatexLabel3D::set_alpha_scissor_threshold(float p_threshold) {
	material->set_alpha_scissor_threshold(p_threshold);
}

float LatexLabel3D::get_alpha_scissor_threshold() const {
	return material->get_alpha_scissor_threshold();
}

void LatexLabel3D::set_alpha_hash_scale(float p_scale) {
	material->set_alpha_hash_scale(p_scale);
}

float LatexLabel3D::get_alpha_hash_scale() const {
	return material->get_alpha_hash_scale();
}

void LatexLabel3D::set_texture_filter(BaseMaterial3D::TextureFilter p_filter) {
	material->set_texture_filter(p_filter);
}

BaseMaterial3D::TextureFilter LatexLabel3D::get_texture_filter() const {
	return material->get_texture_filter();
}

void LatexLabel3D::set_render_priority(int p_priority) {
	material->set_render_priority(p_priority);
}

int LatexLabel3D::get_render_priority() const {
	return material->get_render_priority();
}

String LatexLabel3D::get_render_error() const {
	return render.is_valid() ? render->get_render_error() : String();
}

PackedStringArray LatexLabel3D::_get_configuration_warnings() const {
	PackedStringArray warnings;
	const String error = get_render_error();
	if (!error.is_empty()) {
		warnings.push_back(error);
	}
	return warnings;
}

void LatexLabel3D::queue_rebuild() {
	if (rebuild_queued || !is_inside_tree()) {
		return;
	}
	rebuild_queued = true;
	callable_mp(this, &LatexLabel3D::rebuild).call_deferred();
}

void LatexLabel3D::rebuild() {
	rebuild_queued = false;
	if (!is_inside_tree()) {
		return;
	}
	if (render.is_null()) {
		render.instantiate();
	}
	render->parse(atr(text), font_size, display_mode ? TeXRender::STYLE_DISPLAY : TeXRender::STYLE_TEXT);
	update_configuration_warnings();
	if (!render->is_valid()) {
		set_mesh(Ref<Mesh>());
		if (viewport != nullptr) {
			viewport->set_update_mode(SubViewport::UPDATE_DISABLED);
			viewport->set_size(Vector2i(2, 2));
		}
		return;
	}

	ensure_viewport();
	const Vector2i extent = render->get_size().max(Vector2i(1, 1)) + Vector2i(texture_padding * 2, texture_padding * 2);
	// Limit the render target while preserving the formula's logical size and aspect ratio.
	const float density = std::min(1.0f, max_texture_extent / std::max(extent.x, extent.y));
	const Vector2i texture_size = Vector2i((Vector2(extent) * density).ceil()).max(Vector2i(2, 2));
	viewport->set_size_2d_override(extent);
	viewport->set_size(texture_size);
	update_quad();
	set_mesh(quad);
	canvas->queue_redraw();
	viewport->set_update_mode(SubViewport::UPDATE_ONCE);
}

void LatexLabel3D::ensure_viewport() {
	if (viewport != nullptr) {
		return;
	}
	viewport = memnew(SubViewport);
	viewport->set_name("_LatexViewport");
	viewport->set_size(Vector2i(2, 2));
	viewport->set_transparent_background(true);
	viewport->set_disable_3d(true);
	viewport->set_disable_input(true);
	viewport->set_size_2d_override_stretch(true);
	viewport->set_update_mode(SubViewport::UPDATE_DISABLED);
	add_child(viewport, false, INTERNAL_MODE_BACK);

	canvas = memnew(Node2D);
	viewport->add_child(canvas, false, INTERNAL_MODE_BACK);
	canvas->connect("draw", callable_mp(this, &LatexLabel3D::draw_formula));
	material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, viewport->get_texture());
}

void LatexLabel3D::update_quad() {
	if (render.is_null() || !render->is_valid()) {
		return;
	}
	const Vector2 formula_size = Vector2(render->get_size()).max(Vector2(1.0f, 1.0f));
	Vector2 center = offset;
	if (horizontal_alignment == HORIZONTAL_ALIGNMENT_LEFT) {
		center.x += formula_size.x * 0.5f;
	}
	else if (horizontal_alignment == HORIZONTAL_ALIGNMENT_RIGHT) {
		center.x -= formula_size.x * 0.5f;
	}
	if (vertical_alignment == VERTICAL_ALIGNMENT_TOP) {
		center.y -= formula_size.y * 0.5f;
	}
	else if (vertical_alignment == VERTICAL_ALIGNMENT_BOTTOM) {
		center.y += formula_size.y * 0.5f;
	}
	quad->set_size((formula_size + Vector2(texture_padding * 2, texture_padding * 2)) * pixel_size);
	quad->set_center_offset(Vector3(center.x * pixel_size, center.y * pixel_size, 0.0f));
}

void LatexLabel3D::draw_formula() {
	if (render.is_valid()) {
		render->draw(canvas, Vector2(texture_padding, texture_padding));
	}
}
