#include "markdown_label.h"

#include "formats/html/html_markup.h"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_settings.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/popup_menu.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_uid.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/style_box_line.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <algorithm>
#include <climits>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

using namespace godot;

namespace {
	static bool packed_string_array_has(const PackedStringArray& p_array, const StringName& p_name) {
		const String name = String(p_name);
		for (int32_t i = 0; i < p_array.size(); i++) {
			if (p_array[i] == name) {
				return true;
			}
		}

		return false;
	}

	static float get_theme_base_scale_or_one(Control* p_owner) {
		const float base_scale = p_owner == nullptr ? 1.0f : p_owner->get_theme_default_base_scale();
		return base_scale > 0.0f ? base_scale : 1.0f;
	}

	static int32_t scale_theme_fallback(int32_t p_value, float p_base_scale) {
		return static_cast<int32_t>(Math::round(static_cast<double>(p_value) * static_cast<double>(p_base_scale)));
	}

	static float scale_theme_fallback(float p_value, float p_base_scale) {
		return p_value * p_base_scale;
	}

	static int32_t get_explicit_theme_font_size(Control* p_owner, const StringName& p_type, const StringName& p_name) {
		if (p_owner != nullptr && p_owner->has_theme_font_size_override(p_name)) {
			const int32_t value = p_owner->get_theme_font_size(p_name, p_type);
			if (value > 0) {
				return value;
			}
		}

		const Ref<Theme> theme = p_owner == nullptr ? Ref<Theme>() : p_owner->get_theme();
		if (theme.is_valid() && packed_string_array_has(theme->get_font_size_list(String(p_type)), p_name)) {
			const int32_t value = theme->get_font_size(p_name, p_type);
			if (value > 0) {
				return value;
			}
		}

		return -1;
	}

	static int32_t get_explicit_theme_constant(Control* p_owner, const StringName& p_type, const StringName& p_name) {
		if (p_owner != nullptr && p_owner->has_theme_constant_override(p_name)) {
			return p_owner->get_theme_constant(p_name, p_type);
		}

		const Ref<Theme> theme = p_owner == nullptr ? Ref<Theme>() : p_owner->get_theme();
		if (theme.is_valid() && packed_string_array_has(theme->get_constant_list(String(p_type)), p_name)) {
			return theme->get_constant(p_name, p_type);
		}

		if (p_owner != nullptr && p_owner->has_theme_constant(p_name, p_type)) {
			return p_owner->get_theme_constant(p_name, p_type);
		}

		return INT_MIN;
	}

	static int32_t get_theme_font_size_or(Control* p_owner, const StringName& p_type, const StringName& p_name, int32_t p_default) {
		const int32_t explicit_font_size = get_explicit_theme_font_size(p_owner, p_type, p_name);
		if (explicit_font_size > 0) {
			return explicit_font_size;
		}

		const int32_t explicit_constant = get_explicit_theme_constant(p_owner, p_type, p_name);
		if (explicit_constant > 0) {
			return explicit_constant;
		}

		return p_default;
	}

	static int32_t get_theme_constant_or(Control* p_owner, const StringName& p_type, const StringName& p_name, int32_t p_default) {
		const int32_t explicit_constant = get_explicit_theme_constant(p_owner, p_type, p_name);
		if (explicit_constant != INT_MIN) {
			return explicit_constant;
		}

		return p_default;
	}

	static int32_t get_scaled_theme_constant_or(Control* p_owner, const StringName& p_type, const StringName& p_name, int32_t p_default, float p_base_scale) {
		const int32_t explicit_constant = get_explicit_theme_constant(p_owner, p_type, p_name);
		if (explicit_constant != INT_MIN) {
			return explicit_constant;
		}

		return scale_theme_fallback(p_default, p_base_scale);
	}

	static Color get_theme_color_or(Control* p_owner, const StringName& p_type, const StringName& p_name, const Color& p_default) {
		const Ref<Theme> theme = p_owner == nullptr ? Ref<Theme>() : p_owner->get_theme();
		if (theme.is_valid() && theme->has_color(p_name, p_type)) {
			return theme->get_color(p_name, p_type);
		}
		if (p_owner != nullptr && p_owner->has_theme_color(p_name, p_type)) {
			return p_owner->get_theme_color(p_name, p_type);
		}

		return p_default;
	}

	static Ref<Font> get_theme_font_or(Control* p_owner, const StringName& p_type, const StringName& p_name) {
		if (p_owner != nullptr && p_owner->has_theme_font_override(p_name)) {
			return p_owner->get_theme_font(p_name, p_type);
		}
		const Ref<Theme> theme = p_owner == nullptr ? Ref<Theme>() : p_owner->get_theme();
		if (theme.is_valid() && theme->has_font(p_name, p_type)) {
			return theme->get_font(p_name, p_type);
		}
		if (p_owner != nullptr && p_owner->has_theme_font(p_name, p_type)) {
			return p_owner->get_theme_font(p_name, p_type);
		}
		if (p_owner != nullptr) {
			return p_owner->get_theme_default_font();
		}

		return Ref<Font>();
	}

	static Ref<StyleBox> get_theme_stylebox_or(Control* p_owner, const StringName& p_type, const StringName& p_name) {
		const Ref<Theme> theme = p_owner == nullptr ? Ref<Theme>() : p_owner->get_theme();
		if (theme.is_valid() && theme->has_stylebox(p_name, p_type)) {
			return theme->get_stylebox(p_name, p_type);
		}
		if (p_owner != nullptr && p_owner->has_theme_stylebox(p_name, p_type)) {
			return p_owner->get_theme_stylebox(p_name, p_type);
		}

		return Ref<StyleBox>();
	}

	static Ref<Texture2D> get_theme_icon_or(Control* p_owner, const StringName& p_type, const StringName& p_name) {
		const Ref<Theme> theme = p_owner == nullptr ? Ref<Theme>() : p_owner->get_theme();
		if (theme.is_valid() && theme->has_icon(p_name, p_type)) {
			return theme->get_icon(p_name, p_type);
		}
		if (p_owner != nullptr && p_owner->has_theme_icon(p_name, p_type)) {
			return p_owner->get_theme_icon(p_name, p_type);
		}
		return Ref<Texture2D>();
	}

	static Ref<StyleBoxFlat> create_internal_stylebox(const Color& p_background_color, const Color& p_border_color, int32_t p_border_left, int32_t p_border_top, int32_t p_border_right, int32_t p_border_bottom, float p_margin_left, float p_margin_top, float p_margin_right, float p_margin_bottom, int32_t p_corner_radius = 0, bool p_draw_center = true, int32_t p_corner_detail = 8, float p_base_scale = 1.0f) {
		Ref<StyleBoxFlat> stylebox;
		stylebox.instantiate();
		stylebox->set_bg_color(p_background_color);
		stylebox->set_border_color(p_border_color);
		stylebox->set_border_width(SIDE_LEFT, scale_theme_fallback(p_border_left, p_base_scale));
		stylebox->set_border_width(SIDE_TOP, scale_theme_fallback(p_border_top, p_base_scale));
		stylebox->set_border_width(SIDE_RIGHT, scale_theme_fallback(p_border_right, p_base_scale));
		stylebox->set_border_width(SIDE_BOTTOM, scale_theme_fallback(p_border_bottom, p_base_scale));
		stylebox->set_content_margin(SIDE_LEFT, scale_theme_fallback(p_margin_left, p_base_scale));
		stylebox->set_content_margin(SIDE_TOP, scale_theme_fallback(p_margin_top, p_base_scale));
		stylebox->set_content_margin(SIDE_RIGHT, scale_theme_fallback(p_margin_right, p_base_scale));
		stylebox->set_content_margin(SIDE_BOTTOM, scale_theme_fallback(p_margin_bottom, p_base_scale));
		stylebox->set_corner_radius_all(scale_theme_fallback(p_corner_radius, p_base_scale));
		stylebox->set_draw_center(p_draw_center);
		stylebox->set_corner_detail(p_corner_detail);
		return stylebox;
	}

	static float get_stylebox_margin_or(const Ref<StyleBox>& p_stylebox, Side p_side, float p_default) {
		if (p_stylebox.is_valid()) {
			return p_stylebox->get_margin(p_side);
		}

		return p_default;
	}

	struct EditorColorPair {
		uint32_t from_rgb;
		uint32_t to_rgb;
	};

	struct EditorLightThemeInfo {
		bool enabled = false;
		Ref<Theme> editor_theme;
		Color error_color = Color(1.0f, 0.161f, 0.161f, 1.0f);
		Color success_color = Color(0.0f, 0.6f, 0.2f, 1.0f);
		Color warning_color = Color(0.84f, 0.55f, 0.04f, 1.0f);
	};

	static uint32_t color_to_rgb_key(const Color& p_color) {
		return (static_cast<uint32_t>(p_color.get_r8()) << 16) | (static_cast<uint32_t>(p_color.get_g8()) << 8) | static_cast<uint32_t>(p_color.get_b8());
	}

	static Color color_from_rgb_key(uint32_t p_rgb, float p_alpha) {
		const float red = static_cast<float>((p_rgb >> 16) & 0xff) / 255.0f;
		const float green = static_cast<float>((p_rgb >> 8) & 0xff) / 255.0f;
		const float blue = static_cast<float>(p_rgb & 0xff) / 255.0f;
		return Color(red, green, blue, p_alpha);
	}

	static Color get_editor_theme_color_or(const Ref<Theme>& p_theme, const StringName& p_name, const Color& p_default) {
		if (p_theme.is_valid() && p_theme->has_color(p_name, "Editor")) {
			return p_theme->get_color(p_name, "Editor");
		}

		return p_default;
	}

	static bool is_light_editor_base_color(const Color& p_base_color) {
		return p_base_color.get_luminance() >= 0.5f;
	}

	static EditorLightThemeInfo get_editor_light_theme_info(Control* p_owner) {
		EditorLightThemeInfo info;
		Engine* engine = Engine::get_singleton();
		EditorInterface* editor_interface = engine != nullptr && engine->is_editor_hint() ? EditorInterface::get_singleton() : nullptr;
		if (editor_interface != nullptr) {
			info.editor_theme = editor_interface->get_editor_theme();
			if (info.editor_theme.is_valid()) {
				info.error_color = get_editor_theme_color_or(info.editor_theme, "error_color", info.error_color);
				info.success_color = get_editor_theme_color_or(info.editor_theme, "success_color", info.success_color);
				info.warning_color = get_editor_theme_color_or(info.editor_theme, "warning_color", info.warning_color);
				if (info.editor_theme->has_color("base_color", "Editor")) {
					info.enabled = is_light_editor_base_color(info.editor_theme->get_color("base_color", "Editor"));
					return info;
				}
				if (info.editor_theme->has_color("font_color", "Editor")) {
					info.enabled = info.editor_theme->get_color("font_color", "Editor").get_luminance() < 0.5f;
					return info;
				}
			}

			const Ref<EditorSettings> editor_settings = editor_interface->get_editor_settings();
			if (editor_settings.is_valid() && editor_settings->has_setting("interface/theme/base_color")) {
				const Variant base_color_value = editor_settings->get_setting("interface/theme/base_color");
				if (base_color_value.get_type() == Variant::COLOR) {
					info.enabled = is_light_editor_base_color(base_color_value);
					return info;
				}
			}
		}

		if (p_owner != nullptr && p_owner->has_theme_color("base_color", "Editor")) {
			info.enabled = is_light_editor_base_color(p_owner->get_theme_color("base_color", "Editor"));
		}

		return info;
	}

	static Color convert_editor_light_color(const Color& p_color, const EditorLightThemeInfo& p_theme_info) {
		if (!p_theme_info.enabled || p_color.a <= 0.0f) {
			return p_color;
		}

		const uint32_t rgb = color_to_rgb_key(p_color);
		if (rgb == 0xff5f5f) {
			return Color(p_theme_info.error_color.r, p_theme_info.error_color.g, p_theme_info.error_color.b, p_color.a);
		}
		if (rgb == 0x5fff97) {
			return Color(p_theme_info.success_color.r, p_theme_info.success_color.g, p_theme_info.success_color.b, p_color.a);
		}
		if (rgb == 0xffdd65) {
			return Color(p_theme_info.warning_color.r, p_theme_info.warning_color.g, p_theme_info.warning_color.b, p_color.a);
		}

		static constexpr EditorColorPair editor_color_map[] = {
			{ 0x478cbf, 0x478cbf },
			{ 0x414042, 0x414042 },
			{ 0xffffff, 0x414141 },
			{ 0xfefefe, 0xfefefe },
			{ 0x000000, 0xbfbfbf },
			{ 0x010101, 0x010101 },
			{ 0xff0000, 0xff0000 },
			{ 0x00ff00, 0x00ff00 },
			{ 0x0000ff, 0x0000ff },
			{ 0xe0e0e0, 0x5a5a5a },
			{ 0x808080, 0x808080 },
			{ 0xb3b3b3, 0x363636 },
			{ 0x699ce8, 0x699ce8 },
			{ 0xf9f9f9, 0x606060 },
			{ 0x5fb2ff, 0x0079f0 },
			{ 0x003e7a, 0x2b74bb },
			{ 0xf7f5cf, 0x615f3a },
			{ 0xc38ef1, 0xa85de9 },
			{ 0x8da5f3, 0x3d64dd },
			{ 0x7582a8, 0x6d83c8 },
			{ 0xfc7f7f, 0xcd3838 },
			{ 0xb56d6d, 0xbe6a6a },
			{ 0x99c4ff, 0x4589e6 },
			{ 0x869ebf, 0x7097cd },
			{ 0xffa6bd, 0xe65c7f },
			{ 0xbf909c, 0xcd8b9c },
			{ 0x8eef97, 0x2fa139 },
			{ 0x76ad7b, 0x64a66a },
			{ 0xf0caa0, 0x844b0e },
			{ 0xff4545, 0xff2929 },
			{ 0xffe345, 0xffe337 },
			{ 0x80ff45, 0x74ff34 },
			{ 0x45ffa2, 0x2cff98 },
			{ 0x45d7ff, 0x22ccff },
			{ 0x8045ff, 0x702aff },
			{ 0xff4596, 0xff2781 },
			{ 0xe1da5b, 0xd6cf4b },
			{ 0x62aeff, 0x1678e0 },
			{ 0x75d1e6, 0x41acc5 },
			{ 0x84ffee, 0x49ccba },
			{ 0xf70000, 0xc91616 },
			{ 0xeec315, 0xd58c0b },
			{ 0xdbee15, 0xb7d10a },
			{ 0x288027, 0x218309 },
			{ 0xffca5f, 0xfea900 },
			{ 0x2998ff, 0x68b6ff },
			{ 0xa2d2ff, 0x4998e3 },
			{ 0x69c4d4, 0x29a3cc },
			{ 0xea7940, 0xbd5e2c },
			{ 0xff2b88, 0xbd165f },
			{ 0xeac840, 0xbd9d1f },
			{ 0x3cf34e, 0x16a827 },
			{ 0x2877f6, 0x236be6 },
			{ 0xeae440, 0x9f9722 },
			{ 0xa448f0, 0x9853ce },
			{ 0x5ad5c4, 0x0a9c88 },
			{ 0xd6d6d6, 0x474747 },
			{ 0x474747, 0xd6d6d6 },
			{ 0x919191, 0x6e6e6e },
			{ 0xfce00e, 0xaa8d24 },
			{ 0x0e71fc, 0x0350bd },
			{ 0xc6ced4, 0x828f9b },
			{ 0x41ecad, 0x25e3a0 },
			{ 0x6f91f0, 0x6d8eeb },
			{ 0x5abbef, 0x4fb2e9 },
			{ 0x35d4f4, 0x27ccf0 },
			{ 0x4593ec, 0x4690e7 },
			{ 0xee5677, 0xee7991 },
			{ 0xe1ec41, 0xb2bb19 },
			{ 0x54ed9e, 0x57e99f },
			{ 0x417aec, 0x6993ec },
			{ 0x55f3e3, 0x12d5c3 },
			{ 0xf74949, 0xf77070 },
			{ 0x44bd44, 0x46b946 },
			{ 0xec418e, 0xec69a3 },
			{ 0xf1738f, 0xee758e },
			{ 0x41ec80, 0x2ce573 },
			{ 0xb9ec41, 0x96ce1a },
			{ 0xf68f45, 0xf49047 },
			{ 0xac73f1, 0xad76ee },
			{ 0xde66f0, 0xdc6aed },
			{ 0xf066bd, 0xed6abd },
			{ 0x77ce57, 0x67c046 },
			{ 0xea686c, 0xd95256 },
			{ 0xeac968, 0xd9b64f },
			{ 0xcf68ea, 0xc050dd },
		};

		for (const EditorColorPair& pair : editor_color_map) {
			if (rgb == pair.from_rgb) {
				return color_from_rgb_key(pair.to_rgb, p_color.a);
			}
		}

		const float max_channel = std::max<float>(p_color.r, std::max<float>(p_color.g, p_color.b));
		const float min_channel = std::min<float>(p_color.r, std::min<float>(p_color.g, p_color.b));
		const float luminance = p_color.get_luminance();
		if (max_channel - min_channel <= 0.025f && luminance > 0.64f && rgb != 0xfefefe) {
			const float weight = CLAMP((luminance - 0.64f) / 0.36f, 0.0f, 1.0f);
			const float gray = Math::lerp(0.52f, 0.28f, weight);
			return Color(gray, gray, gray, p_color.a);
		}

		return p_color;
	}

	static Ref<StyleBox> convert_editor_light_stylebox(const Ref<StyleBox>& p_stylebox, const EditorLightThemeInfo& p_theme_info) {
		if (!p_theme_info.enabled || p_stylebox.is_null()) {
			return p_stylebox;
		}

		const Ref<StyleBoxFlat> source_flat = p_stylebox;
		if (source_flat.is_valid()) {
			Ref<StyleBoxFlat> converted;
			converted.instantiate();
			converted->set_bg_color(convert_editor_light_color(source_flat->get_bg_color(), p_theme_info));
			converted->set_border_color(convert_editor_light_color(source_flat->get_border_color(), p_theme_info));
			converted->set_border_blend(source_flat->get_border_blend());
			converted->set_draw_center(source_flat->is_draw_center_enabled());
			converted->set_skew(source_flat->get_skew());
			converted->set_shadow_color(convert_editor_light_color(source_flat->get_shadow_color(), p_theme_info));
			converted->set_shadow_size(source_flat->get_shadow_size());
			converted->set_shadow_offset(source_flat->get_shadow_offset());
			converted->set_anti_aliased(source_flat->is_anti_aliased());
			converted->set_aa_size(source_flat->get_aa_size());
			converted->set_corner_detail(source_flat->get_corner_detail());

			for (int32_t i = 0; i < 4; i++) {
				const Side side = static_cast<Side>(i);
				converted->set_content_margin(side, source_flat->get_content_margin(side));
				converted->set_border_width(side, source_flat->get_border_width(side));
				converted->set_expand_margin(side, source_flat->get_expand_margin(side));
			}
			for (int32_t i = 0; i < 4; i++) {
				const Corner corner = static_cast<Corner>(i);
				converted->set_corner_radius(corner, source_flat->get_corner_radius(corner));
			}

			return converted;
		}

		const Ref<StyleBoxLine> source_line = p_stylebox;
		if (source_line.is_valid()) {
			Ref<StyleBoxLine> converted;
			converted.instantiate();
			converted->set_color(convert_editor_light_color(source_line->get_color(), p_theme_info));
			converted->set_thickness(source_line->get_thickness());
			converted->set_grow_begin(source_line->get_grow_begin());
			converted->set_grow_end(source_line->get_grow_end());
			converted->set_vertical(source_line->is_vertical());

			for (int32_t i = 0; i < 4; i++) {
				const Side side = static_cast<Side>(i);
				converted->set_content_margin(side, source_line->get_content_margin(side));
			}

			return converted;
		}

		return p_stylebox;
	}

	static void apply_editor_light_theme(MarkdownThemeCache& p_cache, const EditorLightThemeInfo& p_theme_info) {
		if (!p_theme_info.enabled) {
			return;
		}

		// 统一在主题缓存阶段转换，绘制代码不用关心编辑器亮暗主题。
		p_cache.color.text = convert_editor_light_color(p_cache.color.text, p_theme_info);
		p_cache.color.link = convert_editor_light_color(p_cache.color.link, p_theme_info);
		p_cache.color.code_text = convert_editor_light_color(p_cache.color.code_text, p_theme_info);
		p_cache.color.code_block = convert_editor_light_color(p_cache.color.code_block, p_theme_info);
		p_cache.color.code_block_background = convert_editor_light_color(p_cache.color.code_block_background, p_theme_info);
		p_cache.color.highlight = convert_editor_light_color(p_cache.color.highlight, p_theme_info);
		p_cache.color.highlight_font = convert_editor_light_color(p_cache.color.highlight_font, p_theme_info);
		p_cache.color.strikethrough = convert_editor_light_color(p_cache.color.strikethrough, p_theme_info);
		p_cache.color.selection_color = convert_editor_light_color(p_cache.color.selection_color, p_theme_info);
		p_cache.color.list_marker = convert_editor_light_color(p_cache.color.list_marker, p_theme_info);
		p_cache.color.table_header_font = convert_editor_light_color(p_cache.color.table_header_font, p_theme_info);
		p_cache.color.table_cell_font = convert_editor_light_color(p_cache.color.table_cell_font, p_theme_info);
		p_cache.color.footnote_ref = convert_editor_light_color(p_cache.color.footnote_ref, p_theme_info);
		p_cache.color.footnote_text = convert_editor_light_color(p_cache.color.footnote_text, p_theme_info);
		for (int32_t i = 0; i < 6; i++) {
			p_cache.color.heading[i] = convert_editor_light_color(p_cache.color.heading[i], p_theme_info);
		}

		p_cache.stylebox.inline_code = convert_editor_light_stylebox(p_cache.stylebox.inline_code, p_theme_info);
		p_cache.stylebox.code_block_panel = convert_editor_light_stylebox(p_cache.stylebox.code_block_panel, p_theme_info);
		p_cache.stylebox.blockquote = convert_editor_light_stylebox(p_cache.stylebox.blockquote, p_theme_info);
		p_cache.stylebox.blockquote_nested = convert_editor_light_stylebox(p_cache.stylebox.blockquote_nested, p_theme_info);
		p_cache.stylebox.selection = convert_editor_light_stylebox(p_cache.stylebox.selection, p_theme_info);
		p_cache.stylebox.search_result = convert_editor_light_stylebox(p_cache.stylebox.search_result, p_theme_info);
		p_cache.stylebox.separator = convert_editor_light_stylebox(p_cache.stylebox.separator, p_theme_info);
		p_cache.stylebox.highlight = convert_editor_light_stylebox(p_cache.stylebox.highlight, p_theme_info);
		p_cache.stylebox.table_panel = convert_editor_light_stylebox(p_cache.stylebox.table_panel, p_theme_info);
		p_cache.stylebox.table_header_panel = convert_editor_light_stylebox(p_cache.stylebox.table_header_panel, p_theme_info);
		p_cache.stylebox.table_cell_panel = convert_editor_light_stylebox(p_cache.stylebox.table_cell_panel, p_theme_info);
		p_cache.stylebox.table_striped_panel = convert_editor_light_stylebox(p_cache.stylebox.table_striped_panel, p_theme_info);
		p_cache.stylebox.footnote_ref = convert_editor_light_stylebox(p_cache.stylebox.footnote_ref, p_theme_info);
	}

	static Ref<Font> first_valid_font(const Ref<Font>& p_primary, const Ref<Font>& p_secondary, const Ref<Font>& p_fallback) {
		if (p_primary.is_valid()) {
			return p_primary;
		}
		if (p_secondary.is_valid()) {
			return p_secondary;
		}

		return p_fallback;
	}

	static Ref<Font> get_span_font(const MarkdownInlineSpan& p_span, const MarkdownThemeCache& p_cache, const Ref<Font>& p_normal_font) {
		if (p_span.code) {
			return p_cache.font.code.is_valid() ? p_cache.font.code : p_normal_font;
		}
		if (p_span.bold && p_span.italic) {
			return first_valid_font(p_cache.font.bold_italic, Ref<Font>(), p_normal_font);
		}
		if (p_span.bold) {
			return first_valid_font(p_cache.font.bold, Ref<Font>(), p_normal_font);
		}
		if (p_span.italic) {
			return first_valid_font(p_cache.font.italic, Ref<Font>(), p_normal_font);
		}

		return p_normal_font;
	}


	enum MarkdownImageLoadStatus {
		MARKDOWN_IMAGE_READY,
		MARKDOWN_IMAGE_LOADING,
		MARKDOWN_IMAGE_FAILED,
	};

	struct MarkdownImageCacheEntry {
		MarkdownImageLoadStatus status = MARKDOWN_IMAGE_FAILED;
		Ref<Texture2D> texture;
		String error;
		double gif_elapsed = 0.0;
	};

	static HashMap<String, MarkdownImageCacheEntry> s_image_cache;
	static HashMap<String, std::vector<uint64_t>> s_image_watchers;
	static std::vector<String> s_pending_local_image_uris;
	static int32_t s_live_image_canvas_count = 0;

	static bool is_remote_image_uri(const String& p_uri) {
		return p_uri.begins_with("http://") || p_uri.begins_with("https://");
	}

	static String image_uri_without_query(const String& p_uri) {
		String clean_uri = p_uri;
		const int64_t query_pos = clean_uri.find("?");
		if (query_pos >= 0) {
			clean_uri = clean_uri.substr(0, query_pos);
		}
		const int64_t hash_pos = clean_uri.find("#");
		if (hash_pos >= 0) {
			clean_uri = clean_uri.substr(0, hash_pos);
		}
		return clean_uri;
	}

	static bool is_gif_image_uri(const String& p_uri) {
		return image_uri_without_query(p_uri).to_lower().ends_with(".gif");
	}

	static bool has_gif_plugin_support() {
		static int32_t gif_support = -1;
		if (gif_support < 0) {
			gif_support = (ClassDB::class_exists("GIFTexture") || ClassDB::class_exists("GIFReader")) ? 1 : 0;
		}
		return gif_support == 1;
	}

	static void register_image_watcher(const String& p_uri, MarkdownLabelCanvas* p_canvas) {
		if (p_canvas == nullptr) {
			return;
		}

		std::vector<uint64_t>& watchers = s_image_watchers[p_uri];
		const uint64_t instance_id = p_canvas->get_instance_id();
		if (std::find(watchers.begin(), watchers.end(), instance_id) == watchers.end()) {
			watchers.push_back(instance_id);
		}
	}

	static void notify_image_watchers(const String& p_uri) {
		if (!s_image_watchers.has(p_uri)) {
			return;
		}

		std::vector<uint64_t> watchers = s_image_watchers[p_uri];
		s_image_watchers.erase(p_uri);
		for (uint64_t instance_id : watchers) {
			Object* object = ObjectDB::get_instance(instance_id);
			MarkdownLabelCanvas* canvas = Object::cast_to<MarkdownLabelCanvas>(object);
			if (canvas != nullptr) {
				canvas->refresh_image_content();
			}
		}
	}

	static bool has_pending_local_image_uri(const String& p_uri) {
		return std::find(s_pending_local_image_uris.begin(), s_pending_local_image_uris.end(), p_uri) != s_pending_local_image_uris.end();
	}

	static void add_pending_local_image_uri(const String& p_uri) {
		if (!has_pending_local_image_uri(p_uri)) {
			s_pending_local_image_uris.push_back(p_uri);
		}
	}

	static Ref<Texture2D> load_gif_texture_from_buffer(const PackedByteArray& p_body) {
		if (!has_gif_plugin_support()) {
			return Ref<Texture2D>();
		}

		Variant result = ClassDB::class_call_static("GIFTexture", "load_from_buffer", p_body);
		Ref<Resource> resource = result;
		Ref<Texture2D> texture = resource;
		return texture;
	}

	static bool is_animated_gif_texture(const Ref<Texture2D>& p_texture) {
		if (p_texture.is_null()) {
			return false;
		}

		Object* texture_object = p_texture.ptr();
		if (texture_object == nullptr || !texture_object->has_method("get_frame_count") || !texture_object->has_method("get_frame") || !texture_object->has_method("set_frame") || !texture_object->has_method("get_frame_delay")) {
			return false;
		}

		return static_cast<int32_t>(texture_object->call("get_frame_count")) > 1;
	}

	static Ref<Texture2D> decode_image_texture_from_buffer(const String& p_uri, const PackedByteArray& p_body) {
		if (p_body.is_empty()) {
			return Ref<Texture2D>();
		}

		if (is_gif_image_uri(p_uri)) {
			return load_gif_texture_from_buffer(p_body);
		}

		const String clean_uri = image_uri_without_query(p_uri).to_lower();
		std::vector<int32_t> decode_order;
		if (clean_uri.ends_with(".png")) {
			decode_order = { 0, 1, 2 };
		}
		else if (clean_uri.ends_with(".jpg") || clean_uri.ends_with(".jpeg")) {
			decode_order = { 1, 0, 2 };
		}
		else if (clean_uri.ends_with(".webp")) {
			decode_order = { 2, 0, 1 };
		}
		else {
			decode_order = { 0, 1, 2 };
		}

		for (int32_t decode_type : decode_order) {
			Ref<Image> image;
			image.instantiate();
			Error error = ERR_UNAVAILABLE;
			if (decode_type == 0) {
				error = image->load_png_from_buffer(p_body);
			}
			else if (decode_type == 1) {
				error = image->load_jpg_from_buffer(p_body);
			}
			else {
				error = image->load_webp_from_buffer(p_body);
			}

			if (error == OK && image.is_valid() && !image->is_empty()) {
				return ImageTexture::create_from_image(image);
			}
		}

		if (has_gif_plugin_support()) {
			return load_gif_texture_from_buffer(p_body);
		}

		return Ref<Texture2D>();
	}

	static MarkdownImageCacheEntry load_image_texture(const String& p_uri, MarkdownLabelCanvas* p_canvas) {
		if (p_uri.is_empty()) {
			MarkdownImageCacheEntry entry;
			entry.status = MARKDOWN_IMAGE_FAILED;
			entry.error = "empty_uri";
			return entry;
		}

		if (s_image_cache.has(p_uri)) {
			MarkdownImageCacheEntry entry = s_image_cache[p_uri];
			if (entry.status == MARKDOWN_IMAGE_LOADING) {
				register_image_watcher(p_uri, p_canvas);
			}
			return entry;
		}

		if (p_uri.begins_with("uid://")) {
			ResourceUID* resource_uid = ResourceUID::get_singleton();
			if (!resource_uid->has_id(resource_uid->text_to_id(p_uri))) {
				// Editor imports can register the UID after this render.
				MarkdownImageCacheEntry entry;
				entry.status = MARKDOWN_IMAGE_FAILED;
				entry.error = "unknown_resource_uid";
				return entry;
			}
		}

		if (is_remote_image_uri(p_uri)) {
			MarkdownImageCacheEntry entry;
			if (is_gif_image_uri(p_uri) && !has_gif_plugin_support()) {
				entry.status = MARKDOWN_IMAGE_FAILED;
				entry.error = "gif_plugin_missing";
				s_image_cache[p_uri] = entry;
				return entry;
			}

			entry.status = MARKDOWN_IMAGE_LOADING;
			s_image_cache[p_uri] = entry;
			register_image_watcher(p_uri, p_canvas);
			if (p_canvas != nullptr) {
				p_canvas->request_remote_image(p_uri);
			}
			return entry;
		}

		MarkdownImageCacheEntry entry;
		if (is_gif_image_uri(p_uri) && !has_gif_plugin_support()) {
			entry.status = MARKDOWN_IMAGE_FAILED;
			entry.error = "gif_plugin_missing";
			s_image_cache[p_uri] = entry;
			return entry;
		}

		entry.status = MARKDOWN_IMAGE_LOADING;
		s_image_cache[p_uri] = entry;
		register_image_watcher(p_uri, p_canvas);
		if (p_canvas != nullptr) {
			p_canvas->request_local_image(p_uri);
		}
		return entry;
	}

	static Vector2 fit_texture_size(const Ref<Texture2D>& p_texture, float p_max_width,
		const Vector2i& p_requested_size = {}, float p_max_height = std::numeric_limits<float>::max()) {
		if (p_texture.is_null() || p_texture->get_width() <= 0 || p_texture->get_height() <= 0) {
			return Vector2(1.0f, 1.0f);
		}

		Vector2 size = p_texture->get_size();
		const float aspect = size.x / size.y;
		if (p_requested_size.x > 0) {
			size = Vector2(p_requested_size.x, p_requested_size.x / aspect);
		}
		if (p_requested_size.y > 0) {
			size = Vector2(p_requested_size.x > 0 ? size.x : p_requested_size.y * aspect, p_requested_size.y);
		}
		const float max_width = std::max<float>(1.0f, p_max_width);
		const float scale = std::min({ 1.0f, max_width / size.x, p_max_height / size.y });
		return (size * scale).max(Vector2(1.0f, 1.0f));
	}

	static bool is_mermaid_block(const MarkdownBlock& p_block) {
		return p_block.type == MARKDOWN_BLOCK_CODE && p_block.end_line > 0 &&
			p_block.argument.to_lower() == "mermaid" && !p_block.text.is_empty();
	}

	static void collect_mermaid_sources(const std::vector<MarkdownBlock>& p_blocks, HashSet<String>& r_sources) {
		for (const MarkdownBlock& block : p_blocks) {
			if (is_mermaid_block(block)) {
				r_sources.insert(block.text);
			}
			if (!block.children.empty()) {
				collect_mermaid_sources(block.children, r_sources);
			}
			for (const MarkdownListItem& item : block.list_items) {
				collect_mermaid_sources(item.children, r_sources);
			}
		}
	}

	static bool add_mermaid_to_paragraph(const Ref<TextParagraph>& p_paragraph, const MermaidRenderCache& p_cache, const MarkdownBlock& p_block, float p_width, MarkdownParagraphObjects& r_objects) {
		const MermaidRenderCache::Result* result = is_mermaid_block(p_block) ? p_cache.get_result(p_block.text) : nullptr;
		if (result == nullptr || result->texture.is_null()) {
			return false;
		}
		const String key = "mermaid";
		const float render_scale = MermaidRenderCache::RASTER_SCALE;
		const Vector2 size = fit_texture_size(result->texture, p_width * render_scale) / render_scale;
		if (!p_paragraph->add_object(key, size, INLINE_ALIGNMENT_CENTER, p_block.text.length())) {
			return false;
		}
		r_objects.images.insert(key, result->texture);
		p_paragraph->set_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		return true;
	}

	static BitField<TextServer::LineBreakFlag> get_autowrap_trim_mask() {
		return TextServer::BREAK_TRIM_INDENT | TextServer::BREAK_TRIM_START_EDGE_SPACES | TextServer::BREAK_TRIM_END_EDGE_SPACES;
	}

	static BitField<TextServer::LineBreakFlag> get_autowrap_break_flags(const MarkdownLabel* p_label) {
		const TextServer::AutowrapMode mode = p_label == nullptr ? TextServer::AUTOWRAP_WORD_SMART : p_label->get_autowrap_mode();
		BitField<TextServer::LineBreakFlag> flags = TextServer::BREAK_MANDATORY;

		switch (mode) {
		case TextServer::AUTOWRAP_WORD_SMART:
			flags = TextServer::BREAK_WORD_BOUND | TextServer::BREAK_ADAPTIVE | TextServer::BREAK_MANDATORY;
			break;
		case TextServer::AUTOWRAP_WORD:
			flags = TextServer::BREAK_WORD_BOUND | TextServer::BREAK_MANDATORY;
			break;
		case TextServer::AUTOWRAP_ARBITRARY:
			flags = TextServer::BREAK_GRAPHEME_BOUND | TextServer::BREAK_MANDATORY;
			break;
		case TextServer::AUTOWRAP_OFF:
			break;
		}

		return flags | (p_label == nullptr ? (TextServer::BREAK_TRIM_START_EDGE_SPACES | TextServer::BREAK_TRIM_END_EDGE_SPACES) : p_label->get_autowrap_trim_flags());
	}

	static void configure_paragraph_autowrap(const Ref<TextParagraph>& p_paragraph, const MarkdownLabel* p_label) {
		if (p_paragraph.is_null()) {
			return;
		}

		p_paragraph->set_break_flags(get_autowrap_break_flags(p_label));
	}

	static Color get_span_color(const MarkdownInlineSpan& p_span, const MarkdownThemeCache& p_cache, const Color& p_default_color) {
		if (p_span.custom_color) {
			return p_span.color;
		}
		if (!p_span.link_uri.is_empty()) {
			return p_cache.color.link;
		}
		if (p_span.footnote_ref) {
			return p_cache.color.footnote_ref;
		}
		if (p_span.highlight) {
			return p_cache.color.highlight_font;
		}
		if (p_span.code) {
			return p_cache.color.code_text;
		}
		return p_default_color;
	}

	static MarkdownInlineSpan make_display_math_span(const MarkdownBlock& p_block) {
		MarkdownInlineSpan span;
		span.text = p_block.text;
		span.latex = p_block.argument;
		span.end = span.text.length();
		span.display_math = true;
		return span;
	}

	static bool add_formula_to_paragraph(const Ref<TextParagraph>& p_paragraph, const MarkdownInlineSpan& p_span, int32_t p_font_size, const Color& p_color, float p_max_width, MarkdownParagraphObjects& r_objects) {
		Ref<TeXRender> formula;
		formula.instantiate();
		const TeXRender::Style style = p_span.display_math ? TeXRender::STYLE_DISPLAY : TeXRender::STYLE_TEXT;
		if (formula->parse(p_span.latex, p_font_size, style, p_color) != OK || !formula->is_valid()) {
			return false;
		}
		formula->set_insets(Vector4i(), true);
		if (formula->get_width() > p_max_width) {
			formula->set_text_size(p_font_size * p_max_width / formula->get_width());
		}
		const Vector2 size = Vector2(formula->get_size()).max(Vector2(1.0f, 1.0f));
		const String key = String("latex_") + String::num_int64(r_objects.formulas.size());
		const InlineAlignment alignment = static_cast<InlineAlignment>(INLINE_ALIGNMENT_BASELINE_TO | INLINE_ALIGNMENT_TO_BASELINE);
		const int32_t object_length = static_cast<int32_t>(p_span.end - p_span.start);
		if (!p_paragraph->add_object(key, size, alignment, object_length, formula->get_baseline_offset())) {
			return false;
		}
		r_objects.formulas.insert(key, formula);
		return true;
	}

	static void add_spans_to_paragraph(const Ref<TextParagraph>& p_paragraph, const String& p_text, const std::vector<MarkdownInlineSpan>& p_spans, const MarkdownThemeCache& p_cache, const Ref<Font>& p_normal_font, int32_t p_font_size, const Color& p_default_color, MarkdownParagraphObjects& r_objects, float p_max_width, MarkdownLabelCanvas* p_canvas) {
		int64_t pos = 0;
		int32_t image_object_index = 0;
		for (const MarkdownInlineSpan& span : p_spans) {
			if (span.start > pos) {
				p_paragraph->add_string(p_text.substr(pos, span.start - pos), p_normal_font, std::max<int32_t>(1, p_font_size));
			}
			if (!span.latex.is_empty() && add_formula_to_paragraph(p_paragraph, span, p_font_size, get_span_color(span, p_cache, p_default_color), p_max_width, r_objects)) {
				pos = span.end;
				continue;
			}
			if (span.html_image || !span.image_uri.is_empty()) {
				const String image_uri = p_canvas->resolve_image_uri(span.image_uri, span.html_image);
				MarkdownImageCacheEntry image_entry = load_image_texture(image_uri, p_canvas);
				const bool image_ready = image_entry.status == MARKDOWN_IMAGE_READY;
				const bool image_failed = image_entry.status == MARKDOWN_IMAGE_FAILED;
				Ref<Texture2D> tex = image_ready ? image_entry.texture : (image_failed ? p_cache.icon.file_broken : Ref<Texture2D>());
				if (span.html_image && image_failed && span.text != String::chr(0xfffc)) {
					const Ref<Font> font = get_span_font(span, p_cache, p_normal_font);
					p_paragraph->add_string(span.text, font, std::max<int32_t>(1, p_font_size));
					pos = span.end;
					continue;
				}
				if (tex.is_valid() || image_entry.status == MARKDOWN_IMAGE_LOADING) {
					String name = String("img_") + String::num_int64(image_object_index++);
					if (tex.is_valid()) {
						r_objects.images[name] = tex;
					}
					int32_t object_length = std::max<int32_t>(1, static_cast<int32_t>(span.end - span.start));
					const int32_t viewport_height = p_canvas->get_layout_height();
					const float max_height = viewport_height > 0 ? viewport_height : std::numeric_limits<float>::max();
					const float placeholder_size = static_cast<float>(std::max<int32_t>(1, p_font_size));
					Vector2 object_size = image_ready ? fit_texture_size(tex, p_max_width, span.image_size, max_height) :
						Vector2(std::min(p_max_width, placeholder_size), std::min(max_height, placeholder_size));
					if (!image_ready) {
						if (!span.html_image) {
							object_length = 1;
						}
					}
					p_paragraph->add_object(name, object_size, INLINE_ALIGNMENT_CENTER, object_length);
					const String object_placeholder = String::chr(0xfffc);
					if (image_failed && !span.text.is_empty() && span.text != object_placeholder) {
						MarkdownInlineSpan fallback_span = span;
						fallback_span.italic = true;
						const Ref<Font> font = get_span_font(fallback_span, p_cache, p_normal_font);
						p_paragraph->add_string(" " + span.text, font, std::max<int32_t>(1, p_font_size));
					}
				}
				else if (image_entry.status == MARKDOWN_IMAGE_FAILED && !span.text.is_empty() && span.text != String::chr(0xfffc)) {
					MarkdownInlineSpan fallback_span = span;
					fallback_span.italic = true;
					const Ref<Font> font = get_span_font(fallback_span, p_cache, p_normal_font);
					p_paragraph->add_string(span.text, font, std::max<int32_t>(1, p_font_size));
				}
			}
			else {
				const Ref<Font> font = get_span_font(span, p_cache, p_normal_font);
				const int32_t font_size = span.footnote_ref ? p_cache.font_size.footnote_ref : (span.code ? p_cache.font_size.code : p_font_size);
				const Ref<Font> actual_font = span.footnote_ref && p_cache.font.footnote_ref.is_valid() ? p_cache.font.footnote_ref : font;
				p_paragraph->add_string(span.text, actual_font, std::max<int32_t>(1, font_size));
			}
			pos = span.end;
		}
		if (pos < static_cast<int64_t>(p_text.length())) {
			p_paragraph->add_string(p_text.substr(pos), p_normal_font, std::max<int32_t>(1, p_font_size));
		}
	}

	static Vector2 get_inline_code_vertical_padding(const std::vector<MarkdownInlineSpan>& p_spans, const MarkdownThemeCache& p_cache) {
		bool has_code_span = false;
		for (const MarkdownInlineSpan& span : p_spans) {
			if (span.code) {
				has_code_span = true;
				break;
			}
		}
		if (!has_code_span) {
			return Vector2();
		}

		const float margin_top = get_stylebox_margin_or(p_cache.stylebox.inline_code, SIDE_TOP, 1.0f);
		const float margin_bottom = get_stylebox_margin_or(p_cache.stylebox.inline_code, SIDE_BOTTOM, 1.0f);
		return Vector2(std::max<float>(0.0f, margin_top), std::max<float>(0.0f, margin_bottom));
	}

	static bool is_search_word_character(char32_t p_character) {
		return (p_character >= 'a' && p_character <= 'z') || (p_character >= 'A' && p_character <= 'Z') || (p_character >= '0' && p_character <= '9') || p_character == '_';
	}
	static bool is_whole_word_match(const String& p_text, int64_t p_start, int64_t p_length) {
		if (p_start > 0 && is_search_word_character(p_text[p_start - 1])) {
			return false;
		}

		const int64_t end = p_start + p_length;
		if (end < p_text.length() && is_search_word_character(p_text[end])) {
			return false;
		}

		return true;
	}

	static const char* block_spacing_type(const MarkdownBlock& p_block) {
		switch (p_block.type) {
		case MARKDOWN_BLOCK_HEADING:
			return "heading";
		case MARKDOWN_BLOCK_CODE:
			return "code_block";
		case MARKDOWN_BLOCK_TABLE:
			return "table";
		case MARKDOWN_BLOCK_FOOTNOTES:
			return "footnotes";
		case MARKDOWN_BLOCK_BLOCKQUOTE:
			return "blockquote";
		case MARKDOWN_BLOCK_TASK_LIST:
		case MARKDOWN_BLOCK_UNORDERED_LIST:
		case MARKDOWN_BLOCK_ORDERED_LIST:
			return "list";
		default:
			return "text";
		}
	}

} // !namespace

void MarkdownThemeCache::build(Control* p_owner, int32_t p_extra_font_size) {
	base_scale = get_theme_base_scale_or_one(p_owner);
	const int32_t default_font_size = std::max<int32_t>(1, p_owner == nullptr ? scale_theme_fallback(16, base_scale) : p_owner->get_theme_default_font_size());
	const auto scale_font_size_from_text = [&](int32_t p_base_font_size) -> int32_t {
		return std::max<int32_t>(1, static_cast<int32_t>(Math::round(static_cast<double>(default_font_size) * static_cast<double>(p_base_font_size) / 16.0)));
		};

	// Font sizes
	font_size.text = std::max<int32_t>(1, get_theme_font_size_or(p_owner, "MarkdownLabel", "text_font_size", default_font_size) + p_extra_font_size);
	font_size.code = std::max<int32_t>(1, get_theme_font_size_or(p_owner, "MarkdownLabel", "code_text_font_size", default_font_size) + p_extra_font_size);
	font_size.code_block = std::max<int32_t>(1, get_theme_font_size_or(p_owner, "MarkdownLabel", "code_block_font_size", default_font_size) + p_extra_font_size);
	font_size.list_marker = std::max<int32_t>(1, get_theme_font_size_or(p_owner, "MarkdownLabel", "list_marker_font_size", default_font_size) + p_extra_font_size);
	font_size.table_header = std::max<int32_t>(1, get_theme_font_size_or(p_owner, "MarkdownLabel", "table_header_font_size", default_font_size) + p_extra_font_size);
	font_size.table_cell = std::max<int32_t>(1, get_theme_font_size_or(p_owner, "MarkdownLabel", "table_cell_font_size", default_font_size) + p_extra_font_size);
	font_size.footnote_text = std::max<int32_t>(1, get_theme_font_size_or(p_owner, "MarkdownLabel", "footnote_text_font_size", font_size.text) + p_extra_font_size);
	font_size.footnote_ref = std::max<int32_t>(1, get_theme_font_size_or(p_owner, "MarkdownLabel", "footnote_ref_font_size", scale_font_size_from_text(12)) + p_extra_font_size);

	static const int32_t default_heading_sizes[] = { 36, 26, 20, 18, 12, 10 };
	for (int32_t i = 0; i < 6; i++) {
		const String name = String("h") + String::num_int64(i + 1) + "_font_size";
		font_size.heading[i] = std::max<int32_t>(1, get_theme_font_size_or(p_owner, "MarkdownLabel", StringName(name), scale_font_size_from_text(default_heading_sizes[i])) + p_extra_font_size);
	}

	// Constants
	constant.line_separation = get_scaled_theme_constant_or(p_owner, "MarkdownLabel", "line_separation", 2, base_scale);
	constant.paragraph_separation = get_scaled_theme_constant_or(p_owner, "MarkdownLabel", "paragraph_separation", 10, base_scale);
	constant.list_indent = std::max<int32_t>(0, get_scaled_theme_constant_or(p_owner, "MarkdownLabel", "list_indent", 26, base_scale));
	constant.list_marker_gap = std::max<int32_t>(0, get_scaled_theme_constant_or(p_owner, "MarkdownLabel", "list_marker_gap", 6, base_scale));
	constant.table_striped = std::max<int32_t>(0, get_theme_constant_or(p_owner, "MarkdownLabel", "table_striped", 1));
	constant.footnote_space_before = std::max<int32_t>(0, get_scaled_theme_constant_or(p_owner, "MarkdownLabel", "footnote_space_before", 16, base_scale));
	constant.footnote_line_separation = std::max<int32_t>(0, get_theme_constant_or(p_owner, "MarkdownLabel", "footnote_line_separation", constant.line_separation));

	// Fonts
	font.text = get_theme_font_or(p_owner, "MarkdownLabel", "text_font");
	font.bold = get_theme_font_or(p_owner, "MarkdownLabel", "bold_font");
	font.bold_italic = get_theme_font_or(p_owner, "MarkdownLabel", "bold_italic_font");
	if (!font.bold_italic.is_valid()) {
		font.bold_italic = font.text;
	}
	font.italic = get_theme_font_or(p_owner, "MarkdownLabel", "italic_font");
	if (!font.italic.is_valid()) {
		font.italic = font.text;
	}
	font.code = get_theme_font_or(p_owner, "MarkdownLabel", "code_text_font");
	if (!font.code.is_valid()) {
		font.code = font.text;
	}
	font.code_block = get_theme_font_or(p_owner, "MarkdownLabel", "code_block_font");
	if (!font.code_block.is_valid()) {
		font.code_block = font.code;
	}
	font.list_marker = get_theme_font_or(p_owner, "MarkdownLabel", "list_marker_font");
	if (!font.list_marker.is_valid()) {
		font.list_marker = font.text;
	}
	font.table_header = get_theme_font_or(p_owner, "MarkdownLabel", "table_header_font");
	if (!font.table_header.is_valid()) {
		font.table_header = font.text;
	}
	font.table_cell = get_theme_font_or(p_owner, "MarkdownLabel", "table_cell_font");
	if (!font.table_cell.is_valid()) {
		font.table_cell = font.text;
	}
	font.footnote_ref = get_theme_font_or(p_owner, "MarkdownLabel", "footnote_ref_font");
	if (!font.footnote_ref.is_valid()) {
		font.footnote_ref = font.text;
	}
	font.footnote_text = get_theme_font_or(p_owner, "MarkdownLabel", "footnote_text_font");
	if (!font.footnote_text.is_valid()) {
		font.footnote_text = font.text;
	}

	for (int32_t i = 0; i < 6; i++) {
		const String name = String("h") + String::num_int64(i + 1) + "_font";
		font.heading[i] = get_theme_font_or(p_owner, "MarkdownLabel", StringName(name));
		if (!font.heading[i].is_valid()) {
			font.heading[i] = font.text;
		}
	}

	// Colors
	color.text = get_theme_color_or(p_owner, "MarkdownLabel", "text_font_color", Color(1.0f, 1.0f, 1.0f, 1.0f));
	color.link = get_theme_color_or(p_owner, "MarkdownLabel", "link_color", Color(0.36f, 0.62f, 1.0f, 1.0f));
	color.selection_color = get_theme_color_or(p_owner, "MarkdownLabel", "selection_color", Color(0.102f, 0.102f, 1.0f, 0.8f));
	stylebox.selection = get_theme_stylebox_or(p_owner, "MarkdownLabel", "selection_stylebox");
	if (!stylebox.selection.is_valid()) {
		stylebox.selection = create_internal_stylebox(color.selection_color, Color(0, 0, 0, 0), 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0, true, 8, base_scale);
	}
	color.code_text = get_theme_color_or(p_owner, "MarkdownLabel", "code_text_font_color", Color(1.0f, 1.0f, 1.0f, 1.0f));
	color.code_block = get_theme_color_or(p_owner, "MarkdownLabel", "code_block_font_color", Color(1.0f, 1.0f, 1.0f, 1.0f));
	color.code_block_background = get_theme_color_or(p_owner, "MarkdownLabel", "code_block_background_color", Color(0.08f, 0.09f, 0.11f, 1.0f));
	color.highlight = get_theme_color_or(p_owner, "MarkdownLabel", "highlight_color", Color(1.0f, 0.92f, 0.35f, 0.5f));
	color.highlight_font = get_theme_color_or(p_owner, "MarkdownLabel", "highlight_font_color", Color(1.0f, 0.700f, 0.211f, 1.0f));
	color.strikethrough = get_theme_color_or(p_owner, "MarkdownLabel", "strikethrough_color", Color(1.0f, 1.0f, 1.0f, 1.0f));
	color.list_marker = get_theme_color_or(p_owner, "MarkdownLabel", "list_marker_color", color.text);
	color.table_header_font = get_theme_color_or(p_owner, "MarkdownLabel", "table_header_font_color", color.text);
	color.table_cell_font = get_theme_color_or(p_owner, "MarkdownLabel", "table_cell_font_color", color.text);
	color.footnote_ref = get_theme_color_or(p_owner, "MarkdownLabel", "footnote_ref_font_color", Color(0.809f, 0.809f, 0.809f, 1.0f));
	color.footnote_text = get_theme_color_or(p_owner, "MarkdownLabel", "footnote_text_font_color", Color(0.875f, 0.875f, 0.875f, 0.502f));

	for (int32_t i = 0; i < 6; i++) {
		const String name = String("h") + String::num_int64(i + 1) + "_font_color";
		color.heading[i] = get_theme_color_or(p_owner, "MarkdownLabel", StringName(name), color.text);
	}

	// StyleBoxes
	stylebox.inline_code = get_theme_stylebox_or(p_owner, "MarkdownLabel", "inline_code");
	if (!stylebox.inline_code.is_valid()) {
		stylebox.inline_code = create_internal_stylebox(Color(0.6f, 0.6f, 0.6f, 0.392f), Color(0, 0, 0, 0), 0, 0, 0, 0, 4.0f, 0.0f, 4.0f, 0.0f, 2, true, 8, base_scale);
	}

	Ref<StyleBoxEmpty> empty;
	empty.instantiate();
	stylebox.footnote_ref = empty;

	stylebox.code_block_panel = get_theme_stylebox_or(p_owner, "MarkdownLabel", "code_block_panel");
	if (!stylebox.code_block_panel.is_valid()) {
		stylebox.code_block_panel = create_internal_stylebox(Color(0.0f, 0.0f, 0.0f, 0.6f), Color(0, 0, 0, 0), 0, 0, 0, 0, 8.0f, 8.0f, 8.0f, 8.0f, 3, true, 8, base_scale);
	}

	stylebox.blockquote = get_theme_stylebox_or(p_owner, "MarkdownLabel", "blockquote_panel");
	if (!stylebox.blockquote.is_valid()) {
		stylebox.blockquote = create_internal_stylebox(Color(0.0f, 0.0f, 0.0f, 0.196f), Color(0.458f, 0.458f, 0.458f, 1.0f), 2, 0, 0, 0, 16.0f, 16.0f, 8.0f, 16.0f, 0, false, 8, base_scale);
	}

	stylebox.blockquote_nested = get_theme_stylebox_or(p_owner, "MarkdownLabel", "blockquote_nested");
	if (!stylebox.blockquote_nested.is_valid()) {
		stylebox.blockquote_nested = stylebox.blockquote;
	}

	stylebox.search_result = get_theme_stylebox_or(p_owner, "MarkdownLabel", "search_result");
	if (!stylebox.search_result.is_valid()) {
		stylebox.search_result = create_internal_stylebox(Color(1.0f, 0.72f, 0.16f, 0.42f), Color(0, 0, 0, 0), 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0, true, 8, base_scale);
	}

	stylebox.highlight = get_theme_stylebox_or(p_owner, "MarkdownLabel", "highlight");
	if (!stylebox.highlight.is_valid()) {
		stylebox.highlight = create_internal_stylebox(Color(0.6f, 0.6f, 0.6f, 0.392f), Color(0, 0, 0, 0), 0, 0, 0, 0, 4.0f, 0.0f, 4.0f, 0.0f, 2, true, 8, base_scale);
	}

	stylebox.separator = get_theme_stylebox_or(p_owner, "MarkdownLabel", "separator");
	if (!stylebox.separator.is_valid()) {
		stylebox.separator = create_internal_stylebox(Color(0.537f, 0.537f, 0.537f, 1.0f), Color(0, 0, 0, 0), 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0, true, 8, base_scale);
	}

	stylebox.table_panel = get_theme_stylebox_or(p_owner, "MarkdownLabel", "table_panel");
	if (!stylebox.table_panel.is_valid()) {
		stylebox.table_panel = create_internal_stylebox(Color(0.0f, 0.0f, 0.0f, 0.6f), Color(0, 0, 0, 0), 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 3, true, 8, base_scale);
	}

	stylebox.table_header_panel = get_theme_stylebox_or(p_owner, "MarkdownLabel", "table_header_panel");
	if (!stylebox.table_header_panel.is_valid()) {
		stylebox.table_header_panel = create_internal_stylebox(Color(1.0f, 1.0f, 1.0f, 0.196f), Color(0, 0, 0, 0), 0, 0, 0, 1, 4.0f, 4.0f, 4.0f, 4.0f, 0, false, 8, base_scale);
	}

	stylebox.table_cell_panel = get_theme_stylebox_or(p_owner, "MarkdownLabel", "table_cell_panel");
	if (!stylebox.table_cell_panel.is_valid()) {
		stylebox.table_cell_panel = create_internal_stylebox(Color(1.0f, 1.0f, 1.0f, 0.196f), Color(0, 0, 0, 0), 0, 0, 0, 0, 4.0f, 4.0f, 4.0f, 4.0f, 0, false, 8, base_scale);
	}

	stylebox.table_striped_panel = get_theme_stylebox_or(p_owner, "MarkdownLabel", "table_striped_panel");
	if (!stylebox.table_striped_panel.is_valid()) {
		stylebox.table_striped_panel = create_internal_stylebox(Color(1.0f, 1.0f, 1.0f, 0.024f), Color(0, 0, 0, 0), 0, 0, 0, 0, 4.0f, 4.0f, 4.0f, 4.0f, 0, true, 8, base_scale);
	}

	// Texture
	icon.task_checked = get_theme_icon_or(p_owner, "MarkdownLabel", "task_checked");
	if (!icon.task_checked.is_valid()) {
		icon.task_checked = ResourceLoader::get_singleton()->load("uid://diq14yv8brh7m");
	}
	icon.task_unchecked = get_theme_icon_or(p_owner, "MarkdownLabel", "task_unchecked");
	if (!icon.task_unchecked.is_valid()) {
		icon.task_unchecked = ResourceLoader::get_singleton()->load("uid://wgx35yuv1sh3");
	}
	icon.file_broken = get_theme_icon_or(p_owner, "MarkdownLabel", "file_broken");
	if (!icon.file_broken.is_valid()) {
		icon.file_broken = ResourceLoader::get_singleton()->load("uid://cn51pvobw1gxd");
	}

	apply_editor_light_theme(*this, get_editor_light_theme_info(p_owner));
}

// MarkdownLabelCanvas
MarkdownLabelCanvas::MarkdownLabelCanvas() {
	s_live_image_canvas_count++;
}

MarkdownLabelCanvas::~MarkdownLabelCanvas() {
	s_live_image_canvas_count = std::max<int32_t>(0, s_live_image_canvas_count - 1);
	if (s_live_image_canvas_count == 0) {
		s_image_watchers.clear();
		s_pending_local_image_uris.clear();
		s_image_cache.clear();
	}
}

void MarkdownLabelCanvas::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_markdown_text", "text"), &MarkdownLabelCanvas::set_markdown_text);
	ClassDB::bind_method(D_METHOD("append_text_incremental", "delta"), &MarkdownLabelCanvas::append_text_incremental);
	ClassDB::bind_method(D_METHOD("finish_stream"), &MarkdownLabelCanvas::finish_stream);
	ClassDB::bind_method(D_METHOD("clear_document"), &MarkdownLabelCanvas::clear_document);
	ClassDB::bind_method(D_METHOD("set_streaming_enabled", "enabled"), &MarkdownLabelCanvas::set_streaming_enabled);
	ClassDB::bind_method(D_METHOD("set_max_unstable_lines", "lines"), &MarkdownLabelCanvas::set_max_unstable_lines);
	ClassDB::bind_method(D_METHOD("set_deferred_layout_enabled", "enabled"), &MarkdownLabelCanvas::set_deferred_layout_enabled);
	ClassDB::bind_method(D_METHOD("is_deferred_layout_enabled"), &MarkdownLabelCanvas::is_deferred_layout_enabled);
	ClassDB::bind_method(D_METHOD("set_layout_flush_interval_msec", "msec"), &MarkdownLabelCanvas::set_layout_flush_interval_msec);
	ClassDB::bind_method(D_METHOD("get_layout_flush_interval_msec"), &MarkdownLabelCanvas::get_layout_flush_interval_msec);
	ClassDB::bind_method(D_METHOD("flush_pending_layout"), &MarkdownLabelCanvas::flush_pending_layout);
	ClassDB::bind_method(D_METHOD("select_all"), &MarkdownLabelCanvas::select_all);
	ClassDB::bind_method(D_METHOD("select_between_points", "from", "to"), &MarkdownLabelCanvas::select_between_points);
	ClassDB::bind_method(D_METHOD("get_link_uri_at_point", "position"), &MarkdownLabelCanvas::get_link_uri_at_point);
	ClassDB::bind_method(D_METHOD("clear_selection"), &MarkdownLabelCanvas::clear_selection);
	ClassDB::bind_method(D_METHOD("search", "text", "flags", "from_line", "from_column"), &MarkdownLabelCanvas::search);
	ClassDB::bind_method(D_METHOD("get_selected_text"), &MarkdownLabelCanvas::get_selected_text);
	ClassDB::bind_method(D_METHOD("get_rendered_block_count"), &MarkdownLabelCanvas::get_rendered_block_count);
	ClassDB::bind_method(D_METHOD("get_content_height"), &MarkdownLabelCanvas::get_content_height);
	ClassDB::bind_method(D_METHOD("has_anchor", "anchor"), &MarkdownLabelCanvas::has_anchor);
	ClassDB::bind_method(D_METHOD("get_anchors"), &MarkdownLabelCanvas::get_anchors);
	ClassDB::bind_method(D_METHOD("get_anchor_offset", "anchor"), &MarkdownLabelCanvas::get_anchor_offset);
	ClassDB::bind_method(D_METHOD("get_search_result_y"), &MarkdownLabelCanvas::get_search_result_y);
	ClassDB::bind_method(D_METHOD("get_bottom_position"), &MarkdownLabelCanvas::get_bottom_position);
}

void MarkdownLabelCanvas::set_label(MarkdownLabel* p_label) {
	label = p_label;
	mark_layout_dirty();
}

void MarkdownLabelCanvas::set_markdown_text(const String& p_text) {
	raw_text = p_text;
	selection_anchor = 0;
	selection_caret = 0;
	search_match_start = -1;
	search_match_end = -1;
	dragging_selection = false;
	selection_dragged = false;
	selection_drag_attempt = false;
	parser_state.reset();
	cached_blocks.clear();
	// A streaming append before the next redraw must start with a fresh layout.
	items.clear();
	pending_stream_parse = false;
	pending_layout_elapsed_msec = 0.0;
	layout_prefix_reuse_requested = false;
	internal_processing_enabled = false;
	set_process_internal(false);
	update_internal_processing();
	mark_layout_dirty();
}

String MarkdownLabelCanvas::resolve_image_uri(const String& p_uri, bool p_html) const {
	if (p_html) {
		const String path = docrender::resolve_html_image_path(p_uri, label ? label->get_html_base_dir() : String("res://"));
		return !path.is_empty() && ResourceLoader::get_singleton()->exists(path, "Texture2D") ? path : String();
	}
	if (label == nullptr || label->image_base_dir.is_empty() || p_uri.is_absolute_path() || p_uri.contains(":")) {
		return p_uri;
	}
	return label->image_base_dir.path_join(p_uri).simplify_path();
}

int32_t MarkdownLabelCanvas::get_layout_height() const {
	return layout_height;
}

MarkdownParseOptions MarkdownLabelCanvas::get_parse_options() const {
	return label != nullptr ? MarkdownParseOptions{ label->is_parsing_latex(), label->is_parsing_gfm(), label->is_parsing_html() } : MarkdownParseOptions();
}

void MarkdownLabelCanvas::parse_current_text_incremental() {
	const MarkdownParseOptions options = get_parse_options();
	if (streaming_enabled) {
		std::vector<String> all_lines = split_lines(raw_text);
		int32_t reparse_line = find_reparse_line(cached_blocks, all_lines, parser_state, max_unstable_lines);
		if (options.parse_html && !cached_blocks.empty()) {
			// The append can complete the previous last line and add more lines in the same chunk.
			reparse_line = std::min(reparse_line, std::max(0, cached_blocks.back().line - 1));
		}
		rebuild_tail(raw_text, reparse_line, cached_blocks, parser_state, options);
		layout_prefix_reuse_requested = true;
	}
	else {
		parser_state.reset();
		MarkdownParseResult result = parse_markdown_document(raw_text, nullptr, INT_MAX, options);
		cached_blocks = result.blocks;
		parser_state = result.state;
		layout_prefix_reuse_requested = false;
	}
}

void MarkdownLabelCanvas::append_text_incremental(const String& p_delta) {
	raw_text += p_delta;

	if (deferred_layout_enabled && streaming_enabled) {
		pending_stream_parse = true;
		update_internal_processing();
		return;
	}

	parse_current_text_incremental();

	mark_layout_dirty();
}

void MarkdownLabelCanvas::finish_stream() {
	pending_stream_parse = false;
	pending_layout_elapsed_msec = 0.0;
	parser_state.reset();
	MarkdownParseResult result = parse_markdown_document(raw_text, nullptr, INT_MAX, get_parse_options());
	cached_blocks = result.blocks;
	parser_state = result.state;
	layout_prefix_reuse_requested = false;
	mark_layout_dirty();
}

void MarkdownLabelCanvas::clear_document() {
	raw_text = String();
	plain_text = String();
	items.clear();
	lines.clear();
	cached_blocks.clear();
	parser_state.reset();
	mermaid_cache.update({}, {});
	pending_stream_parse = false;
	pending_layout_elapsed_msec = 0.0;
	layout_prefix_reuse_requested = false;
	rendered_block_count = 0;
	selection_anchor = 0;
	selection_caret = 0;
	search_match_start = -1;
	search_match_end = -1;
	dragging_selection = false;
	selection_dragged = false;
	selection_drag_attempt = false;
	internal_processing_enabled = false;
	set_process_internal(false);
	content_height = 1.0f;
	layout_dirty = false;
	anchor_offsets.clear();
	set_custom_minimum_size(Vector2(1.0, content_height));
	update_minimum_size();
	queue_redraw();
	update_internal_processing();
}

void MarkdownLabelCanvas::set_streaming_enabled(bool p_enabled) {
	streaming_enabled = p_enabled;
}

void MarkdownLabelCanvas::set_max_unstable_lines(int32_t p_lines) {
	max_unstable_lines = std::max<int32_t>(1, std::min<int32_t>(p_lines, 256));
}

void MarkdownLabelCanvas::set_deferred_layout_enabled(bool p_enabled) {
	if (deferred_layout_enabled == p_enabled) {
		return;
	}

	if (!p_enabled) {
		flush_pending_layout();
	}
	deferred_layout_enabled = p_enabled;
	update_internal_processing();
}

bool MarkdownLabelCanvas::is_deferred_layout_enabled() const {
	return deferred_layout_enabled;
}

void MarkdownLabelCanvas::set_layout_flush_interval_msec(int32_t p_msec) {
	layout_flush_interval_msec = std::max<int32_t>(1, std::min<int32_t>(p_msec, 1000));
}

int32_t MarkdownLabelCanvas::get_layout_flush_interval_msec() const {
	return layout_flush_interval_msec;
}

bool MarkdownLabelCanvas::flush_pending_layout() {
	if (!pending_stream_parse) {
		return false;
	}

	pending_stream_parse = false;
	pending_layout_elapsed_msec = 0.0;
	parse_current_text_incremental();
	mark_layout_dirty();
	update_internal_processing();
	if (label != nullptr) {
		label->notify_canvas_layout_changed();
	}
	return true;
}

void MarkdownLabelCanvas::request_local_image(const String& p_uri) {
	if (p_uri.is_empty() || !s_image_cache.has(p_uri) || s_image_cache[p_uri].status != MARKDOWN_IMAGE_LOADING) {
		return;
	}

	if (has_pending_local_image_uri(p_uri)) {
		update_internal_processing();
		return;
	}

	ResourceLoader* loader = ResourceLoader::get_singleton();
	if (loader == nullptr) {
		MarkdownImageCacheEntry entry;
		entry.status = MARKDOWN_IMAGE_FAILED;
		entry.error = "resource_loader_missing";
		s_image_cache[p_uri] = entry;
		notify_image_watchers(p_uri);
		return;
	}

	const Error error = loader->load_threaded_request(p_uri, "Texture2D", true);
	if (error != OK) {
		MarkdownImageCacheEntry entry;
		entry.status = MARKDOWN_IMAGE_FAILED;
		entry.error = "threaded_request_failed";
		s_image_cache[p_uri] = entry;
		notify_image_watchers(p_uri);
		return;
	}

	add_pending_local_image_uri(p_uri);
	update_internal_processing();
}

void MarkdownLabelCanvas::request_remote_image(const String& p_uri) {
	if (p_uri.is_empty() || !s_image_cache.has(p_uri) || s_image_cache[p_uri].status != MARKDOWN_IMAGE_LOADING) {
		return;
	}

	HTTPRequest* request = memnew(HTTPRequest);
	request->set_name("_MarkdownImageRequest");
	request->set_use_threads(true);
	request->set_timeout(12.0);
	request->set_body_size_limit(8 * 1024 * 1024);
	request->set_max_redirects(8);
	const uint64_t request_id = request->get_instance_id();
	request->connect("request_completed", callable_mp(this, &MarkdownLabelCanvas::on_image_request_completed).bind(p_uri, request_id));
	add_child(request, false, Node::INTERNAL_MODE_BACK);

	const Error error = request->request(p_uri);
	if (error != OK) {
		MarkdownImageCacheEntry entry;
		entry.status = MARKDOWN_IMAGE_FAILED;
		entry.error = "request_start_failed";
		s_image_cache[p_uri] = entry;
		request->queue_free();
		notify_image_watchers(p_uri);
	}
	update_internal_processing();
}

void MarkdownLabelCanvas::on_image_request_completed(int32_t p_result, int32_t p_response_code, const PackedStringArray& p_headers, const PackedByteArray& p_body, const String& p_uri, uint64_t p_request_id) {
	(void)p_headers;

	MarkdownImageCacheEntry entry;
	if (p_result == HTTPRequest::RESULT_SUCCESS && p_response_code >= 200 && p_response_code < 300) {
		Ref<Texture2D> texture = decode_image_texture_from_buffer(p_uri, p_body);
		if (texture.is_valid()) {
			entry.status = MARKDOWN_IMAGE_READY;
			entry.texture = texture;
		}
		else {
			entry.status = MARKDOWN_IMAGE_FAILED;
			entry.error = "decode_failed";
		}
	}
	else {
		entry.status = MARKDOWN_IMAGE_FAILED;
		entry.error = "request_failed";
	}

	s_image_cache[p_uri] = entry;
	Object* request_object = ObjectDB::get_instance(p_request_id);
	HTTPRequest* request = Object::cast_to<HTTPRequest>(request_object);
	if (request != nullptr) {
		request->queue_free();
	}
	notify_image_watchers(p_uri);
	update_internal_processing();
}

void MarkdownLabelCanvas::refresh_image_content() {
	mark_layout_dirty();
}

bool MarkdownLabelCanvas::process_pending_local_images() {
	if (s_pending_local_image_uris.empty()) {
		return false;
	}

	ResourceLoader* loader = ResourceLoader::get_singleton();
	if (loader == nullptr) {
		return false;
	}

	std::vector<String> remaining;
	for (const String& uri : s_pending_local_image_uris) {
		if (!s_image_cache.has(uri) || s_image_cache[uri].status != MARKDOWN_IMAGE_LOADING) {
			continue;
		}

		const ResourceLoader::ThreadLoadStatus status = loader->load_threaded_get_status(uri);
		if (status == ResourceLoader::THREAD_LOAD_IN_PROGRESS) {
			remaining.push_back(uri);
			continue;
		}

		MarkdownImageCacheEntry entry;
		if (status == ResourceLoader::THREAD_LOAD_LOADED) {
			Ref<Resource> resource = loader->load_threaded_get(uri);
			Ref<Texture2D> texture = resource;
			if (texture.is_valid()) {
				entry.status = MARKDOWN_IMAGE_READY;
				entry.texture = texture;
			}
			else {
				entry.status = MARKDOWN_IMAGE_FAILED;
				entry.error = "resource_not_texture";
			}
		}
		else {
			entry.status = MARKDOWN_IMAGE_FAILED;
			entry.error = "threaded_load_failed";
		}

		s_image_cache[uri] = entry;
		notify_image_watchers(uri);
	}

	s_pending_local_image_uris = remaining;
	return !s_pending_local_image_uris.empty();
}

bool MarkdownLabelCanvas::update_gif_animations(double p_delta) {
	bool has_animated_gif = false;
	for (const KeyValue<String, MarkdownImageCacheEntry>& key_value : s_image_cache) {
		if (key_value.value.status != MARKDOWN_IMAGE_READY || !is_animated_gif_texture(key_value.value.texture)) {
			continue;
		}

		Object* texture_object = key_value.value.texture.ptr();
		if (texture_object == nullptr) {
			continue;
		}

		const int32_t frame_count = static_cast<int32_t>(texture_object->call("get_frame_count"));
		if (frame_count <= 1) {
			continue;
		}

		has_animated_gif = true;
		MarkdownImageCacheEntry& entry = s_image_cache[key_value.key];
		const int32_t frame = static_cast<int32_t>(texture_object->call("get_frame"));
		const double delay = std::max<double>(0.01, static_cast<double>(texture_object->call("get_frame_delay", frame)));
		entry.gif_elapsed += p_delta;
		if (entry.gif_elapsed >= delay) {
			entry.gif_elapsed = 0.0;
			texture_object->call("set_frame", (frame + 1) % frame_count);
			queue_redraw();
		}
	}

	return has_animated_gif;
}

void MarkdownLabelCanvas::update_internal_processing() {
	const bool should_process = pending_stream_parse || mermaid_cache.has_pending() || !s_pending_local_image_uris.empty() || dragging_selection || update_gif_animations(0.0);
	if (internal_processing_enabled != should_process) {
		internal_processing_enabled = should_process;
		set_process_internal(should_process);
	}
}

void MarkdownLabelCanvas::mark_layout_dirty() {
	layout_dirty = true;
	queue_redraw();
}

bool MarkdownLabelCanvas::has_selection() const {
	return selection_anchor != selection_caret;
}

bool MarkdownLabelCanvas::is_character_selected(int64_t p_character) const {
	const int64_t from = std::min<int64_t>(selection_anchor, selection_caret);
	const int64_t to = std::max<int64_t>(selection_anchor, selection_caret);
	return from < to && p_character >= from && p_character <= to;
}

bool MarkdownLabelCanvas::is_position_inside_selection(const Vector2& p_position) {
	if (!has_selection()) {
		return false;
	}
	flush_pending_layout();
	rebuild_layout();
	return is_character_selected(hit_test_document(p_position));
}

void MarkdownLabelCanvas::select_all() {
	flush_pending_layout();
	rebuild_layout();
	selection_anchor = 0;
	selection_caret = plain_text.length();
	queue_redraw();
}

void MarkdownLabelCanvas::select_between_points(const Vector2& p_from, const Vector2& p_to) {
	flush_pending_layout();
	rebuild_layout();
	selection_anchor = hit_test_document(p_from);
	selection_caret = hit_test_document(p_to);
	queue_redraw();
}

String MarkdownLabelCanvas::get_link_uri_at_point(const Vector2& p_position) {
	flush_pending_layout();
	rebuild_layout();
	return link_uri_at_position(p_position);
}


void MarkdownLabelCanvas::clear_selection() {
	selection_drag_attempt = false;
	selection_anchor = selection_caret;
	queue_redraw();
}

Vector2i MarkdownLabelCanvas::search(const String& p_text, int32_t p_flags, int32_t p_from_line, int32_t p_from_column) {
	flush_pending_layout();
	rebuild_layout();

	const String needle = p_text;
	if (needle.is_empty() || plain_text.is_empty()) {
		search_match_start = -1;
		search_match_end = -1;
		queue_redraw();
		return Vector2i(-1, -1);
	}

	const bool match_case = (p_flags & 1) != 0;
	const bool whole_words = (p_flags & 2) != 0;
	const bool backwards = (p_flags & 4) != 0;
	const String haystack = match_case ? plain_text : plain_text.to_lower();
	const String search_needle = match_case ? needle : needle.to_lower();
	const int64_t needle_length = search_needle.length();
	int64_t start_offset = line_column_to_offset(p_from_line, p_from_column);

	if (backwards) {
		int64_t index = std::min<int64_t>(start_offset, std::max<int64_t>(0, haystack.length() - needle_length));
		while (index >= 0) {
			if (haystack.substr(static_cast<int32_t>(index), static_cast<int32_t>(needle_length)) == search_needle && (!whole_words || is_whole_word_match(plain_text, index, needle_length))) {
				search_match_start = index;
				search_match_end = index + needle_length;
				queue_redraw();
				return offset_to_line_column(index);
			}
			index--;
		}
	}
	else {
		int64_t index = std::max<int64_t>(0, std::min<int64_t>(start_offset, haystack.length()));
		while (index <= haystack.length() - needle_length) {
			const int64_t found = haystack.find(search_needle, static_cast<int32_t>(index));
			if (found < 0) {
				break;
			}
			if (!whole_words || is_whole_word_match(plain_text, found, needle_length)) {
				search_match_start = found;
				search_match_end = found + needle_length;
				queue_redraw();
				return offset_to_line_column(found);
			}
			index = found + 1;
		}
	}

	search_match_start = -1;
	search_match_end = -1;
	queue_redraw();
	return Vector2i(-1, -1);
}

String MarkdownLabelCanvas::get_selected_text() const {
	const_cast<MarkdownLabelCanvas*>(this)->flush_pending_layout();
	const_cast<MarkdownLabelCanvas*>(this)->rebuild_layout();
	const int64_t from = std::min<int64_t>(selection_anchor, selection_caret);
	const int64_t to = std::max<int64_t>(selection_anchor, selection_caret);
	if (from >= to || from < 0 || to > plain_text.length()) {
		return String();
	}

	return plain_text.substr(static_cast<int32_t>(from), static_cast<int32_t>(to - from));
}

Variant MarkdownLabelCanvas::_get_drag_data(const Vector2& p_at_position) {
	Variant drag_data = Control::_get_drag_data(p_at_position);
	if (drag_data.get_type() != Variant::NIL) {
		return drag_data;
	}

	if (label == nullptr || !label->is_drag_and_drop_selection_enabled() || !selection_drag_attempt) {
		return Variant();
	}

	const String selected_text = get_selected_text();
	if (selected_text.is_empty()) {
		return Variant();
	}

	Label* preview = memnew(Label);
	preview->set_text(selected_text);
	preview->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	preview->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
	set_drag_preview(preview);
	return selected_text;
}

void MarkdownLabelCanvas::_generate_context_menu() {
	menu = memnew(PopupMenu);
	add_child(menu, false, Node::INTERNAL_MODE_FRONT);
	menu->connect("id_pressed", callable_mp(this, &MarkdownLabelCanvas::menu_option));

	menu->add_item("Copy", MarkdownLabel::MENU_COPY);
	menu->add_item("Select All", MarkdownLabel::MENU_SELECT_ALL);
}

void MarkdownLabelCanvas::_update_context_menu() {
	if (menu == nullptr) {
		_generate_context_menu();
	}

	int32_t idx = menu->get_item_index(MarkdownLabel::MENU_COPY);
	if (idx >= 0) {
		menu->set_item_accelerator(idx, label != nullptr && label->is_shortcut_keys_enabled() ? _get_menu_action_accelerator("ui_copy") : KEY_NONE);
		menu->set_item_disabled(idx, label == nullptr || !label->is_selection_enabled());
	}

	idx = menu->get_item_index(MarkdownLabel::MENU_SELECT_ALL);
	if (idx >= 0) {
		menu->set_item_accelerator(idx, label != nullptr && label->is_shortcut_keys_enabled() ? _get_menu_action_accelerator("ui_text_select_all") : KEY_NONE);
		menu->set_item_disabled(idx, label == nullptr || !label->is_selection_enabled());
	}
}

Key MarkdownLabelCanvas::_get_menu_action_accelerator(const String& p_action) {
	const TypedArray<Ref<InputEvent>> events = InputMap::get_singleton()->action_get_events(p_action);
	if (events.is_empty()) {
		return KEY_NONE;
	}

	const Ref<InputEvent> first_event = events[0];
	if (first_event.is_null()) {
		return KEY_NONE;
	}

	const Ref<InputEventKey> key_event = first_event;
	if (key_event.is_null()) {
		return KEY_NONE;
	}

	if (key_event->get_physical_keycode() != KEY_NONE) {
		return key_event->get_physical_keycode_with_modifiers();
	}
	else {
		return key_event->get_keycode_with_modifiers();
	}
}

PopupMenu* MarkdownLabelCanvas::get_menu() {
	if (menu == nullptr) {
		_generate_context_menu();
	}
	return menu;
}

bool MarkdownLabelCanvas::is_menu_visible() const {
	return menu != nullptr && menu->is_visible();
}

void MarkdownLabelCanvas::menu_option(int p_option) {
	switch (p_option) {
	case MarkdownLabel::MENU_COPY: {
		String txt = get_selected_text();
		if (txt.is_empty()) {
			txt = plain_text;
		}
		if (!txt.is_empty()) {
			DisplayServer::get_singleton()->clipboard_set(txt);
		}
	} break;
	case MarkdownLabel::MENU_SELECT_ALL: {
		select_all();
	} break;
	}
}

int32_t MarkdownLabelCanvas::get_rendered_block_count() {
	flush_pending_layout();
	rebuild_layout();
	return rendered_block_count;
}

float MarkdownLabelCanvas::get_content_height() {
	flush_pending_layout();
	rebuild_layout();
	return content_height;
}

bool MarkdownLabelCanvas::has_anchor(const String& p_anchor) const {
	const_cast<MarkdownLabelCanvas*>(this)->flush_pending_layout();
	const_cast<MarkdownLabelCanvas*>(this)->rebuild_layout();
	return anchor_offsets.has(p_anchor);
}

PackedStringArray MarkdownLabelCanvas::get_anchors() const {
	const_cast<MarkdownLabelCanvas*>(this)->flush_pending_layout();
	const_cast<MarkdownLabelCanvas*>(this)->rebuild_layout();
	PackedStringArray result;
	const Array keys = anchor_offsets.keys();
	for (int32_t i = 0; i < keys.size(); i++) {
		result.append(keys[i]);
	}
	return result;
}

float MarkdownLabelCanvas::get_anchor_offset(const String& p_anchor) const {
	const_cast<MarkdownLabelCanvas*>(this)->flush_pending_layout();
	const_cast<MarkdownLabelCanvas*>(this)->rebuild_layout();
	if (!anchor_offsets.has(p_anchor)) {
		return -1.0f;
	}
	return anchor_offsets[p_anchor];
}

float MarkdownLabelCanvas::get_search_result_y() const {
	const_cast<MarkdownLabelCanvas*>(this)->flush_pending_layout();
	const_cast<MarkdownLabelCanvas*>(this)->rebuild_layout();
	if (search_match_start < 0 || search_match_end <= search_match_start) {
		return -1.0f;
	}

	for (const MarkdownCanvasItem& item : items) {
		if (item.html_layout.is_valid() && search_match_start >= item.global_start && search_match_start < item.global_end) {
			const auto rects = item.html_layout->get_selection_rects(search_match_start - item.global_start, search_match_end - item.global_start);
			return item.text_rect.position.y + (rects.is_empty() ? 0.0f : Rect2(rects[0]).position.y);
		}
		if (!item.container_lines.empty() || item.type == MARKDOWN_BLOCK_BLOCKQUOTE) {
			for (const MarkdownContainerLine& line : item.container_lines) {
				if (search_match_start >= line.global_start && search_match_start < line.global_end) {
					if (line.html_layout.is_valid()) {
						const auto rects = line.html_layout->get_selection_rects(search_match_start - line.global_start, search_match_end - line.global_start);
						return line.text_rect.position.y + (rects.is_empty() ? 0.0f : Rect2(rects[0]).position.y);
					}
					return item.rect.position.y;
				}
			}
		}
		if (search_match_start >= item.paragraph_global_start && search_match_start < item.paragraph_global_end) {
			return item.rect.position.y;
		}
		if (search_match_start >= item.header_global_start && search_match_start < item.header_global_end) {
			return item.rect.position.y;
		}
	}

	return -1.0f;
}

float MarkdownLabelCanvas::get_bottom_position() const {
	const_cast<MarkdownLabelCanvas*>(this)->flush_pending_layout();
	const_cast<MarkdownLabelCanvas*>(this)->rebuild_layout();
	return content_height;
}

Vector2 MarkdownLabelCanvas::_get_minimum_size() const {
	return Vector2(1.0, content_height);
}

void MarkdownLabelCanvas::build_theme_cache() {
	const int32_t extra = label == nullptr ? 0 : label->get_extra_font_size();
	theme_cache.build(label, extra);
}

void MarkdownLabelCanvas::rebuild_layout() {
	const float width = std::max<float>(1.0f, get_size().x);
	const MarkdownParseOptions options = get_parse_options();
	const int32_t viewport_height = label != nullptr && !label->fit_content
		? std::max(0, static_cast<int32_t>(label->get_size().y - 2 * label->content_margin)) : 0;
	int32_t html_height = 0;
	if (options.parse_html) {
		// Scene instantiation can request the minimum size before the canvas has a viewport.
		html_height = viewport_height;
		if (html_height <= 0) {
			html_height = is_inside_tree() ? std::max(1, static_cast<int32_t>(get_viewport_rect().size.y)) : 1;
		}
	}
	const bool same_size = std::abs(width - layout_width) < 0.01f && viewport_height == layout_height && html_height == layout_html_height;
	if (!layout_dirty && same_size) {
		return;
	}

	layout_dirty = false;
	const bool can_reuse_layout_prefix = layout_prefix_reuse_requested && streaming_enabled && !items.empty() && same_size;
	const std::vector<MarkdownCanvasItem> previous_items = can_reuse_layout_prefix ? items : std::vector<MarkdownCanvasItem>();
	const std::vector<MarkdownCanvasLine> previous_lines = can_reuse_layout_prefix ? lines : std::vector<MarkdownCanvasLine>();
	const String previous_plain_text = can_reuse_layout_prefix ? plain_text : String();
	layout_width = width;
	layout_height = viewport_height;
	layout_html_height = html_height;
	pressed_html_link = 0;
	items.clear();
	lines.clear();
	plain_text = String();
	rendered_block_count = 0;
	anchor_offsets.clear();
	footnote_reference_offsets.clear();
	footnote_definition_offsets.clear();

	build_theme_cache();
	const String html_base_dir = label ? label->get_html_base_dir() : String("res://");
	const String html_defaults = options.parse_html
		? "html { color: #" + theme_cache.color.text.to_html(true) + "; font-size: " +
			String::num_int64(theme_cache.font_size.text) + "px; } a:link { color: #" + theme_cache.color.link.to_html(true) +
			"; } pre, xmp, plaintext, listing, pre code, pre tt, pre kbd, pre samp { font-family: docrender-code-block; }"
		: String();
	const auto& font = theme_cache.font;
	const auto html_theme_font = [&](const StringName& p_name, const Ref<Font>& p_font) {
		return options.parse_html && label && label->has_theme_font(p_name, "MarkdownLabel") ? p_font : Ref<Font>();
	};
	const Ref<Font> html_code_font = html_theme_font("code_text_font", font.code);
	const Ref<Font> html_code_block_font = html_theme_font("code_block_font", font.code_block);
	const HTMLLayout::Fonts html_fonts{ font.text,
		html_theme_font("bold_font", font.bold), html_theme_font("italic_font", font.italic),
		html_theme_font("bold_italic_font", font.bold_italic), html_code_font,
		html_code_block_font.is_valid() ? html_code_block_font : html_code_font };
	auto layout_html = [&](const String& p_source, float p_width) -> Ref<HTMLLayout> {
		Ref<HTMLLayout> html;
		html.instantiate();
		if (html->parse_source(p_source, html_base_dir, String(), Dictionary(), true, html_defaults, html_fonts) != OK ||
			html->layout(Vector2i(std::max(1, static_cast<int32_t>(p_width)), html_height)) != OK) {
			return {};
		}
		return html;
	};
	HashMap<String, int32_t> footnote_numbers;

	auto finalize_inline_result = [&](MarkdownInlineSpanParseResult p_result) {
		if (p_result.spans.empty()) {
			return p_result;
		}

		MarkdownInlineSpanParseResult result;
		int64_t old_position = 0;
		for (MarkdownInlineSpan span : p_result.spans) {
			const int64_t old_span_end = span.end;
			if (span.start > old_position) {
				result.output_text += p_result.output_text.substr(old_position, span.start - old_position);
			}

			if (span.footnote_ref) {
				if (!footnote_numbers.has(span.footnote_id)) {
					footnote_numbers[span.footnote_id] = static_cast<int32_t>(footnote_numbers.size()) + 1;
				}
				span.footnote_number = footnote_numbers[span.footnote_id];
				span.text = String::num_int64(span.footnote_number);
			}

			span.start = result.output_text.length();
			result.output_text += span.text;
			span.end = result.output_text.length();
			result.spans.push_back(span);
			old_position = std::max<int64_t>(old_position, old_span_end);
		}

		if (old_position < p_result.output_text.length()) {
			result.output_text += p_result.output_text.substr(old_position);
		}
		return result;
		};

	auto parse_inline = [&](const String& p_text) {
		return finalize_inline_result(parse_inline_spans(p_text, options));
	};

	auto paragraph_y_for_local_character = [&](const Ref<TextParagraph>& p_paragraph, const Rect2& p_text_rect, int64_t p_local_character) {
		if (p_paragraph.is_null()) {
			return p_text_rect.position.y;
		}

		float paragraph_y = p_text_rect.position.y;
		for (int32_t line_index = 0; line_index < p_paragraph->get_line_count(); line_index++) {
			const Vector2i range = p_paragraph->get_line_range(line_index);
			if (p_local_character >= range.x && p_local_character <= range.y) {
				return paragraph_y;
			}
			paragraph_y += p_paragraph->get_line_size(line_index).y + p_paragraph->get_line_spacing();
		}
		return p_text_rect.position.y;
		};

	auto record_footnote_reference_offsets = [&](const Ref<TextParagraph>& p_paragraph, const Rect2& p_text_rect, const std::vector<MarkdownInlineSpan>& p_spans) {
		for (const MarkdownInlineSpan& span : p_spans) {
			if (span.footnote_ref && !span.footnote_id.is_empty() && !footnote_reference_offsets.has(span.footnote_id)) {
				footnote_reference_offsets[span.footnote_id] = paragraph_y_for_local_character(p_paragraph, p_text_rect, span.start);
			}
		}
		};

	auto register_footnote_numbers = [&](const std::vector<MarkdownInlineSpan>& p_spans) {
		for (const MarkdownInlineSpan& span : p_spans) {
			if (span.footnote_ref && !span.footnote_id.is_empty() && !footnote_numbers.has(span.footnote_id)) {
				footnote_numbers[span.footnote_id] = span.footnote_number > 0 ? span.footnote_number : static_cast<int32_t>(footnote_numbers.size()) + 1;
			}
		}
		};

	auto register_item_footnote_numbers = [&](const MarkdownCanvasItem& p_item) {
		register_footnote_numbers(p_item.spans);
		for (const MarkdownTableCell& cell : p_item.cells) {
			register_footnote_numbers(cell.spans);
		}
		for (const MarkdownContainerLine& line : p_item.container_lines) {
			register_footnote_numbers(line.spans);
		}
		};

	float y = 0.0f;

	std::vector<MarkdownBlock> blocks;
	if (streaming_enabled && !cached_blocks.empty()) {
		blocks = cached_blocks;
	}
	else {
		MarkdownParseResult result = parse_markdown_document(raw_text, nullptr, INT_MAX, options);
		blocks = result.blocks;
		cached_blocks = blocks;
		parser_state = result.state;
	}

	HashSet<String> mermaid_sources;
	if (label != nullptr && label->is_parsing_mermaid() && MermaidRender::is_available()) {
		collect_mermaid_sources(blocks, mermaid_sources);
	}
	MermaidRenderCache::Palette mermaid_palette;
	mermaid_palette.light = theme_cache.color.text.get_luminance() < 0.5f;
	mermaid_cache.update(mermaid_sources, mermaid_palette);

	int32_t block_start_index = 0;
	if (can_reuse_layout_prefix) {
		const int32_t max_reuse = std::min<int32_t>(static_cast<int32_t>(blocks.size()), static_cast<int32_t>(previous_items.size()));
		for (int32_t reuse_index = 0; reuse_index < max_reuse; reuse_index++) {
			const MarkdownBlock& block = blocks[reuse_index];
			if (block.type == MARKDOWN_BLOCK_FOOTNOTES) {
				break;
			}

			const uint64_t block_hash = compute_block_hash(block);
			const MarkdownCanvasItem& previous_item = previous_items[reuse_index];
			if (previous_item.type != block.type || previous_item.content_hash != block_hash) {
				break;
			}

			items.push_back(previous_item);
			items.back().source_line = block.line - 1;
			register_item_footnote_numbers(previous_item);
			record_footnote_reference_offsets(previous_item.paragraph, previous_item.text_rect, previous_item.spans);
			for (const MarkdownTableCell& cell : previous_item.cells) {
				record_footnote_reference_offsets(cell.paragraph, cell.text_rect, cell.spans);
			}
			for (const MarkdownContainerLine& line : previous_item.container_lines) {
				record_footnote_reference_offsets(line.paragraph, line.text_rect, line.spans);
			}
			for (const MarkdownCanvasLine& line : previous_lines) {
				if (line.item_index == reuse_index) {
					lines.push_back(line);
				}
			}
			plain_text = previous_plain_text.substr(0, previous_item.global_end);
			const char* spacing_type = block_spacing_type(block);
			const String spacing_after_name = String(spacing_type) + "_space_after";
			const int32_t block_spacing_after = std::max<int32_t>(0, get_theme_constant_or(label, "MarkdownLabel", StringName(spacing_after_name), theme_cache.constant.paragraph_separation));
			y = previous_item.rect.get_end().y + static_cast<float>(block_spacing_after);
			rendered_block_count++;
			block_start_index = reuse_index + 1;
		}
	}

	for (int32_t block_idx = block_start_index; block_idx < static_cast<int32_t>(blocks.size()); block_idx++) {
		const MarkdownBlock& block = blocks[block_idx];
		MarkdownCanvasItem item;
		String item_text;
		std::vector<MarkdownInlineSpan> item_spans;
		Ref<Font> font = theme_cache.font.text;
		int32_t font_size = theme_cache.font_size.text;
		float left_padding = 0.0f;
		float right_padding = 0.0f;
		float top_padding = 0.0f;
		float bottom_padding = 0.0f;
		const char* spacing_type = block_spacing_type(block);
		const String spacing_before_name = String(spacing_type) + "_space_before";
		const String spacing_after_name = String(spacing_type) + "_space_after";
		const int32_t block_spacing_before = std::max<int32_t>(0, get_theme_constant_or(label, "MarkdownLabel", StringName(spacing_before_name), 0));
		const int32_t block_spacing_after = std::max<int32_t>(0, get_theme_constant_or(label, "MarkdownLabel", StringName(spacing_after_name), theme_cache.constant.paragraph_separation));

		item.text_color = theme_cache.color.text;
		item.background_color = Color(0, 0, 0, 0);
		item.border_color = Color(0, 0, 0, 0);
		item.type = block.type;
		item.source_line = block.line - 1;
		item.content_hash = compute_block_hash(block);

		switch (block.type) {
		case MARKDOWN_BLOCK_HTML: {
			item.html_layout = layout_html(block.text, width);
			if (item.html_layout.is_null()) {
				item_text = block.text;
				break;
			}
			if (y > 0.0f) {
				y += static_cast<float>(block_spacing_before);
			}
			const String text = item.html_layout->get_plain_text();
			if (!text.is_empty() && !plain_text.is_empty()) {
				plain_text += "\n\n";
			}
			item.global_start = plain_text.length();
			plain_text += text;
			item.global_end = plain_text.length();
			item.rect = Rect2(0.0f, y, width, item.html_layout->get_content_size().y);
			item.text_rect = item.rect;
			items.push_back(std::move(item));
			y += items.back().rect.size.y + static_cast<float>(block_spacing_after);
			rendered_block_count++;
			continue;
		}
		case MARKDOWN_BLOCK_HEADING: {
			const int32_t index = std::min<int32_t>(std::max<int32_t>(block.level - 1, 0), 5);
			font_size = theme_cache.font_size.heading[index];
			font = theme_cache.font.heading[index];
			item.text_color = theme_cache.color.heading[index];
			item.heading = true;
			item.heading_level = block.level;
			item.anchor = block.anchor;

			MarkdownInlineSpanParseResult parsed = parse_inline(block.text);
			item_text = parsed.output_text;
			item_spans = parsed.spans;
		} break;
		case MARKDOWN_BLOCK_CODE:
			item_text = block.text;
			font = theme_cache.font.code_block;
			font_size = theme_cache.font_size.code_block;
			item.text_color = theme_cache.color.code_block;
			item.background_color = theme_cache.color.code_block_background;
			item.panel_stylebox = theme_cache.stylebox.code_block_panel;
			item.framed = true;
			item.code = true;
			item.code_language = block.argument;
			left_padding = get_stylebox_margin_or(item.panel_stylebox, SIDE_LEFT, 8.0f);
			top_padding = get_stylebox_margin_or(item.panel_stylebox, SIDE_TOP, 8.0f);
			right_padding = get_stylebox_margin_or(item.panel_stylebox, SIDE_RIGHT, 8.0f);
			bottom_padding = get_stylebox_margin_or(item.panel_stylebox, SIDE_BOTTOM, 8.0f);
			break;
		case MARKDOWN_BLOCK_LATEX:
			item_text = block.text;
			item_spans.push_back(make_display_math_span(block));
			break;
		case MARKDOWN_BLOCK_TASK_LIST:
		case MARKDOWN_BLOCK_UNORDERED_LIST:
		case MARKDOWN_BLOCK_ORDERED_LIST:
			if (block.list_items.empty()) {
				for (int32_t i = 0; i < block.items.size(); i++) {
					if (!item_text.is_empty()) {
						item_text += "\n";
					}
					MarkdownInlineSpanParseResult parsed = parse_inline(block.items[i]);
					const int64_t item_start = item_text.length();
					item_text += parsed.output_text;
					for (MarkdownInlineSpan& span : parsed.spans) {
						span.start += item_start;
						span.end += item_start;
						item_spans.push_back(span);
					}
				}
				left_padding = static_cast<float>(theme_cache.constant.list_indent) * (1.0f + static_cast<float>(block.indent));
				break;
			}
			[[fallthrough]];
		case MARKDOWN_BLOCK_BLOCKQUOTE: {
			if (y > 0.0f) {
				y += static_cast<float>(block_spacing_before);
			}

			const bool root_quote = block.type == MARKDOWN_BLOCK_BLOCKQUOTE;
			if (root_quote) {
				item.panel_stylebox = theme_cache.stylebox.blockquote;
				item.framed = true;
				item.blockquote_depth = 1;
			}

			const float panel_top = root_quote ? get_stylebox_margin_or(item.panel_stylebox, SIDE_TOP, 16.0f) : 0.0f;
			const float panel_right = root_quote ? get_stylebox_margin_or(item.panel_stylebox, SIDE_RIGHT, 8.0f) : 0.0f;
			const float panel_bottom = root_quote ? get_stylebox_margin_or(item.panel_stylebox, SIDE_BOTTOM, 16.0f) : 0.0f;
			const float panel_left = root_quote ? get_stylebox_margin_or(item.panel_stylebox, SIDE_LEFT, 28.0f) : 0.0f;
			const float quote_indent = static_cast<float>(std::max<int32_t>(12, get_theme_constant_or(label, "MarkdownLabel", "blockquote_indent", 25)));
			const float quote_line_gap = static_cast<float>(std::max<int32_t>(0, get_theme_constant_or(label, "MarkdownLabel", "blockquote_line_gap", 14)));
			const float nested_left = get_stylebox_margin_or(theme_cache.stylebox.blockquote_nested, SIDE_LEFT, 28.0f);
			const float nested_top = get_stylebox_margin_or(theme_cache.stylebox.blockquote_nested, SIDE_TOP, 0.0f);
			const float nested_right = get_stylebox_margin_or(theme_cache.stylebox.blockquote_nested, SIDE_RIGHT, 8.0f);
			const float nested_bottom = get_stylebox_margin_or(theme_cache.stylebox.blockquote_nested, SIDE_BOTTOM, 0.0f);

			if (!plain_text.is_empty()) {
				plain_text += "\n\n";
			}
			item.global_start = plain_text.length();
			item.paragraph_global_start = item.global_start;

			float quote_y = y + panel_top;
			bool quote_has_text = false;
			int32_t last_quote_spacing_after = 0;

			auto get_quote_content_rect = [&](int32_t p_depth) {
				const int32_t depth = std::max<int32_t>(1, p_depth);
				float line_left = panel_left;
				float line_width = std::max<float>(1.0f, width - panel_left - panel_right);
				const int32_t base_depth = root_quote ? 1 : 0;
				if (p_depth > base_depth) {
					const float nested_rect_left = panel_left + static_cast<float>(depth - base_depth - 1) * quote_indent;
					line_left = nested_rect_left + nested_left;
					line_width = std::max<float>(1.0f, width - nested_rect_left - nested_left - nested_right - panel_right);
				}
				return Rect2(line_left, 0.0f, line_width, 0.0f);
				};

			auto add_quote_depth_spacing = [&](int32_t p_depth) {
				if (item.container_lines.empty()) {
					return;
				}

				const int32_t previous_depth = std::max<int32_t>(1, item.container_lines.back().depth);
				const int32_t depth = std::max<int32_t>(1, p_depth);
				if (depth > previous_depth) {
					quote_y += nested_top * static_cast<float>(depth - previous_depth);
				}
				else if (depth < previous_depth) {
					quote_y += nested_bottom * static_cast<float>(previous_depth - depth);
				}
				};

			auto append_quote_plain_text = [&](MarkdownContainerLine& r_line) {
				const String text = r_line.html_layout.is_valid() ? r_line.html_layout->get_plain_text() : r_line.text;
				if (r_line.html_layout.is_valid() && text.is_empty()) {
					r_line.global_start = r_line.global_end = plain_text.length();
					return;
				}
				if (quote_has_text) {
					plain_text += "\n\n";
				}
				quote_has_text = true;
				r_line.global_start = plain_text.length();
				plain_text += text;
				r_line.global_end = plain_text.length();
				};

			auto add_quote_line = [&](MarkdownContainerLine& r_line, int32_t p_block_spacing_before, int32_t p_block_spacing_after) {
				add_quote_depth_spacing(r_line.depth);
				if (!item.container_lines.empty()) {
					quote_y += static_cast<float>(p_block_spacing_before);
				}

				append_quote_plain_text(r_line);
				item.blockquote_depth = std::max<int32_t>(item.blockquote_depth, r_line.depth);
				record_footnote_reference_offsets(r_line.paragraph, r_line.text_rect, r_line.spans);
				item.container_lines.push_back(r_line);
				last_quote_spacing_after = p_block_spacing_after;
				quote_y = item.container_lines.back().rect.get_end().y + static_cast<float>(p_block_spacing_after);
				};

			std::function<void(const MarkdownBlock&, int32_t, float)> append_container_block;
			append_container_block = [&](const MarkdownBlock& p_child, int32_t p_depth, float p_list_indent) {
				if (p_child.type == MARKDOWN_BLOCK_BLOCKQUOTE) {
					const int32_t child_depth = std::max<int32_t>(1, p_depth + 1);
					if (p_child.children.empty() && !p_child.text.is_empty()) {
						MarkdownBlock fallback_child;
						fallback_child.type = MARKDOWN_BLOCK_PARAGRAPH;
						fallback_child.text = p_child.text;
						append_container_block(fallback_child, child_depth, p_list_indent);
					}
					for (const MarkdownBlock& nested_child : p_child.children) {
						append_container_block(nested_child, child_depth, p_list_indent);
					}
					return;
				}
				if (!p_child.list_items.empty()) {
					const float indent = p_list_indent + theme_cache.constant.list_indent * (1 + p_child.indent);
					for (size_t index = 0; index < p_child.list_items.size(); ++index) {
						const MarkdownListItem& list_item = p_child.list_items[index];
						const size_t first = item.container_lines.size();
						for (const MarkdownBlock& nested_child : list_item.children) {
							append_container_block(nested_child, p_depth, indent);
						}
						const Rect2 column = get_quote_content_rect(p_depth);
						if (first == item.container_lines.size()) {
							MarkdownContainerLine empty;
							empty.depth = p_depth;
							empty.rect = Rect2(column.position.x + indent, quote_y, std::max<float>(1.0f, column.size.x - indent),
								theme_cache.font_size.text);
							empty.text_rect = empty.rect;
							add_quote_line(empty, 0, theme_cache.constant.line_separation);
						}
						MarkdownContainerLine& first_line = item.container_lines[first];
						MarkdownListMarker marker;
						marker.font = theme_cache.font.list_marker;
						marker.font_size = theme_cache.font_size.list_marker;
						marker.color = theme_cache.color.list_marker;
						if (p_child.type == MARKDOWN_BLOCK_TASK_LIST) {
							marker.task_checked = list_item.checked;
							marker.icon = list_item.checked ? theme_cache.icon.task_checked : theme_cache.icon.task_unchecked;
						}
						else {
							marker.text = p_child.type == MARKDOWN_BLOCK_ORDERED_LIST
								? String::num_int64(p_child.list_start + static_cast<int64_t>(index)) + "."
								: String::chr(0x2022);
						}
						const float marker_width = marker.icon.is_valid() ? marker.font_size :
							marker.font.is_valid() ? marker.font->get_string_size(marker.text, HORIZONTAL_ALIGNMENT_LEFT, -1, marker.font_size).x : 0.0f;
						const float ascent = first_line.paragraph.is_valid() && first_line.paragraph->get_line_count() > 0
							? first_line.paragraph->get_line_ascent(0)
							: theme_cache.font.text->get_ascent(theme_cache.font_size.text);
						marker.position = Vector2(std::max<float>(0.0f, column.position.x + indent - theme_cache.constant.list_marker_gap - marker_width),
							first_line.text_rect.position.y + ascent);
						first_line.list_markers.push_back(std::move(marker));
					}
					return;
				}

				MarkdownBlock render_block = p_child;
				if (render_block.type == MARKDOWN_BLOCK_TABLE) {
					String table_text;
					for (int32_t row_index = 0; row_index < render_block.rows.size(); row_index++) {
						if (!table_text.is_empty()) {
							table_text += "\n";
						}
						for (int32_t column_index = 0; column_index < render_block.rows[row_index].size(); column_index++) {
							if (column_index > 0) {
								table_text += " | ";
							}
							table_text += render_block.rows[row_index][column_index];
						}
					}
					render_block.type = MARKDOWN_BLOCK_PARAGRAPH;
					render_block.text = table_text;
				}

				const char* child_spacing_type = block_spacing_type(render_block);
				const String child_spacing_before_name = String(child_spacing_type) + "_space_before";
				const String child_spacing_after_name = String(child_spacing_type) + "_space_after";
				const int32_t child_spacing_before = std::max<int32_t>(0, get_theme_constant_or(label, "MarkdownLabel", StringName(child_spacing_before_name), 0));
				const int32_t child_spacing_after = std::max<int32_t>(0, get_theme_constant_or(label, "MarkdownLabel", StringName(child_spacing_after_name), quote_line_gap));

				MarkdownContainerLine quote_line;
				quote_line.type = render_block.type;
				quote_line.depth = p_depth;
				quote_line.text_color = theme_cache.color.text;
				quote_line.anchor = render_block.anchor;

				Ref<Font> quote_font = theme_cache.font.text;
				int32_t quote_font_size = theme_cache.font_size.text;
				float local_left_padding = 0.0f;
				float local_top_padding = 0.0f;
				float local_right_padding = 0.0f;
				float local_bottom_padding = 0.0f;

				switch (render_block.type) {
				case MARKDOWN_BLOCK_HTML:
					quote_line.text = render_block.text;
					break;
				case MARKDOWN_BLOCK_HEADING: {
					const int32_t heading_index = std::min<int32_t>(std::max<int32_t>(render_block.level - 1, 0), 5);
					quote_font = theme_cache.font.heading[heading_index];
					quote_font_size = theme_cache.font_size.heading[heading_index];
					quote_line.text_color = theme_cache.color.heading[heading_index];
					MarkdownInlineSpanParseResult parsed = parse_inline(render_block.text);
					quote_line.text = parsed.output_text;
					quote_line.spans = parsed.spans;
				} break;
				case MARKDOWN_BLOCK_TASK_LIST:
				case MARKDOWN_BLOCK_UNORDERED_LIST:
				case MARKDOWN_BLOCK_ORDERED_LIST: {
					for (int32_t i = 0; i < render_block.items.size(); i++) {
						if (!quote_line.text.is_empty()) {
							quote_line.text += "\n";
						}
						MarkdownInlineSpanParseResult parsed = parse_inline(render_block.items[i]);
						const int64_t item_start = quote_line.text.length();
						quote_line.text += parsed.output_text;
						for (MarkdownInlineSpan& span : parsed.spans) {
							span.start += item_start;
							span.end += item_start;
							quote_line.spans.push_back(span);
						}
					}
					local_left_padding = static_cast<float>(theme_cache.constant.list_indent) * (1.0f + static_cast<float>(render_block.indent));
				} break;
				case MARKDOWN_BLOCK_CODE:
					quote_line.text = render_block.text;
					quote_font = theme_cache.font.code_block;
					quote_font_size = theme_cache.font_size.code_block;
					quote_line.text_color = theme_cache.color.code_block;
					quote_line.stylebox = theme_cache.stylebox.code_block_panel;
					local_left_padding = get_stylebox_margin_or(quote_line.stylebox, SIDE_LEFT, 8.0f);
					local_top_padding = get_stylebox_margin_or(quote_line.stylebox, SIDE_TOP, 8.0f);
					local_right_padding = get_stylebox_margin_or(quote_line.stylebox, SIDE_RIGHT, 8.0f);
					local_bottom_padding = get_stylebox_margin_or(quote_line.stylebox, SIDE_BOTTOM, 8.0f);
					break;
				case MARKDOWN_BLOCK_THEMATIC_BREAK:
					quote_line.stylebox = theme_cache.stylebox.separator;
					break;
				case MARKDOWN_BLOCK_LATEX:
					quote_line.text = render_block.text;
					quote_line.spans.push_back(make_display_math_span(render_block));
					break;
				case MARKDOWN_BLOCK_IMAGE: {
					quote_line.text = String("[Image: ") + render_block.argument + "]";
					MarkdownInlineSpan span;
					span.text = quote_line.text;
					span.italic = true;
					span.start = 0;
					span.end = quote_line.text.length();
					quote_line.spans.push_back(span);
				} break;
				case MARKDOWN_BLOCK_PARAGRAPH:
				default: {
					MarkdownInlineSpanParseResult parsed = parse_inline(render_block.text);
					quote_line.text = parsed.output_text;
					quote_line.spans = parsed.spans;
				} break;
				}

				Rect2 content_rect = get_quote_content_rect(quote_line.depth);
				content_rect.position.x += p_list_indent;
				content_rect.size.x = std::max<float>(1.0f, content_rect.size.x - p_list_indent);
				if (render_block.type == MARKDOWN_BLOCK_HTML) {
					quote_line.html_layout = layout_html(render_block.text, content_rect.size.x);
					if (quote_line.html_layout.is_valid()) {
						quote_line.rect = Rect2(content_rect.position.x, quote_y, content_rect.size.x,
							quote_line.html_layout->get_content_size().y);
						quote_line.text_rect = quote_line.rect;
						quote_line.text = String();
						add_quote_line(quote_line, child_spacing_before, child_spacing_after);
						return;
					}
				}
				if (render_block.type == MARKDOWN_BLOCK_THEMATIC_BREAK) {
					const float separator_height = get_stylebox_margin_or(quote_line.stylebox, SIDE_TOP, 2.0f) + get_stylebox_margin_or(quote_line.stylebox, SIDE_BOTTOM, 1.0f);
					quote_line.rect = Rect2(content_rect.position.x, quote_y, content_rect.size.x, std::max<float>(1.0f, separator_height));
					quote_line.text_rect = quote_line.rect;
					quote_line.text = String();
					add_quote_line(quote_line, child_spacing_before, child_spacing_after);
					return;
				}

				if (quote_line.text.is_empty()) {
					return;
				}

				const float paragraph_width = std::max<float>(1.0f, content_rect.size.x - local_left_padding - local_right_padding);
				quote_line.paragraph.instantiate();
				configure_paragraph_autowrap(quote_line.paragraph, label);
				quote_line.paragraph->set_width(paragraph_width);
				quote_line.paragraph->set_line_spacing(theme_cache.constant.line_separation);
				if (!add_mermaid_to_paragraph(quote_line.paragraph, mermaid_cache, render_block, paragraph_width, quote_line.objects)) {
					add_spans_to_paragraph(quote_line.paragraph, quote_line.text, quote_line.spans, theme_cache, quote_font, quote_font_size, quote_line.text_color, quote_line.objects, paragraph_width, this);
				}
				if (quote_line.type == MARKDOWN_BLOCK_LATEX && !quote_line.objects.formulas.is_empty()) {
					quote_line.paragraph->set_alignment(HORIZONTAL_ALIGNMENT_CENTER);
				}

				const Vector2 paragraph_size = quote_line.paragraph->get_size();
				const Vector2 inline_code_padding = get_inline_code_vertical_padding(quote_line.spans, theme_cache);
				local_top_padding += inline_code_padding.x;
				local_bottom_padding += inline_code_padding.y;
				quote_line.text_rect = Rect2(content_rect.position.x + local_left_padding, quote_y + local_top_padding, paragraph_width, paragraph_size.y);
				quote_line.rect = Rect2(content_rect.position.x, quote_y, content_rect.size.x, paragraph_size.y + local_top_padding + local_bottom_padding);

				if (render_block.type == MARKDOWN_BLOCK_UNORDERED_LIST || render_block.type == MARKDOWN_BLOCK_ORDERED_LIST || render_block.type == MARKDOWN_BLOCK_TASK_LIST) {
					float list_line_y = quote_line.text_rect.position.y;
					for (int32_t marker_index = 0; marker_index < render_block.items.size(); marker_index++) {
						String marker_text;
						if (render_block.type == MARKDOWN_BLOCK_ORDERED_LIST) {
							marker_text = String::num_int64(std::max<int32_t>(1, render_block.list_start) + marker_index) + ".";
						}
						else if (render_block.type == MARKDOWN_BLOCK_TASK_LIST) {
							marker_text = " ";
						}
						else {
							marker_text = String::chr(0x2022);
						}

						float marker_size_x = 0.0f;
						if (render_block.type == MARKDOWN_BLOCK_TASK_LIST) {
							marker_size_x = static_cast<float>(theme_cache.font_size.list_marker);
						}
						else if (theme_cache.font.list_marker.is_valid()) {
							const Vector2 marker_size = theme_cache.font.list_marker->get_string_size(marker_text, HORIZONTAL_ALIGNMENT_RIGHT, static_cast<int32_t>(quote_line.text_rect.position.x - theme_cache.constant.list_marker_gap), theme_cache.font_size.list_marker);
							marker_size_x = marker_size.x;
						}

						MarkdownListMarker marker;
						marker.text = marker_text;
						marker.position = Vector2(std::max<float>(0.0f, quote_line.text_rect.position.x - static_cast<float>(theme_cache.constant.list_marker_gap) - marker_size_x), list_line_y + quote_line.paragraph->get_line_ascent(0));
						marker.font = theme_cache.font.list_marker;
						marker.font_size = theme_cache.font_size.list_marker;
						marker.color = theme_cache.color.list_marker;
						if (render_block.type == MARKDOWN_BLOCK_TASK_LIST && marker_index < static_cast<int32_t>(render_block.task_checked.size())) {
							marker.task_checked = render_block.task_checked[marker_index];
							marker.icon = marker.task_checked ? theme_cache.icon.task_checked : theme_cache.icon.task_unchecked;
						}
						quote_line.list_markers.push_back(marker);
						list_line_y += quote_line.paragraph->get_line_size(0).y + theme_cache.constant.line_separation;
					}
				}

				add_quote_line(quote_line, child_spacing_before, child_spacing_after);
				};

			if (!root_quote) {
				append_container_block(block, 0, 0.0f);
			}
			else if (!block.children.empty()) {
				for (const MarkdownBlock& child : block.children) {
					append_container_block(child, 1, 0.0f);
				}
			}
			else {
				MarkdownBlock fallback_child;
				fallback_child.type = MARKDOWN_BLOCK_PARAGRAPH;
				fallback_child.text = block.text;
				append_container_block(fallback_child, 1, 0.0f);
			}

			if (!item.container_lines.empty()) {
				const int32_t last_depth = std::max<int32_t>(1, item.container_lines.back().depth);
				if (last_depth > 1) {
					quote_y += nested_bottom * static_cast<float>(last_depth - 1);
				}
				quote_y -= static_cast<float>(last_quote_spacing_after);
			}
			item.global_end = plain_text.length();
			item.paragraph_global_end = item.global_end;
			item.rect = Rect2(0.0f, y, width, std::max<float>(1.0f, quote_y - y + panel_bottom));
			item.text_rect = item.rect;
			item.text_color = theme_cache.color.text;
			items.push_back(item);
			y += item.rect.size.y + static_cast<float>(block_spacing_after);
			rendered_block_count++;
			continue;
		}
		case MARKDOWN_BLOCK_THEMATIC_BREAK:
			item.panel_stylebox = theme_cache.stylebox.separator;
			item.framed = true;
			break;
		case MARKDOWN_BLOCK_FOOTNOTES: {
			item.type = MARKDOWN_BLOCK_FOOTNOTES;
			font = theme_cache.font.footnote_text;
			font_size = theme_cache.font_size.footnote_text;
			item.text_color = theme_cache.color.footnote_text;
			top_padding = static_cast<float>(theme_cache.constant.footnote_space_before);

			for (int32_t current_number = 1; current_number <= static_cast<int32_t>(footnote_numbers.size()); current_number++) {
				for (const MarkdownFootnoteDefinition& definition : block.footnotes) {
					if (!footnote_numbers.has(definition.id) || footnote_numbers[definition.id] != current_number) {
						continue;
					}

					if (!item_text.is_empty()) {
						item_text += "\n";
					}

					const int64_t footnote_start = item_text.length();
					const String number_text = String::num_int64(current_number) + ". ";
					item_text += number_text;
					MarkdownInlineSpan number_span;
					number_span.text = number_text;
					number_span.start = footnote_start;
					number_span.end = item_text.length();
					item_spans.push_back(number_span);
					MarkdownInlineSpanParseResult parsed = parse_inline(definition.text);
					const int64_t content_start = item_text.length();
					item_text += parsed.output_text;
					for (MarkdownInlineSpan& span : parsed.spans) {
						span.start += content_start;
						span.end += content_start;
						item_spans.push_back(span);
					}

					item.footnote_ids.push_back(definition.id);
					item.footnote_numbers.push_back(current_number);
					item.footnote_starts.push_back(footnote_start);
					item.footnote_ends.push_back(item_text.length());
				}
			}
		} break;
		case MARKDOWN_BLOCK_IMAGE: {
			item_text = String("[Image: ") + block.argument + "]";
			MarkdownInlineSpan span;
			span.text = item_text;
			span.italic = true;
			span.start = 0;
			span.end = item_text.length();
			item_spans.push_back(span);
		} break;
		case MARKDOWN_BLOCK_TABLE: {
			item.type = MARKDOWN_BLOCK_TABLE;
			const int32_t columns = block.columns > 0 ? block.columns : 1;
			const float table_left = get_stylebox_margin_or(theme_cache.stylebox.table_panel, SIDE_LEFT, 0.0f);
			const float table_top = get_stylebox_margin_or(theme_cache.stylebox.table_panel, SIDE_TOP, 0.0f);
			const float table_right = get_stylebox_margin_or(theme_cache.stylebox.table_panel, SIDE_RIGHT, 0.0f);
			const float column_width = std::max<float>(1.0f, (width - table_left - table_right) / static_cast<float>(columns));
			const bool striped = theme_cache.constant.table_striped != 0;
			item.panel_stylebox = theme_cache.stylebox.table_panel;

			std::vector<float> row_heights;
			for (int32_t row_idx = 0; row_idx < block.rows.size(); row_idx++) {
				float row_height = 0.0f;
				for (int32_t col_idx = 0; col_idx < columns && col_idx < block.rows[row_idx].size(); col_idx++) {
					MarkdownTableCell cell;
					cell.header = (row_idx < block.header_rows);
					cell.text = block.rows[row_idx][col_idx];
					MarkdownInlineSpanParseResult parsed = parse_inline(cell.text);
					cell.text = parsed.output_text;
					cell.spans = parsed.spans;
					const Ref<Font> cell_font = cell.header ? theme_cache.font.table_header : theme_cache.font.table_cell;
					const int32_t cell_font_size = cell.header ? theme_cache.font_size.table_header : theme_cache.font_size.table_cell;
					cell.text_color = cell.header ? theme_cache.color.table_header_font : theme_cache.color.table_cell_font;

					if (cell.header) {
						cell.stylebox = theme_cache.stylebox.table_header_panel;
					}
					else if (striped && row_idx % 2 == 0) {
						cell.stylebox = theme_cache.stylebox.table_striped_panel;
					}
					else {
						cell.stylebox = theme_cache.stylebox.table_cell_panel;
					}

					const float cell_left = get_stylebox_margin_or(cell.stylebox, SIDE_LEFT, 6.0f);
					const float cell_top = get_stylebox_margin_or(cell.stylebox, SIDE_TOP, 4.0f);
					const float cell_right = get_stylebox_margin_or(cell.stylebox, SIDE_RIGHT, 6.0f);
					const float cell_bottom = get_stylebox_margin_or(cell.stylebox, SIDE_BOTTOM, 4.0f);

					cell.paragraph.instantiate();
					configure_paragraph_autowrap(cell.paragraph, label);
					cell.paragraph->set_width(std::max<float>(1.0f, column_width - cell_left - cell_right));
					cell.paragraph->set_line_spacing(theme_cache.constant.line_separation);
					if (col_idx < block.column_alignments.size()) {
						cell.paragraph->set_alignment(static_cast<HorizontalAlignment>(block.column_alignments[col_idx]));
					}
					add_spans_to_paragraph(cell.paragraph, cell.text, cell.spans, theme_cache, cell_font, cell_font_size, cell.text_color, cell.objects, std::max<float>(1.0f, column_width - cell_left - cell_right), this);

					const float cell_height = cell.paragraph->get_size().y + cell_top + cell_bottom;
					row_height = std::max<float>(row_height, cell_height);
					item.cells.push_back(cell);
				}
				row_heights.push_back(row_height);
			}

			float table_y = y + table_top;
			int32_t cell_index = 0;
			for (int32_t row_idx = 0; row_idx < block.rows.size(); row_idx++) {
				for (int32_t col_idx = 0; col_idx < columns && cell_index < static_cast<int32_t>(item.cells.size()); col_idx++, cell_index++) {
					MarkdownTableCell& cell = item.cells[cell_index];
					const float cell_left = get_stylebox_margin_or(cell.stylebox, SIDE_LEFT, 6.0f);
					const float cell_top = get_stylebox_margin_or(cell.stylebox, SIDE_TOP, 4.0f);
					cell.rect = Rect2(table_left + static_cast<float>(col_idx) * column_width, table_y, column_width, row_heights[row_idx]);
					cell.text_rect = Rect2(cell.rect.position.x + cell_left, cell.rect.position.y + cell_top, column_width - cell_left - get_stylebox_margin_or(cell.stylebox, SIDE_RIGHT, 6.0f), cell.paragraph->get_size().y);
					record_footnote_reference_offsets(cell.paragraph, cell.text_rect, cell.spans);

					if (!plain_text.is_empty()) plain_text += " ";
					cell.global_start = plain_text.length();
					plain_text += cell.text;
					cell.global_end = plain_text.length();
				}
				table_y += row_heights[row_idx];
			}

			item.rect = Rect2(0.0f, y, width, table_y - y + get_stylebox_margin_or(theme_cache.stylebox.table_panel, SIDE_BOTTOM, 0.0f));
			item.text_rect = item.rect;
			y = item.rect.get_end().y;
			rendered_block_count++;
			items.push_back(item);
			continue;
		}
		case MARKDOWN_BLOCK_PARAGRAPH:
		default: {
			MarkdownInlineSpanParseResult parsed = parse_inline(block.text);
			item_text = parsed.output_text;
			item_spans = parsed.spans;
		} break;
		}

		if (item_text.is_empty() && block.type != MARKDOWN_BLOCK_THEMATIC_BREAK) {
			continue;
		}

		if (block.type == MARKDOWN_BLOCK_THEMATIC_BREAK) {
			const float sep_height = get_stylebox_margin_or(item.panel_stylebox, SIDE_TOP, 2.0f) + get_stylebox_margin_or(item.panel_stylebox, SIDE_BOTTOM, 1.0f);
			if (y > 0.0f) {
				y += static_cast<float>(block_spacing_before);
			}
			item.rect = Rect2(0.0f, y, width, std::max<float>(1.0f, sep_height));
			item.text_rect = item.rect;
			y += item.rect.size.y + static_cast<float>(block_spacing_after);
			items.push_back(item);
			rendered_block_count++;
			continue;
		}

		if (!plain_text.is_empty()) {
			plain_text += "\n\n";
		}
		item.global_start = plain_text.length();
		plain_text += item_text;
		item.global_end = plain_text.length();
		item.paragraph_global_start = item.global_start;
		item.paragraph_global_end = item.global_end;
		item.text = item_text;
		item.spans = item_spans;

		item.paragraph.instantiate();
		configure_paragraph_autowrap(item.paragraph, label);
		const float paragraph_width = std::max<float>(1.0f, width - left_padding - right_padding);
		item.paragraph->set_width(paragraph_width);
		item.paragraph->set_line_spacing(item.type == MARKDOWN_BLOCK_FOOTNOTES ? theme_cache.constant.footnote_line_separation : theme_cache.constant.line_separation);
		if (!add_mermaid_to_paragraph(item.paragraph, mermaid_cache, block, paragraph_width, item.objects)) {
			add_spans_to_paragraph(item.paragraph, item_text, item.spans, theme_cache, font, font_size, item.text_color, item.objects, paragraph_width, this);
		}
		if (item.type == MARKDOWN_BLOCK_LATEX && !item.objects.formulas.is_empty()) {
			item.paragraph->set_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		}

		const Vector2 paragraph_size = item.paragraph->get_size();
		const Vector2 inline_code_padding = get_inline_code_vertical_padding(item.spans, theme_cache);
		top_padding += inline_code_padding.x;
		bottom_padding += inline_code_padding.y;
		if (y > 0.0f) {
			y += static_cast<float>(block_spacing_before);
		}
		item.text_rect = Rect2(left_padding, y + top_padding, std::max<float>(1.0f, width - left_padding - right_padding), paragraph_size.y);
		item.rect = Rect2(0.0f, y, width, paragraph_size.y + top_padding + bottom_padding);

		const int32_t item_index = static_cast<int32_t>(items.size());
		const int32_t paragraph_lines = item.paragraph->get_line_count();
		Vector2 line_position = item.text_rect.position;
		for (int32_t line_index = 0; line_index < paragraph_lines; line_index++) {
			const Vector2i range = item.paragraph->get_line_range(line_index);
			MarkdownCanvasLine line;
			line.item_index = item_index;
			line.paragraph_line = line_index;
			line.global_start = item.paragraph_global_start + range.x;
			line.global_end = item.paragraph_global_start + range.y;
			line.position = line_position;
			line.size = item.paragraph->get_line_size(line_index);
			line_position.y += line.size.y + theme_cache.constant.line_separation;
			lines.push_back(line);
		}
		record_footnote_reference_offsets(item.paragraph, item.text_rect, item.spans);

		if (item.type == MARKDOWN_BLOCK_FOOTNOTES) {
			for (int32_t footnote_index = 0; footnote_index < static_cast<int32_t>(item.footnote_ids.size()); footnote_index++) {
				const String footnote_id = item.footnote_ids[footnote_index];
				if (footnote_index < static_cast<int32_t>(item.footnote_starts.size())) {
					footnote_definition_offsets[footnote_id] = paragraph_y_for_local_character(item.paragraph, item.text_rect, item.footnote_starts[footnote_index]);
				}
			}
		}

		if ((block.type == MARKDOWN_BLOCK_UNORDERED_LIST || block.type == MARKDOWN_BLOCK_ORDERED_LIST || block.type == MARKDOWN_BLOCK_TASK_LIST)) {
			float list_line_y = item.text_rect.position.y;
			for (int32_t marker_index = 0; marker_index < block.items.size(); marker_index++) {
				String marker_text;
				if (block.type == MARKDOWN_BLOCK_ORDERED_LIST) {
					marker_text = String::num_int64(std::max<int32_t>(1, block.list_start) + marker_index) + ".";
				}
				else if (block.type == MARKDOWN_BLOCK_TASK_LIST) {
					marker_text = " ";
				}
				else {
					marker_text = String::chr(0x2022);
				}
				float marker_size_x = 0.0f;
				if (block.type == MARKDOWN_BLOCK_TASK_LIST) {
					marker_size_x = static_cast<float>(theme_cache.font_size.list_marker);
				}
				else if (theme_cache.font.list_marker.is_valid()) {
					const Vector2 ms = theme_cache.font.list_marker->get_string_size(marker_text, HORIZONTAL_ALIGNMENT_RIGHT, static_cast<int32_t>(item.text_rect.position.x - theme_cache.constant.list_marker_gap), theme_cache.font_size.list_marker);
					marker_size_x = ms.x;
				}
				float marker_x = std::max<float>(0.0f, item.text_rect.position.x - static_cast<float>(theme_cache.constant.list_marker_gap) - marker_size_x);
				float marker_y = list_line_y + item.paragraph->get_line_ascent(0);

				MarkdownListMarker marker;
				marker.text = marker_text;
				marker.position = Vector2(std::max<float>(0.0f, marker_x), marker_y);
				marker.font = theme_cache.font.list_marker;
				marker.font_size = theme_cache.font_size.list_marker;
				marker.color = theme_cache.color.list_marker;
				if (block.type == MARKDOWN_BLOCK_TASK_LIST && marker_index < static_cast<int32_t>(block.task_checked.size())) {
					marker.task_checked = block.task_checked[marker_index];
					marker.icon = marker.task_checked ? theme_cache.icon.task_checked : theme_cache.icon.task_unchecked;
				}
				item.list_markers.push_back(marker);
				list_line_y += item.paragraph->get_line_size(0).y + theme_cache.constant.line_separation;
			}
		}

		items.push_back(item);
		y += item.rect.size.y + static_cast<float>(block_spacing_after);
		rendered_block_count++;
	}

	// Index anchors from the final layout, including blocks retained by streaming updates.
	const auto add_anchor = [&](const String& p_id, float p_offset) {
		if (!p_id.is_empty() && !anchor_offsets.has(p_id)) {
			anchor_offsets[p_id] = std::max(0.0f, p_offset);
		}
	};
	const auto add_html_anchors = [&](const Ref<HTMLLayout>& p_layout, const Rect2& p_rect) {
		if (p_layout.is_valid()) {
			for (const auto& anchor : p_layout->get_anchor_rects()) {
				add_anchor(anchor.key, p_rect.position.y + anchor.value.position.y);
			}
		}
	};
	for (const MarkdownCanvasItem& item : items) {
		add_anchor(item.anchor, item.rect.position.y);
		add_html_anchors(item.html_layout, item.text_rect);
		for (const MarkdownContainerLine& line : item.container_lines) {
			add_anchor(line.anchor, line.rect.position.y);
			add_html_anchors(line.html_layout, line.text_rect);
		}
	}

	content_height = std::max<float>(1.0f, y);
	set_custom_minimum_size(Vector2(1.0, content_height));
	update_minimum_size();

	const int64_t text_length = plain_text.length();
	selection_anchor = std::min<int64_t>(selection_anchor, text_length);
	selection_caret = std::min<int64_t>(selection_caret, text_length);
	if (search_match_start >= text_length || search_match_end > text_length || search_match_end <= search_match_start) {
		search_match_start = -1;
		search_match_end = -1;
	}
	layout_prefix_reuse_requested = false;
	update_internal_processing();
	if (label != nullptr) {
		label->notify_canvas_layout_changed();
	}
}

double MarkdownLabelCanvas::get_offset_for_source_line(double p_line, int32_t p_line_count) {
	rebuild_layout();
	if (items.empty() || p_line <= 0.0) {
		return 0.0;
	}
	const auto next = std::upper_bound(items.begin(), items.end(), p_line, [](double p_line, const MarkdownCanvasItem& p_item) {
		return p_line < p_item.source_line;
		});
	const MarkdownCanvasItem* previous = next == items.begin() ? nullptr : &*(next - 1);
	const double start_line = previous ? previous->source_line : 0.0;
	const double start_offset = previous ? previous->rect.position.y : 0.0;
	const double end_line = next == items.end() ? p_line_count : next->source_line;
	const double end_offset = next == items.end() ? content_height : next->rect.position.y;
	if (end_line <= start_line) {
		return start_offset;
	}
	const double fraction = Math::clamp((p_line - start_line) / (end_line - start_line), 0.0, 1.0);
	return Math::lerp(start_offset, end_offset, fraction);
}

double MarkdownLabelCanvas::get_source_line_at_offset(double p_offset, int32_t p_line_count) {
	rebuild_layout();
	if (items.empty() || p_offset <= 0.0) {
		return 0.0;
	}
	const auto next = std::upper_bound(items.begin(), items.end(), p_offset, [](double p_offset, const MarkdownCanvasItem& p_item) {
		return p_offset < p_item.rect.position.y;
		});
	const MarkdownCanvasItem* previous = next == items.begin() ? nullptr : &*(next - 1);
	const double start_line = previous ? previous->source_line : 0.0;
	const double start_offset = previous ? previous->rect.position.y : 0.0;
	const double end_line = next == items.end() ? p_line_count : next->source_line;
	const double end_offset = next == items.end() ? content_height : next->rect.position.y;
	if (end_offset <= start_offset) {
		return start_line;
	}
	const double fraction = Math::clamp((p_offset - start_offset) / (end_offset - start_offset), 0.0, 1.0);
	return Math::lerp(start_line, end_line, fraction);
}

void MarkdownLabelCanvas::draw_selection_for_item(const MarkdownCanvasItem& p_item) {
	if (p_item.type == MARKDOWN_BLOCK_TABLE) {
		for (const MarkdownTableCell& cell : p_item.cells) {
			draw_selection_for_paragraph(cell.paragraph, cell.text_rect, cell.global_start, cell.global_end);
		}
		return;
	}
	if (!p_item.container_lines.empty() || p_item.type == MARKDOWN_BLOCK_BLOCKQUOTE) {
		for (const MarkdownContainerLine& line : p_item.container_lines) {
			draw_selection_for_paragraph(line.paragraph, line.text_rect, line.global_start, line.global_end);
		}
		return;
	}
	draw_selection_for_paragraph(p_item.paragraph, p_item.text_rect, p_item.paragraph_global_start, p_item.paragraph_global_end);
}
void MarkdownLabelCanvas::draw_search_highlight_for_item(const MarkdownCanvasItem& p_item) {
	if (search_match_start < 0 || search_match_end <= search_match_start) {
		return;
	}

	if (p_item.type == MARKDOWN_BLOCK_TABLE) {
		for (const MarkdownTableCell& cell : p_item.cells) {
			if (search_match_start < cell.global_end && search_match_end > cell.global_start) {
				draw_range_background_for_paragraph(cell.paragraph, cell.text_rect, cell.global_start, cell.global_end, search_match_start, search_match_end, theme_cache.stylebox.search_result);
			}
		}
		return;
	}
	if (!p_item.container_lines.empty() || p_item.type == MARKDOWN_BLOCK_BLOCKQUOTE) {
		for (const MarkdownContainerLine& line : p_item.container_lines) {
			if (search_match_start < line.global_end && search_match_end > line.global_start) {
				draw_range_background_for_paragraph(line.paragraph, line.text_rect, line.global_start, line.global_end, search_match_start, search_match_end, theme_cache.stylebox.search_result);
			}
		}
		return;
	}
	draw_range_background_for_paragraph(p_item.paragraph, p_item.text_rect, p_item.paragraph_global_start, p_item.paragraph_global_end, search_match_start, search_match_end, theme_cache.stylebox.search_result);
}


static float get_alignment_offset(const Ref<TextParagraph>& p_paragraph, int32_t p_line_index, float p_available_width) {
	const HorizontalAlignment align = p_paragraph->get_alignment();
	if (align == HORIZONTAL_ALIGNMENT_LEFT || align == HORIZONTAL_ALIGNMENT_FILL) {
		return 0.0f;
	}
	const float line_width = p_paragraph->get_line_size(p_line_index).x;
	const float extra = p_available_width - line_width;
	if (extra <= 0.0f) {
		return 0.0f;
	}
	if (align == HORIZONTAL_ALIGNMENT_CENTER) {
		return extra / 2.0f;
	}
	return extra;
}
void MarkdownLabelCanvas::draw_range_background_for_paragraph(const Ref<TextParagraph>& p_paragraph, const Rect2& p_text_rect, int64_t p_global_start, int64_t p_global_end, int64_t p_range_start, int64_t p_range_end, const Ref<StyleBox>& p_stylebox) {
	if (p_paragraph.is_null()) {
		return;
	}

	TextServerManager* text_server_manager = TextServerManager::get_singleton();
	if (text_server_manager == nullptr) {
		return;
	}
	const Ref<TextServer> text_server = text_server_manager->get_primary_interface();
	if (text_server.is_null()) {
		return;
	}

	if (p_range_end <= p_global_start || p_range_start >= p_global_end) {
		return;
	}

	float y = p_text_rect.position.y;
	for (int32_t line_index = 0; line_index < p_paragraph->get_line_count(); line_index++) {
		const Vector2i range = p_paragraph->get_line_range(line_index);
		const int64_t local_from = std::max<int64_t>(range.x, p_range_start - p_global_start);
		const int64_t local_to = std::min<int64_t>(range.y, p_range_end - p_global_start);
		const Vector2 line_size = p_paragraph->get_line_size(line_index);
		const float x_base = p_text_rect.position.x + get_alignment_offset(p_paragraph, line_index, p_text_rect.size.x);
		if (local_from < local_to) {
			const PackedVector2Array selected_ranges = text_server->shaped_text_get_selection(p_paragraph->get_line_rid(line_index), local_from, local_to);
			for (int32_t range_index = 0; range_index < selected_ranges.size(); range_index++) {
				const Vector2 selected_range = selected_ranges[range_index];
				draw_stylebox_or_rect(p_stylebox, Rect2(x_base + selected_range.x, y, selected_range.y - selected_range.x, line_size.y), Color());
			}
		}
		y += line_size.y + p_paragraph->get_line_spacing();
	}
}

void MarkdownLabelCanvas::draw_selection_for_paragraph(const Ref<TextParagraph>& p_paragraph, const Rect2& p_text_rect, int64_t p_global_start, int64_t p_global_end) {
	if (!has_selection()) {
		return;
	}

	const int64_t selection_from = std::min<int64_t>(selection_anchor, selection_caret);
	const int64_t selection_to = std::max<int64_t>(selection_anchor, selection_caret);
	draw_range_background_for_paragraph(p_paragraph, p_text_rect, p_global_start, p_global_end, selection_from, selection_to, theme_cache.stylebox.selection);
}

void MarkdownLabelCanvas::draw_stylebox_or_rect(const Ref<StyleBox>& p_stylebox, const Rect2& p_rect, const Color& p_fallback_color) {
	if (p_stylebox.is_valid()) {
		p_stylebox->draw(get_canvas_item(), p_rect);
		return;
	}

	if (p_fallback_color.a > 0.0f) {
		draw_rect(p_rect, p_fallback_color, true);
	}
}

void MarkdownLabelCanvas::draw_paragraph_objects(const Ref<TextParagraph>& p_paragraph, const Rect2& p_text_rect, const MarkdownParagraphObjects& p_objects) {
	if (p_paragraph.is_null() || (p_objects.images.is_empty() && p_objects.formulas.is_empty())) {
		return;
	}

	for (int32_t line_index = 0; line_index < p_paragraph->get_line_count(); line_index++) {
		const Array objects = p_paragraph->get_line_objects(line_index);
		for (int32_t object_index = 0; object_index < objects.size(); object_index++) {
			const String object_name = objects[object_index];
			if (const Ref<TeXRender>* formula = p_objects.formulas.getptr(object_name)) {
				const Rect2 object_rect = p_paragraph->get_line_object_rect(line_index, object_name);
				(*formula)->draw_oversampled(this, p_text_rect.position + object_rect.position);
				continue;
			}

			const Ref<Texture2D>* texture = p_objects.images.getptr(object_name);
			if (texture == nullptr || texture->is_null()) {
				continue;
			}

			const Rect2 object_rect = p_paragraph->get_line_object_rect(line_index, object_name);
			(*texture)->draw_rect(get_canvas_item(), Rect2(p_text_rect.position + object_rect.position, object_rect.size), false);
		}
	}
}


void MarkdownLabelCanvas::draw_span_decorations(const Ref<TextParagraph>& p_paragraph, const Rect2& p_text_rect, const std::vector<MarkdownInlineSpan>& p_spans, int64_t p_global_start, bool p_draw_code_backgrounds, bool p_draw_link_underlines) {
	if (p_paragraph.is_null() || p_spans.empty()) {
		return;
	}

	TextServerManager* text_server_manager = TextServerManager::get_singleton();
	if (text_server_manager == nullptr) {
		return;
	}
	const Ref<TextServer> text_server = text_server_manager->get_primary_interface();
	if (text_server.is_null()) {
		return;
	}

	const Color& link_color = theme_cache.color.link;
	const float decoration_line_width = std::max<float>(1.0f, theme_cache.base_scale);

	for (const MarkdownInlineSpan& span : p_spans) {
		if (!(p_draw_code_backgrounds && (span.custom_background || span.code || span.highlight || (span.footnote_ref && theme_cache.stylebox.footnote_ref.is_valid()))) && !(p_draw_link_underlines && (!span.link_uri.is_empty() || span.strikethrough))) {
			continue;
		}

		float y = p_text_rect.position.y;
		for (int32_t line_index = 0; line_index < p_paragraph->get_line_count(); line_index++) {
			const float align_offset = get_alignment_offset(p_paragraph, line_index, p_text_rect.size.x);
			const float x_base = p_text_rect.position.x + align_offset;
			const Vector2i range = p_paragraph->get_line_range(line_index);
			const int64_t local_from = std::max<int64_t>(range.x, span.start);
			const int64_t local_to = std::min<int64_t>(range.y, span.end);
			const Vector2 line_size = p_paragraph->get_line_size(line_index);
			if (local_from < local_to) {
				const PackedVector2Array selected_ranges = text_server->shaped_text_get_selection(p_paragraph->get_line_rid(line_index), local_from, local_to);
				for (int32_t range_index = 0; range_index < selected_ranges.size(); range_index++) {
					const Vector2 selected_range = selected_ranges[range_index];
					if (p_draw_code_backgrounds && span.custom_background) {
						draw_rect(Rect2(x_base + selected_range.x, y, selected_range.y - selected_range.x, line_size.y), span.background_color);
					}
					if (p_draw_code_backgrounds && span.highlight && !span.custom_background) {
						const float hl_top = get_stylebox_margin_or(theme_cache.stylebox.highlight, SIDE_TOP, 1.0f);
						const float hl_bottom = get_stylebox_margin_or(theme_cache.stylebox.highlight, SIDE_BOTTOM, 1.0f);
						const float hl_left = get_stylebox_margin_or(theme_cache.stylebox.highlight, SIDE_LEFT, 2.0f);
						const float hl_right = get_stylebox_margin_or(theme_cache.stylebox.highlight, SIDE_RIGHT, 2.0f);
						const Rect2 hl_rect = Rect2(x_base + selected_range.x - hl_left, y - hl_top, selected_range.y - selected_range.x + hl_left + hl_right, line_size.y + hl_top + hl_bottom);
						draw_stylebox_or_rect(theme_cache.stylebox.highlight, hl_rect, theme_cache.color.highlight);
					}
					if (p_draw_code_backgrounds && span.code && !span.custom_background) {
						const float margin_left = get_stylebox_margin_or(theme_cache.stylebox.inline_code, SIDE_LEFT, 2.0f);
						const float margin_top = get_stylebox_margin_or(theme_cache.stylebox.inline_code, SIDE_TOP, 1.0f);
						const float margin_right = get_stylebox_margin_or(theme_cache.stylebox.inline_code, SIDE_RIGHT, 2.0f);
						const float margin_bottom = get_stylebox_margin_or(theme_cache.stylebox.inline_code, SIDE_BOTTOM, 1.0f);
						const Rect2 code_rect = Rect2(x_base + selected_range.x - margin_left, y - margin_top, selected_range.y - selected_range.x + margin_left + margin_right, line_size.y + margin_top + margin_bottom);
						draw_stylebox_or_rect(theme_cache.stylebox.inline_code, code_rect, Color());
					}
					if (p_draw_code_backgrounds && span.footnote_ref && theme_cache.stylebox.footnote_ref.is_valid()) {
						const float margin_left = get_stylebox_margin_or(theme_cache.stylebox.footnote_ref, SIDE_LEFT, 1.0f);
						const float margin_top = get_stylebox_margin_or(theme_cache.stylebox.footnote_ref, SIDE_TOP, 0.0f);
						const float margin_right = get_stylebox_margin_or(theme_cache.stylebox.footnote_ref, SIDE_RIGHT, 1.0f);
						const float margin_bottom = get_stylebox_margin_or(theme_cache.stylebox.footnote_ref, SIDE_BOTTOM, 0.0f);
						const float ref_offset = static_cast<float>(theme_cache.font_size.footnote_ref) * 0.35f;
						const Rect2 ref_rect = Rect2(x_base + selected_range.x - margin_left, y - ref_offset - margin_top, selected_range.y - selected_range.x + margin_left + margin_right, line_size.y + margin_top + margin_bottom);
						draw_stylebox_or_rect(theme_cache.stylebox.footnote_ref, ref_rect, Color());
					}
					if (p_draw_link_underlines && span.strikethrough) {
						const float strike_y = y + line_size.y * 0.5f;
						draw_line(Vector2(x_base + selected_range.x, strike_y), Vector2(x_base + selected_range.y, strike_y), theme_cache.color.strikethrough, decoration_line_width);
					}
					if (p_draw_link_underlines && !span.link_uri.is_empty()) {
						const float underline_y = y + line_size.y - scale_theme_fallback(2.0f, theme_cache.base_scale);
						draw_line(Vector2(x_base + selected_range.x, underline_y), Vector2(x_base + selected_range.y, underline_y),
							span.custom_color ? span.color : link_color, decoration_line_width);
					}
				}
			}
			y += line_size.y + p_paragraph->get_line_spacing();
		}
	}
}

void MarkdownLabelCanvas::draw_paragraph_text(const Ref<TextParagraph>& p_paragraph, const Rect2& p_text_rect, const std::vector<MarkdownInlineSpan>& p_spans, int64_t p_global_start, const Color& p_default_color) {
	if (p_paragraph.is_null()) {
		return;
	}

	TextServerManager* text_server_manager = TextServerManager::get_singleton();
	if (text_server_manager == nullptr) {
		return;
	}
	const Ref<TextServer> text_server = text_server_manager->get_primary_interface();
	if (text_server.is_null()) {
		return;
	}

	const RID canvas = get_canvas_item();
	float y = p_text_rect.position.y;
	for (int32_t line_index = 0; line_index < p_paragraph->get_line_count(); line_index++) {
		const RID line_rid = p_paragraph->get_line_rid(line_index);
		const float align_offset = get_alignment_offset(p_paragraph, line_index, p_text_rect.size.x);
		const Vector2 line_position = Vector2(p_text_rect.position.x + align_offset, y + p_paragraph->get_line_ascent(line_index));
		const Vector2i range = p_paragraph->get_line_range(line_index);
		if (p_spans.empty()) {
			text_server->shaped_text_draw(line_rid, canvas, line_position, -1, -1, p_default_color);
		}
		else {
			for (const MarkdownInlineSpan& span : p_spans) {
				const Color span_color = get_span_color(span, theme_cache, p_default_color);
				const int64_t local_from = std::max<int64_t>(range.x, span.start);
				const int64_t local_to = std::min<int64_t>(range.y, span.end);
				if (local_from >= local_to) {
					continue;
				}
				const PackedVector2Array selected_ranges = text_server->shaped_text_get_selection(line_rid, local_from, local_to);
				for (int32_t range_index = 0; range_index < selected_ranges.size(); range_index++) {
					const Vector2 selected_range = selected_ranges[range_index];
					Vector2 draw_position = line_position;
					if (span.footnote_ref) {
						draw_position.y -= static_cast<float>(theme_cache.font_size.footnote_ref) * 0.35f;
					}
					text_server->shaped_text_draw(line_rid, canvas, draw_position, selected_range.x, selected_range.y, span_color);
				}
			}
		}
		y += p_paragraph->get_line_size(line_index).y + p_paragraph->get_line_spacing();
	}
}

Rect2 MarkdownLabelCanvas::get_visible_content_rect() const {
	const float width = std::max<float>(1.0f, get_size().x);
	if (label == nullptr) {
		return Rect2(0.0f, 0.0f, width, content_height);
	}

	VScrollBar* v_scroll = label->get_v_scroll_bar();
	const float visible_y = v_scroll == nullptr ? 0.0f : static_cast<float>(v_scroll->get_value());
	const float visible_height = std::max<float>(1.0f, label->get_size().y);
	const float buffer = 256.0f;
	return Rect2(0.0f, std::max<float>(0.0f, visible_y - buffer), width, visible_height + buffer * 2.0f);
}

Rect2 MarkdownLabelCanvas::get_html_visible_rect() const {
	const Rect2 bounds(Vector2(), get_size());
	if (!is_inside_tree() || label == nullptr || label->scroll_container == nullptr) {
		return bounds;
	}
	const Transform2D transform = get_global_transform_with_canvas().affine_inverse() * label->scroll_container->get_global_transform_with_canvas();
	return bounds.intersection(transform.xform(Rect2(Vector2(), label->scroll_container->get_size())));
}

void MarkdownLabelCanvas::_draw() {
	if (label == nullptr || !label->is_parsing_html()) {
		return;
	}
	const Rect2 visible = get_html_visible_rect();
	const int64_t selection_from = std::min(selection_anchor, selection_caret);
	const int64_t selection_to = std::max(selection_anchor, selection_caret);
	auto draw_html = [&](const Ref<HTMLLayout>& p_layout, const Rect2& p_rect, int64_t p_from, int64_t p_to) {
		if (p_layout.is_valid()) {
			const Rect2 clip = visible.intersection(p_rect);
			if (clip.has_area()) {
				const auto local_range = [&](int64_t p_start, int64_t p_end) {
					return Vector2i(CLAMP(p_start, p_from, p_to) - p_from, CLAMP(p_end, p_from, p_to) - p_from);
				};
				p_layout->draw_with_highlights(this, p_rect.position, clip, local_range(selection_from, selection_to),
					theme_cache.stylebox.selection.ptr(), theme_cache.color.selection_color,
					local_range(search_match_start, search_match_end), theme_cache.stylebox.search_result.ptr());
			}
		}
	};
	for (const MarkdownCanvasItem& item : items) {
		if (item.rect.position.y > visible.get_end().y) {
			break;
		}
		if (item.rect.get_end().y < visible.position.y) {
			continue;
		}
		draw_html(item.html_layout, item.text_rect, item.global_start, item.global_end);
		for (const MarkdownContainerLine& line : item.container_lines) {
			draw_html(line.html_layout, line.text_rect, line.global_start, line.global_end);
		}
	}
}

bool MarkdownLabelCanvas::is_rect_visible(const Rect2& p_rect, const Rect2& p_visible_rect) const {
	return p_rect.intersects(p_visible_rect) || p_visible_rect.encloses(p_rect) || p_rect.encloses(p_visible_rect);
}

void MarkdownLabelCanvas::_notification(int p_what) {
	if (p_what == NOTIFICATION_THEME_CHANGED) {
		layout_prefix_reuse_requested = false;
	}
	if (p_what == NOTIFICATION_RESIZED || p_what == NOTIFICATION_THEME_CHANGED) {
		mark_layout_dirty();
	}

	if (p_what == NOTIFICATION_PREDELETE) {
		s_image_watchers.clear();
		s_pending_local_image_uris.clear();
		s_image_cache.clear();
	}

	if (p_what == NOTIFICATION_MOUSE_EXIT) {
		pressed_html_link = 0;
	}

	if (p_what == NOTIFICATION_FOCUS_EXIT || p_what == NOTIFICATION_EXIT_TREE) {
		pressed_html_link = 0;
		pressed_html_layout.unref();
		dragging_selection = false;
	}

	if (p_what == NOTIFICATION_FOCUS_EXIT) {
		if (label != nullptr && label->is_deselect_on_focus_loss_enabled()) {
			clear_selection();
		}
		selection_drag_attempt = false;
	}

	if (p_what == NOTIFICATION_DRAG_END) {
		selection_drag_attempt = false;
	}

	if (p_what == NOTIFICATION_INTERNAL_PROCESS) {
		const double delta = get_process_delta_time();
		if (mermaid_cache.poll()) {
			layout_prefix_reuse_requested = false;
			mark_layout_dirty();
		}
		if (pending_stream_parse) {
			pending_layout_elapsed_msec += delta * 1000.0;
			if (pending_layout_elapsed_msec >= static_cast<double>(layout_flush_interval_msec)) {
				flush_pending_layout();
			}
		}

		const bool has_pending_images = process_pending_local_images();
		const bool has_animated_gifs = update_gif_animations(delta);
		const bool should_keep_processing = pending_stream_parse || mermaid_cache.has_pending() || has_pending_images || has_animated_gifs;
		if (!dragging_selection || !selection_dragged || label == nullptr) {
			internal_processing_enabled = should_keep_processing;
			set_process_internal(internal_processing_enabled);
			return;
		}

		VScrollBar* v_scroll = label->get_v_scroll_bar();
		if (v_scroll == nullptr) {
			internal_processing_enabled = should_keep_processing;
			set_process_internal(internal_processing_enabled);
			return;
		}

		const Size2 label_size = label->get_size();
		const Vector2 label_mouse = label->get_local_mouse_position();

		const float scroll_margin = 24.0f;
		float scroll_delta = 0.0f;
		const float above = scroll_margin - label_mouse.y;
		const float below = label_mouse.y - (label_size.y - scroll_margin);

		if (above > 0.0f) {
			const float base_speed = 80.0f;
			scroll_delta = -base_speed * (above / scroll_margin) * static_cast<float>(delta);
		}
		else if (below > 0.0f) {
			const float base_speed = 80.0f;
			scroll_delta = base_speed * (below / scroll_margin) * static_cast<float>(delta);
		}

		if (scroll_delta != 0.0f) {
			const float max_scroll = v_scroll->get_max() - v_scroll->get_page();
			const float new_value = std::max<float>(0.0f, std::min<float>(max_scroll, v_scroll->get_value() + scroll_delta));
			v_scroll->set_value(new_value);

			const Vector2 canvas_mouse = get_local_mouse_position();
			selection_caret = hit_test_document(canvas_mouse);
			queue_redraw();
		}

		internal_processing_enabled = true;
		set_process_internal(true);
		return;
	}

	if (p_what != NOTIFICATION_DRAW) {
		return;
	}

	rebuild_layout();
	const Rect2 visible_rect = get_visible_content_rect();

	for (const MarkdownCanvasItem& item : items) {
		if (!is_rect_visible(item.rect, visible_rect)) {
			continue;
		}
		if (!item.container_lines.empty() || item.type == MARKDOWN_BLOCK_BLOCKQUOTE) {
			const bool root_quote = item.type == MARKDOWN_BLOCK_BLOCKQUOTE;
			if (root_quote) {
				draw_stylebox_or_rect(item.panel_stylebox, item.rect, item.background_color);
			}

			const float quote_indent = static_cast<float>(std::max<int32_t>(12, get_theme_constant_or(label, "MarkdownLabel", "blockquote_indent", 25)));
			const float panel_left = root_quote ? get_stylebox_margin_or(item.panel_stylebox, SIDE_LEFT, 28.0f) : 0.0f;
			const float panel_right = root_quote ? get_stylebox_margin_or(item.panel_stylebox, SIDE_RIGHT, 8.0f) : 0.0f;
			const float nested_top = get_stylebox_margin_or(theme_cache.stylebox.blockquote_nested, SIDE_TOP, 0.0f);
			const float nested_bottom = get_stylebox_margin_or(theme_cache.stylebox.blockquote_nested, SIDE_BOTTOM, 0.0f);

			const int32_t first_nested_depth = root_quote ? 2 : 1;
			for (int32_t depth = first_nested_depth; depth <= item.blockquote_depth; depth++) {
				int32_t run_start = -1;
				float run_top = 0.0f;
				float run_bottom = 0.0f;

				for (int32_t line_index = 0; line_index <= static_cast<int32_t>(item.container_lines.size()); line_index++) {
					const bool line_in_run = line_index < static_cast<int32_t>(item.container_lines.size()) && item.container_lines[line_index].depth >= depth;
					if (line_in_run && run_start < 0) {
						run_start = line_index;
						run_top = item.container_lines[line_index].rect.position.y - nested_top;
						run_bottom = item.container_lines[line_index].rect.get_end().y + nested_bottom;
					}
					else if (line_in_run) {
						run_bottom = item.container_lines[line_index].rect.get_end().y + nested_bottom;
					}
					else if (run_start >= 0) {
						const float nested_rect_left = item.rect.position.x + panel_left + static_cast<float>(depth - first_nested_depth) * quote_indent;
						const float nested_rect_width = std::max<float>(1.0f, item.rect.get_end().x - nested_rect_left - panel_right);
						const Rect2 nested_rect = Rect2(nested_rect_left, run_top, nested_rect_width, std::max<float>(1.0f, run_bottom - run_top));
						draw_stylebox_or_rect(theme_cache.stylebox.blockquote_nested, nested_rect, Color());
						run_start = -1;
					}
				}
			}

			for (const MarkdownContainerLine& line : item.container_lines) {
				if (!is_rect_visible(line.rect, visible_rect)) {
					continue;
				}
				if (line.stylebox.is_valid()) {
					draw_stylebox_or_rect(line.stylebox, line.rect, Color());
				}
				for (const MarkdownListMarker& marker : line.list_markers) {
					if (marker.icon.is_valid()) {
						const float icon_size = static_cast<float>(marker.font_size);
						const float icon_y = marker.position.y - icon_size * 0.85f;
						marker.icon->draw_rect(get_canvas_item(), Rect2(marker.position.x, icon_y, icon_size, icon_size), false);
					}
					if (marker.font.is_valid()) {
						marker.font->draw_string(get_canvas_item(), marker.position, marker.text, HORIZONTAL_ALIGNMENT_LEFT, -1, marker.font_size, marker.color);
					}
				}
				draw_span_decorations(line.paragraph, line.text_rect, line.spans, line.global_start, true, false);
				if (search_match_start >= 0 && search_match_start < line.global_end && search_match_end > line.global_start) {
					draw_range_background_for_paragraph(line.paragraph, line.text_rect, line.global_start, line.global_end, search_match_start, search_match_end, theme_cache.stylebox.search_result);
				}
				draw_selection_for_paragraph(line.paragraph, line.text_rect, line.global_start, line.global_end);
				draw_paragraph_text(line.paragraph, line.text_rect, line.spans, line.global_start, line.text_color);
				draw_paragraph_objects(line.paragraph, line.text_rect, line.objects);
				draw_span_decorations(line.paragraph, line.text_rect, line.spans, line.global_start, false, true);
			}
			continue;
		}

		if (item.type == MARKDOWN_BLOCK_TABLE) {
			draw_stylebox_or_rect(item.panel_stylebox, item.rect, Color());
			for (const MarkdownTableCell& cell : item.cells) {
				if (!is_rect_visible(cell.rect, visible_rect)) {
					continue;
				}
				draw_stylebox_or_rect(cell.stylebox, cell.rect, Color(0, 0, 0, 0));
				draw_span_decorations(cell.paragraph, cell.text_rect, cell.spans, cell.global_start, true, false);
				if (search_match_start >= 0 && search_match_start < cell.global_end && search_match_end > cell.global_start) {
					draw_range_background_for_paragraph(cell.paragraph, cell.text_rect, cell.global_start, cell.global_end, search_match_start, search_match_end, theme_cache.stylebox.search_result);
				}
				draw_selection_for_paragraph(cell.paragraph, cell.text_rect, cell.global_start, cell.global_end);
				draw_paragraph_text(cell.paragraph, cell.text_rect, cell.spans, cell.global_start, cell.text_color);
				draw_paragraph_objects(cell.paragraph, cell.text_rect, cell.objects);
				draw_span_decorations(cell.paragraph, cell.text_rect, cell.spans, cell.global_start, false, true);
			}
		}
		else if (item.framed) {
			draw_stylebox_or_rect(item.panel_stylebox, item.rect, item.background_color);
		}

		if (item.type != MARKDOWN_BLOCK_TABLE && item.type != MARKDOWN_BLOCK_THEMATIC_BREAK) {
			draw_span_decorations(item.paragraph, item.text_rect, item.spans, item.paragraph_global_start, true, false);
			draw_search_highlight_for_item(item);
			draw_selection_for_item(item);
		}

		const RID canvas_item_rid = get_canvas_item();
		for (const MarkdownListMarker& marker : item.list_markers) {
			if (marker.icon.is_valid()) {
				const float icon_size = static_cast<float>(marker.font_size);
				const float icon_y = marker.position.y - icon_size * 0.85f;
				marker.icon->draw_rect(canvas_item_rid, Rect2(marker.position.x, icon_y, icon_size, icon_size), false);
			}
			if (marker.font.is_valid()) {
				marker.font->draw_string(canvas_item_rid, marker.position, marker.text, HORIZONTAL_ALIGNMENT_LEFT, -1, marker.font_size, marker.color);
			}
		}

		if (item.type != MARKDOWN_BLOCK_THEMATIC_BREAK) {
			draw_paragraph_text(item.paragraph, item.text_rect, item.spans, item.paragraph_global_start, item.text_color);
			draw_paragraph_objects(item.paragraph, item.text_rect, item.objects);
			draw_span_decorations(item.paragraph, item.text_rect, item.spans, item.paragraph_global_start, false, true);
		}
	}
}

int64_t MarkdownLabelCanvas::hit_test_document(const Vector2& p_position) const {
	if (items.empty()) {
		return 0;
	}

	if (p_position.y < items.front().rect.position.y) {
		return items.front().global_start;
	}
	if (p_position.y > items.back().rect.get_end().y) {
		return plain_text.length();
	}

	int32_t low = 0;
	int32_t high = static_cast<int32_t>(items.size()) - 1;
	int32_t item_index = high;
	while (low <= high) {
		const int32_t middle = low + (high - low) / 2;
		if (p_position.y < items[middle].rect.position.y) {
			item_index = middle;
			high = middle - 1;
		}
		else if (p_position.y > items[middle].rect.get_end().y) {
			low = middle + 1;
		}
		else {
			item_index = middle;
			break;
		}
	}

	const MarkdownCanvasItem& item = items[item_index];
	if (p_position.y < item.rect.position.y) {
		return item.global_start;
	}
	if (p_position.y <= item.rect.get_end().y) {
		if (item.html_layout.is_valid()) {
			return item.global_start + item.html_layout->hit_test_text(p_position - item.text_rect.position);
		}
		if (item.type == MARKDOWN_BLOCK_TABLE) {
			for (const MarkdownTableCell& cell : item.cells) {
				if (cell.rect.has_point(p_position)) {
					Vector2 local_position = p_position - cell.text_rect.position;
					if (!cell.paragraph.is_null() && cell.paragraph->get_line_count() > 0) {
						float line_y = 0.0f;
						for (int32_t li = 0; li < cell.paragraph->get_line_count(); li++) {
							const float line_h = cell.paragraph->get_line_size(li).y + cell.paragraph->get_line_spacing();
							if (local_position.y < line_y + line_h || li == cell.paragraph->get_line_count() - 1) {
								local_position.x -= get_alignment_offset(cell.paragraph, li, cell.text_rect.size.x);
								break;
							}
							line_y += line_h;
						}
					}
					const int32_t local_character = std::min<int32_t>(std::max<int32_t>(cell.paragraph->hit_test(local_position), 0), static_cast<int32_t>(cell.global_end - cell.global_start));
					return cell.global_start + local_character;
				}
			}
			return item.cells.empty() ? item.global_start : item.cells[0].global_start;
		}
		if (!item.container_lines.empty() || item.type == MARKDOWN_BLOCK_BLOCKQUOTE) {
			for (const MarkdownContainerLine& line : item.container_lines) {
				if (p_position.y <= line.rect.get_end().y) {
					if (line.html_layout.is_valid()) {
						return line.global_start + line.html_layout->hit_test_text(p_position - line.text_rect.position);
					}
					if (line.paragraph.is_null()) {
						return p_position.x < line.rect.get_center().x ? line.global_start : line.global_end;
					}
					const Vector2 local_position = p_position - line.text_rect.position;
					const int32_t local_character = std::min<int32_t>(std::max<int32_t>(line.paragraph->hit_test(local_position), 0), static_cast<int32_t>(line.global_end - line.global_start));
					return line.global_start + local_character;
				}
			}
			return p_position.x < item.rect.get_center().x ? item.global_start : item.global_end;
		}
		if (!item.paragraph.is_valid()) {
			return p_position.x < item.rect.get_center().x ? item.global_start : item.global_end;
		}
		const Vector2 local_position = p_position - item.text_rect.position;
		const int32_t local_character = std::min<int32_t>(std::max<int32_t>(item.paragraph->hit_test(local_position), 0), static_cast<int32_t>(item.paragraph_global_end - item.paragraph_global_start));
		return item.paragraph_global_start + local_character;
	}

	return plain_text.length();
}

String MarkdownLabelCanvas::link_uri_at_position(const Vector2& p_position) const {
	Vector2 local_position;
	const Ref<HTMLLayout> html = html_at_position(p_position, local_position);
	if (html.is_valid()) {
		return get_html_visible_rect().has_point(p_position) ? html->get_link_at(local_position) : String();
	}
	return link_uri_at_character(hit_test_document(p_position));
}

Ref<HTMLLayout> MarkdownLabelCanvas::html_at_position(const Vector2& p_position, Vector2& r_local_position) const {
	if (label == nullptr || !label->is_parsing_html()) {
		return {};
	}
	for (const MarkdownCanvasItem& item : items) {
		if (item.rect.position.y > p_position.y) {
			break;
		}
		if (!item.rect.has_point(p_position)) {
			continue;
		}
		if (item.html_layout.is_valid() && item.text_rect.has_point(p_position)) {
			r_local_position = p_position - item.text_rect.position;
			return item.html_layout;
		}
		for (const MarkdownContainerLine& line : item.container_lines) {
			if (line.html_layout.is_valid() && line.text_rect.has_point(p_position)) {
				r_local_position = p_position - line.text_rect.position;
				return line.html_layout;
			}
		}
		break;
	}
	return {};
}

int64_t MarkdownLabelCanvas::line_column_to_offset(int32_t p_line, int32_t p_column) const {
	const int32_t target_line = std::max<int32_t>(0, p_line);
	const int32_t target_column = std::max<int32_t>(0, p_column);
	int32_t current_line = 0;
	int32_t current_column = 0;

	for (int64_t index = 0; index < plain_text.length(); index++) {
		if (current_line == target_line && current_column >= target_column) {
			return index;
		}

		if (plain_text[index] == '\n') {
			if (current_line == target_line) {
				return index;
			}
			current_line++;
			current_column = 0;
		}
		else {
			current_column++;
		}
	}

	return plain_text.length();
}

Vector2i MarkdownLabelCanvas::offset_to_line_column(int64_t p_offset) const {
	const int64_t target_offset = std::max<int64_t>(0, std::min<int64_t>(p_offset, plain_text.length()));
	int32_t line = 0;
	int32_t column = 0;

	for (int64_t index = 0; index < target_offset; index++) {
		if (plain_text[index] == '\n') {
			line++;
			column = 0;
		}
		else {
			column++;
		}
	}

	return Vector2i(line, column);
}

String MarkdownLabelCanvas::link_uri_at_character(int64_t p_character) const {
	for (const MarkdownCanvasItem& item : items) {
		if (p_character < item.global_start || p_character > item.global_end) {
			continue;
		}
		if (item.type == MARKDOWN_BLOCK_TABLE) {
			for (const MarkdownTableCell& cell : item.cells) {
				if (p_character >= cell.global_start && p_character <= cell.global_end) {
					const int64_t local_character = p_character - cell.global_start;
					for (const MarkdownInlineSpan& span : cell.spans) {
						if (!span.link_uri.is_empty() && local_character >= span.start && local_character <= span.end) {
							return span.link_uri;
						}
					}
					return String();
				}
			}
			continue;
		}
		if (!item.container_lines.empty() || item.type == MARKDOWN_BLOCK_BLOCKQUOTE) {
			for (const MarkdownContainerLine& line : item.container_lines) {
				if (p_character >= line.global_start && p_character <= line.global_end) {
					const int64_t local_character = p_character - line.global_start;
					for (const MarkdownInlineSpan& span : line.spans) {
						if (!span.link_uri.is_empty() && local_character >= span.start && local_character <= span.end) {
							return span.link_uri;
						}
					}
					return String();
				}
			}
			continue;
		}
		if (p_character >= item.paragraph_global_start && p_character <= item.paragraph_global_end) {
			const int64_t local_character = p_character - item.paragraph_global_start;
			for (const MarkdownInlineSpan& span : item.spans) {
				if (!span.link_uri.is_empty() && local_character >= span.start && local_character <= span.end) {
					return span.link_uri;
				}
			}
		}
	}

	return String();
}

String MarkdownLabelCanvas::image_tooltip_at_position(const Vector2& p_position) const {
	const int64_t ch = hit_test_document(p_position);
	for (const MarkdownCanvasItem& item : items) {
		if (ch >= item.global_start && ch <= item.global_end) {
			if (ch >= item.paragraph_global_start && ch <= item.paragraph_global_end) {
				const int64_t local_ch = ch - item.paragraph_global_start;
				for (const MarkdownInlineSpan& span : item.spans) {
					if (!span.image_uri.is_empty() && local_ch >= span.start && local_ch <= span.end) {
						if (!span.image_title.is_empty()) return span.image_title;
						return String();
					}
				}
			}
		}
	}
	return String();
}

String MarkdownLabelCanvas::footnote_id_at_character(int64_t p_character, bool p_reference) const {
	for (const MarkdownCanvasItem& item : items) {
		if (p_character < item.global_start || p_character > item.global_end) {
			continue;
		}

		if (!p_reference && item.type == MARKDOWN_BLOCK_FOOTNOTES) {
			for (int32_t footnote_index = 0; footnote_index < static_cast<int32_t>(item.footnote_ids.size()); footnote_index++) {
				if (footnote_index < static_cast<int32_t>(item.footnote_starts.size()) && footnote_index < static_cast<int32_t>(item.footnote_ends.size())) {
					const int64_t start = item.paragraph_global_start + item.footnote_starts[footnote_index];
					const int64_t end = item.paragraph_global_start + item.footnote_ends[footnote_index];
					if (p_character >= start && p_character <= end) {
						return item.footnote_ids[footnote_index];
					}
				}
			}
		}

		if (item.type == MARKDOWN_BLOCK_TABLE) {
			for (const MarkdownTableCell& cell : item.cells) {
				if (p_character >= cell.global_start && p_character <= cell.global_end) {
					const int64_t local_character = p_character - cell.global_start;
					for (const MarkdownInlineSpan& span : cell.spans) {
						if (span.footnote_ref == p_reference && !span.footnote_id.is_empty() && local_character >= span.start && local_character <= span.end) {
							return span.footnote_id;
						}
					}
				}
			}
			continue;
		}

		if (!item.container_lines.empty() || item.type == MARKDOWN_BLOCK_BLOCKQUOTE) {
			for (const MarkdownContainerLine& line : item.container_lines) {
				if (p_character >= line.global_start && p_character <= line.global_end) {
					const int64_t local_character = p_character - line.global_start;
					for (const MarkdownInlineSpan& span : line.spans) {
						if (span.footnote_ref == p_reference && !span.footnote_id.is_empty() && local_character >= span.start && local_character <= span.end) {
							return span.footnote_id;
						}
					}
				}
			}
			continue;
		}

		if (p_character >= item.paragraph_global_start && p_character <= item.paragraph_global_end) {
			const int64_t local_character = p_character - item.paragraph_global_start;
			for (const MarkdownInlineSpan& span : item.spans) {
				if (span.footnote_ref == p_reference && !span.footnote_id.is_empty() && local_character >= span.start && local_character <= span.end) {
					return span.footnote_id;
				}
			}
		}
	}
	return String();
}

float MarkdownLabelCanvas::footnote_target_at_position(const Vector2& p_position) const {
	const int64_t character = hit_test_document(p_position);
	const String reference_id = footnote_id_at_character(character, true);
	if (!reference_id.is_empty() && footnote_definition_offsets.has(reference_id)) {
		return footnote_definition_offsets[reference_id];
	}

	const String definition_id = footnote_id_at_character(character, false);
	if (!definition_id.is_empty() && footnote_reference_offsets.has(definition_id)) {
		return footnote_reference_offsets[definition_id];
	}

	return -1.0f;
}

void MarkdownLabelCanvas::_gui_input(const Ref<InputEvent>& p_event) {
	if (label == nullptr) {
		return;
	}

	const Ref<InputEventMouseButton> mouse_button = p_event;
	if (mouse_button.is_valid()) {
		if (mouse_button->get_button_index() == MOUSE_BUTTON_WHEEL_UP || mouse_button->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN) {
			if (mouse_button->is_ctrl_pressed()) {
				int32_t delta = mouse_button->get_button_index() == MOUSE_BUTTON_WHEEL_UP ? 1 : -1;
				label->set_extra_font_size(std::max<int32_t>(0, label->get_extra_font_size() + delta));
				accept_event();
				return;
			}
			return;
		}

		if (mouse_button->get_button_index() == MOUSE_BUTTON_LEFT) {
			if (mouse_button->is_pressed()) {
				press_position = mouse_button->get_position();
				selection_dragged = false;
				Vector2 html_position;
				pressed_html_layout = label->is_parsing_html() && get_html_visible_rect().has_point(press_position)
					? html_at_position(press_position, html_position) : Ref<HTMLLayout>();
				if (pressed_html_layout.is_valid()) {
					pressed_html_link = pressed_html_layout->find_link_at(html_position).id;
					if (!label->is_selection_enabled()) {
						dragging_selection = false;
						selection_drag_attempt = false;
						accept_event();
						return;
					}
				}
				if (pressed_html_layout.is_null() && label->is_drag_and_drop_selection_enabled() && is_position_inside_selection(press_position)) {
					selection_drag_attempt = true;
					dragging_selection = false;
					if (!has_focus()) {
						grab_focus();
					}
					accept_event();
					return;
				}
				selection_drag_attempt = false;
				selection_anchor = hit_test_document(press_position);
				selection_caret = selection_anchor;
				queue_redraw();
				dragging_selection = true;
				if (!has_focus()) {
					grab_focus();
				}
				accept_event();
				return;
			}

			if (pressed_html_layout.is_valid()) {
				Vector2 html_position;
				const Ref<HTMLLayout> html = html_at_position(mouse_button->get_position(), html_position);
				const HTMLLayout::LinkHit hit = html == pressed_html_layout && get_html_visible_rect().has_point(mouse_button->get_position())
					? html->find_link_at(html_position) : HTMLLayout::LinkHit{};
				const bool clicked = !selection_dragged && pressed_html_link != 0 && pressed_html_link == hit.id && !mouse_button->is_canceled() &&
					mouse_button->get_position().distance_to(press_position) <= 4.0f;
				if (dragging_selection && selection_dragged && !mouse_button->is_canceled()) {
					selection_caret = hit_test_document(mouse_button->get_position());
					queue_redraw();
				}
				pressed_html_link = 0;
				pressed_html_layout.unref();
				dragging_selection = false;
				update_internal_processing();
				accept_event();
				if (clicked) {
					label->activate_link_uri(hit.uri);
				}
				return;
			}

			if (selection_drag_attempt) {
				selection_drag_attempt = false;
				if (is_position_inside_selection(mouse_button->get_position())) {
					clear_selection();
				}
				accept_event();
				return;
			}

			if (dragging_selection) {
				dragging_selection = false;
				update_internal_processing();
				if (!selection_dragged) {
					const Vector2 up_position = mouse_button->get_position();
					Vector2 html_position;
					if (html_at_position(up_position, html_position).is_valid()) {
						accept_event();
						return;
					}
					const String uri = link_uri_at_position(up_position);
					if (!uri.is_empty()) {
						label->activate_link_uri(uri);
					}
					else {
						const float footnote_target = footnote_target_at_position(up_position);
						if (footnote_target >= 0.0f) {
							VScrollBar* v_scroll = label->get_v_scroll_bar();
							if (v_scroll != nullptr) {
								const float max_scroll = std::max<float>(0.0f, v_scroll->get_max() - v_scroll->get_page());
								v_scroll->set_value(std::min<float>(std::max<float>(0.0f, footnote_target - 8.0f), max_scroll));
							}
						}
					}
				}
				accept_event();
				return;
			}
			return;
		}

		if (mouse_button->get_button_index() == MOUSE_BUTTON_RIGHT && mouse_button->is_pressed() && label->is_context_menu_enabled()) {
			get_menu();
			_update_context_menu();
			menu->set_position(get_screen_transform().xform(mouse_button->get_position()));
			menu->reset_size();
			menu->popup();
			menu->grab_focus();
			accept_event();
			return;
		}
		return;
	}

	const Ref<InputEventMouseMotion> mouse_motion = p_event;
	if (mouse_motion.is_valid()) {
		if (pressed_html_layout.is_valid() && mouse_motion->get_position().distance_to(press_position) > 4.0f) {
			pressed_html_link = 0;
		}
		if (selection_drag_attempt) {
			return;
		}

		if (dragging_selection) {
			const Vector2 current_position = mouse_motion->get_position();
			if (current_position.distance_to(press_position) > 4.0f) {
				selection_dragged = true;
			}
			if (selection_dragged || pressed_html_layout.is_null()) {
				selection_caret = hit_test_document(current_position);
			}
			internal_processing_enabled = true;
			set_process_internal(true);
			queue_redraw();
			accept_event();
			return;
		}

		const String uri = link_uri_at_position(mouse_motion->get_position());
		const String img_tip = image_tooltip_at_position(mouse_motion->get_position());
		const float footnote_target = footnote_target_at_position(mouse_motion->get_position());
		set_default_cursor_shape((!uri.is_empty() || !img_tip.is_empty() || footnote_target >= 0.0f) ? CURSOR_POINTING_HAND : (label->is_selection_enabled() ? CURSOR_IBEAM : CURSOR_ARROW));
		if (!uri.is_empty()) {
			set_tooltip_text(uri);
		}
		else if (!img_tip.is_empty()) {
			set_tooltip_text(img_tip);
		}
		else if (footnote_target >= 0.0f) {
			set_tooltip_text(String());
		}
		else {
			set_tooltip_text(String());
		}
		return;
	}

	const Ref<InputEventKey> key_event = p_event;
	if (key_event.is_valid() && key_event->is_pressed()) {
		if (label->is_shortcut_keys_enabled() && key_event->is_action("ui_text_select_all", true)) {
			select_all();
			accept_event();
			return;
		}
		if (label->is_shortcut_keys_enabled() && key_event->is_action("ui_copy", true)) {
			const String selected = get_selected_text();
			if (!selected.is_empty()) {
				DisplayServer::get_singleton()->clipboard_set(selected);
			}
			accept_event();
			return;
		}
		if (key_event->is_action("ui_menu", true)) {
			if (label->is_context_menu_enabled()) {
				get_menu();
				_update_context_menu();
				menu->set_position(get_screen_position());
				menu->reset_size();
				menu->popup();
				menu->grab_focus();
			}
			accept_event();
			return;
		}
		return;
	}
}

// MarkdownLabel
void MarkdownLabel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_markdown_text", "text"), &MarkdownLabel::set_markdown_text);
	ClassDB::bind_method(D_METHOD("get_markdown_text"), &MarkdownLabel::get_markdown_text);
	ClassDB::bind_method(D_METHOD("set_parse_latex", "enabled"), &MarkdownLabel::set_parse_latex);
	ClassDB::bind_method(D_METHOD("set_parse_html", "enabled"), &MarkdownLabel::set_parse_html);
	ClassDB::bind_method(D_METHOD("is_parsing_html"), &MarkdownLabel::is_parsing_html);
	ClassDB::bind_method(D_METHOD("is_parsing_latex"), &MarkdownLabel::is_parsing_latex);
	ClassDB::bind_method(D_METHOD("set_parse_gfm", "enabled"), &MarkdownLabel::set_parse_gfm);
	ClassDB::bind_method(D_METHOD("is_parsing_gfm"), &MarkdownLabel::is_parsing_gfm);
	ClassDB::bind_method(D_METHOD("set_parse_mermaid", "enabled"), &MarkdownLabel::set_parse_mermaid);
	ClassDB::bind_method(D_METHOD("is_parsing_mermaid"), &MarkdownLabel::is_parsing_mermaid);

	ClassDB::bind_method(D_METHOD("append_text", "delta"), &MarkdownLabel::append_text);
	ClassDB::bind_method(D_METHOD("finish_stream"), &MarkdownLabel::finish_stream);
	ClassDB::bind_method(D_METHOD("clear"), &MarkdownLabel::clear);

	ClassDB::bind_method(D_METHOD("set_streaming_enabled", "enabled"), &MarkdownLabel::set_streaming_enabled);
	ClassDB::bind_method(D_METHOD("is_streaming_enabled"), &MarkdownLabel::is_streaming_enabled);

	ClassDB::bind_method(D_METHOD("set_max_unstable_lines", "lines"), &MarkdownLabel::set_max_unstable_lines);
	ClassDB::bind_method(D_METHOD("get_max_unstable_lines"), &MarkdownLabel::get_max_unstable_lines);

	ClassDB::bind_method(D_METHOD("set_deferred_layout_enabled", "enabled"), &MarkdownLabel::set_deferred_layout_enabled);
	ClassDB::bind_method(D_METHOD("is_deferred_layout_enabled"), &MarkdownLabel::is_deferred_layout_enabled);

	ClassDB::bind_method(D_METHOD("set_layout_flush_interval_msec", "msec"), &MarkdownLabel::set_layout_flush_interval_msec);
	ClassDB::bind_method(D_METHOD("get_layout_flush_interval_msec"), &MarkdownLabel::get_layout_flush_interval_msec);

	ClassDB::bind_method(D_METHOD("flush_pending_layout"), &MarkdownLabel::flush_pending_layout);

	ClassDB::bind_method(D_METHOD("set_fit_content", "enabled"), &MarkdownLabel::set_fit_content);
	ClassDB::bind_method(D_METHOD("is_fit_content_enabled"), &MarkdownLabel::is_fit_content_enabled);

	ClassDB::bind_method(D_METHOD("set_scroll_active", "active"), &MarkdownLabel::set_scroll_active);
	ClassDB::bind_method(D_METHOD("is_scroll_active"), &MarkdownLabel::is_scroll_active);

	ClassDB::bind_method(D_METHOD("set_scroll_follow", "follow"), &MarkdownLabel::set_scroll_follow);
	ClassDB::bind_method(D_METHOD("is_scroll_following"), &MarkdownLabel::is_scroll_following);

	ClassDB::bind_method(D_METHOD("set_scroll_follow_visible_characters", "follow"), &MarkdownLabel::set_scroll_follow_visible_characters);
	ClassDB::bind_method(D_METHOD("is_scroll_following_visible_characters"), &MarkdownLabel::is_scroll_following_visible_characters);

	ClassDB::bind_method(D_METHOD("set_autowrap_mode", "autowrap_mode"), &MarkdownLabel::set_autowrap_mode);
	ClassDB::bind_method(D_METHOD("get_autowrap_mode"), &MarkdownLabel::get_autowrap_mode);

	ClassDB::bind_method(D_METHOD("set_autowrap_trim_flags", "autowrap_trim_flags"), &MarkdownLabel::set_autowrap_trim_flags);
	ClassDB::bind_method(D_METHOD("get_autowrap_trim_flags"), &MarkdownLabel::get_autowrap_trim_flags);

	ClassDB::bind_method(D_METHOD("set_content_margin", "margin"), &MarkdownLabel::set_content_margin);
	ClassDB::bind_method(D_METHOD("get_content_margin"), &MarkdownLabel::get_content_margin);

	ClassDB::bind_method(D_METHOD("set_extra_font_size", "size"), &MarkdownLabel::set_extra_font_size);
	ClassDB::bind_method(D_METHOD("get_extra_font_size"), &MarkdownLabel::get_extra_font_size);

	ClassDB::bind_method(D_METHOD("set_selection_enabled", "enabled"), &MarkdownLabel::set_selection_enabled);
	ClassDB::bind_method(D_METHOD("is_selection_enabled"), &MarkdownLabel::is_selection_enabled);

	ClassDB::bind_method(D_METHOD("set_deselect_on_focus_loss_enabled", "enabled"), &MarkdownLabel::set_deselect_on_focus_loss_enabled);
	ClassDB::bind_method(D_METHOD("is_deselect_on_focus_loss_enabled"), &MarkdownLabel::is_deselect_on_focus_loss_enabled);

	ClassDB::bind_method(D_METHOD("set_drag_and_drop_selection_enabled", "enabled"), &MarkdownLabel::set_drag_and_drop_selection_enabled);
	ClassDB::bind_method(D_METHOD("is_drag_and_drop_selection_enabled"), &MarkdownLabel::is_drag_and_drop_selection_enabled);

	ClassDB::bind_method(D_METHOD("set_open_external_links", "enabled"), &MarkdownLabel::set_open_external_links);
	ClassDB::bind_method(D_METHOD("get_open_external_links"), &MarkdownLabel::get_open_external_links);

	ClassDB::bind_method(D_METHOD("select_all"), &MarkdownLabel::select_all);
	ClassDB::bind_method(D_METHOD("clear_selection"), &MarkdownLabel::clear_selection);
	ClassDB::bind_method(D_METHOD("get_selected_text"), &MarkdownLabel::get_selected_text);

	ClassDB::bind_method(D_METHOD("search", "text", "flags", "from_line", "from_column"), &MarkdownLabel::search);

	ClassDB::bind_method(D_METHOD("get_link_uri_at_point", "position"), &MarkdownLabel::get_link_uri_at_point);
	ClassDB::bind_method(D_METHOD("activate_link_uri", "uri"), &MarkdownLabel::activate_link_uri);

	ClassDB::bind_method(D_METHOD("has_anchor", "anchor"), &MarkdownLabel::has_anchor);
	ClassDB::bind_method(D_METHOD("get_anchors"), &MarkdownLabel::get_anchors);
	ClassDB::bind_method(D_METHOD("scroll_to_anchor", "anchor", "align"), &MarkdownLabel::scroll_to_anchor, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("scroll_to_search_result", "align"), &MarkdownLabel::scroll_to_search_result, DEFVAL(0.0f));

	ClassDB::bind_method(D_METHOD("load_markdown_file", "path"), &MarkdownLabel::load_markdown_file);
	ClassDB::bind_method(D_METHOD("refresh"), &MarkdownLabel::refresh);

	ClassDB::bind_method(D_METHOD("get_rendered_block_count"), &MarkdownLabel::get_rendered_block_count);
	ClassDB::bind_method(D_METHOD("get_content_height"), &MarkdownLabel::get_content_height);

	ClassDB::bind_method(D_METHOD("get_h_scroll_bar"), &MarkdownLabel::get_h_scroll_bar);
	ClassDB::bind_method(D_METHOD("get_v_scroll_bar"), &MarkdownLabel::get_v_scroll_bar);

	ClassDB::bind_method(D_METHOD("set_context_menu_enabled", "enabled"), &MarkdownLabel::set_context_menu_enabled);
	ClassDB::bind_method(D_METHOD("is_context_menu_enabled"), &MarkdownLabel::is_context_menu_enabled);

	ClassDB::bind_method(D_METHOD("set_shortcut_keys_enabled", "enabled"), &MarkdownLabel::set_shortcut_keys_enabled);
	ClassDB::bind_method(D_METHOD("is_shortcut_keys_enabled"), &MarkdownLabel::is_shortcut_keys_enabled);

	ClassDB::bind_method(D_METHOD("get_menu"), &MarkdownLabel::get_menu);
	ClassDB::bind_method(D_METHOD("is_menu_visible"), &MarkdownLabel::is_menu_visible);
	ClassDB::bind_method(D_METHOD("menu_option", "option"), &MarkdownLabel::menu_option);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_markdown_text", "get_markdown_text");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "parse_latex"), "set_parse_latex", "is_parsing_latex");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "parse_html"), "set_parse_html", "is_parsing_html");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "parse_gfm"), "set_parse_gfm", "is_parsing_gfm");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "parse_mermaid"), "set_parse_mermaid", "is_parsing_mermaid");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "content_margin", PROPERTY_HINT_RANGE, "0,128"), "set_content_margin", "get_content_margin");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "extra_font_size", PROPERTY_HINT_RANGE, "0,16"), "set_extra_font_size", "get_extra_font_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "open_external_links"), "set_open_external_links", "get_open_external_links");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selection_enabled"), "set_selection_enabled", "is_selection_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "deselect_on_focus_loss_enabled"), "set_deselect_on_focus_loss_enabled", "is_deselect_on_focus_loss_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "drag_and_drop_selection_enabled"), "set_drag_and_drop_selection_enabled", "is_drag_and_drop_selection_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "context_menu_enabled"), "set_context_menu_enabled", "is_context_menu_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shortcut_keys_enabled"), "set_shortcut_keys_enabled", "is_shortcut_keys_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "streaming_enabled"), "set_streaming_enabled", "is_streaming_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_unstable_lines", PROPERTY_HINT_RANGE, "1,256"), "set_max_unstable_lines", "get_max_unstable_lines");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "deferred_layout_enabled"), "set_deferred_layout_enabled", "is_deferred_layout_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "layout_flush_interval_msec", PROPERTY_HINT_RANGE, "1,1000,1"), "set_layout_flush_interval_msec", "get_layout_flush_interval_msec");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fit_content"), "set_fit_content", "is_fit_content_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_active"), "set_scroll_active", "is_scroll_active");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_following"), "set_scroll_follow", "is_scroll_following");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_following_visible_characters"), "set_scroll_follow_visible_characters", "is_scroll_following_visible_characters");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "autowrap_mode", PROPERTY_HINT_ENUM, "Off,Arbitrary,Word,Word (Smart)"), "set_autowrap_mode", "get_autowrap_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "autowrap_trim_flags", PROPERTY_HINT_FLAGS, "Trim Spaces After Break:64,Trim Spaces Before Break:128"), "set_autowrap_trim_flags", "get_autowrap_trim_flags");

	ADD_SIGNAL(MethodInfo("link_pressed", PropertyInfo(Variant::STRING, "uri")));
	ADD_SIGNAL(MethodInfo("text_changed"));
	ADD_SIGNAL(MethodInfo("render_warning", PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::INT, "line")));
	ADD_SIGNAL(MethodInfo("extra_font_size_changed", PropertyInfo(Variant::INT, "font_size")));

	BIND_ENUM_CONSTANT(MENU_COPY);
	BIND_ENUM_CONSTANT(MENU_SELECT_ALL);
	BIND_ENUM_CONSTANT(MENU_MAX);
}

void MarkdownLabel::ensure_controls() {
	if (scroll_container != nullptr) {
		return;
	}

	set_theme_type_variation("MarkdownLabel");

	scroll_container = memnew(ScrollContainer);
	scroll_container->set_name("_MarkdownScroll");
	scroll_container->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	scroll_container->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	scroll_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(scroll_container, false, Node::INTERNAL_MODE_BACK);
	VScrollBar* v_scroll = scroll_container->get_v_scroll_bar();
	if (v_scroll != nullptr) {
		v_scroll->connect("value_changed", callable_mp(this, &MarkdownLabel::on_scroll_value_changed));
	}
	HScrollBar* h_scroll = scroll_container->get_h_scroll_bar();
	if (h_scroll != nullptr) {
		h_scroll->connect("value_changed", callable_mp(this, &MarkdownLabel::on_scroll_value_changed));
	}

	margin_container = memnew(MarginContainer);
	margin_container->set_name("_MarkdownMargin");
	margin_container->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	margin_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	margin_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	scroll_container->add_child(margin_container, false, Node::INTERNAL_MODE_BACK);

	canvas = memnew(MarkdownLabelCanvas);
	canvas->set_name("_MarkdownCanvas");
	canvas->set_label(this);
	canvas->set_focus_mode(Control::FOCUS_CLICK);
	canvas->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	canvas->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	canvas->set_custom_minimum_size(Vector2(0.0, 1.0));
	canvas->set_streaming_enabled(streaming_enabled);
	canvas->set_max_unstable_lines(max_unstable_lines);
	canvas->set_deferred_layout_enabled(deferred_layout_enabled);
	canvas->set_layout_flush_interval_msec(layout_flush_interval_msec);
	canvas->set_markdown_text(raw_text);
	margin_container->add_child(canvas, false, Node::INTERNAL_MODE_BACK);

	apply_theme_settings();
	apply_scroll_settings();
}

void MarkdownLabel::on_scroll_value_changed(double p_value) {
	(void)p_value;
	update_scroll_following();
	if (canvas != nullptr) {
		canvas->pressed_html_link = 0;
		canvas->queue_redraw();
	}
}

void MarkdownLabel::apply_theme_settings() {
	if (margin_container == nullptr) {
		return;
	}

	margin_container->add_theme_constant_override("margin_left", content_margin);
	margin_container->add_theme_constant_override("margin_right", content_margin);
	margin_container->add_theme_constant_override("margin_top", content_margin);
	if (fit_content) {
		update_minimum_size();
	}
}

void MarkdownLabel::apply_scroll_settings() {
	if (scroll_container == nullptr) {
		return;
	}

	scroll_container->set_vertical_scroll_mode(scroll_active ? ScrollContainer::SCROLL_MODE_AUTO : ScrollContainer::SCROLL_MODE_DISABLED);
}

void MarkdownLabel::update_scroll_following() {
	if (!scroll_follow) {
		scroll_following = false;
		return;
	}

	if (scroll_container == nullptr) {
		scroll_following = true;
		return;
	}

	VScrollBar* v_scroll = scroll_container->get_v_scroll_bar();
	if (v_scroll == nullptr || !v_scroll->is_visible_in_tree()) {
		scroll_following = true;
		return;
	}

	scroll_following = v_scroll->get_value() > (v_scroll->get_max() - v_scroll->get_page() - 1.0);
}

void MarkdownLabel::scroll_to_following() {
	if (!scroll_follow || !scroll_following || scroll_container == nullptr || canvas == nullptr) {
		return;
	}

	const float target_height = canvas->get_content_height();
	scroll_container->call_deferred("set_deferred", "scroll_vertical", static_cast<int32_t>(target_height));
}

void MarkdownLabel::_notification(int p_what) {
	if (p_what == NOTIFICATION_POSTINITIALIZE || p_what == NOTIFICATION_POST_ENTER_TREE) {
		ensure_controls();
	}

	if (p_what == NOTIFICATION_READY) {
		refresh();
	}
	if (p_what == NOTIFICATION_RESIZED && canvas != nullptr && !fit_content) {
		canvas->mark_layout_dirty();
	}

	if (p_what == NOTIFICATION_THEME_CHANGED) {
		apply_theme_settings();
		if (canvas != nullptr) {
			refresh();
		}
	}
}

Vector2 MarkdownLabel::_get_minimum_size() const {
	if (!fit_content) {
		return Vector2();
	}

	const float text_height = canvas == nullptr ? 1.0f : canvas->get_content_height();
	return Vector2(0.0f, text_height + static_cast<float>(content_margin));
}

void MarkdownLabel::set_markdown_text(const String& p_text) {
	set_markdown_source(p_text, String());
}

void MarkdownLabel::set_markdown_source(const String& p_text, const String& p_html_base_dir) {
	html_file_base_dir = p_html_base_dir;
	update_scroll_following();
	raw_text = p_text;
	if (canvas != nullptr) {
		canvas->set_markdown_text(p_text);
	}
	notify_canvas_layout_changed();
	emit_signal("text_changed");
}

String MarkdownLabel::get_html_base_dir() const {
	if (!html_file_base_dir.is_empty()) {
		return html_file_base_dir;
	}
	return image_base_dir.begins_with("res://") ? image_base_dir : String("res://");
}

void MarkdownLabel::set_parse_html(bool p_enabled) {
	if (parse_html == p_enabled) {
		return;
	}
	parse_html = p_enabled;
	if (canvas != nullptr) {
		canvas->set_markdown_text(raw_text);
	}
	notify_canvas_layout_changed();
}

bool MarkdownLabel::is_parsing_html() const {
	return parse_html;
}

String MarkdownLabel::get_markdown_text() const {
	return raw_text;
}

void MarkdownLabel::set_parse_latex(bool p_enabled) {
	if (parse_latex == p_enabled) {
		return;
	}
	parse_latex = p_enabled;
	if (canvas != nullptr) {
		canvas->set_markdown_text(raw_text);
	}
	notify_canvas_layout_changed();
}

bool MarkdownLabel::is_parsing_latex() const {
	return parse_latex;
}

void MarkdownLabel::set_parse_gfm(bool p_enabled) {
	if (parse_gfm == p_enabled) {
		return;
	}
	parse_gfm = p_enabled;
	if (canvas != nullptr) {
		canvas->set_markdown_text(raw_text);
	}
	notify_canvas_layout_changed();
}

bool MarkdownLabel::is_parsing_gfm() const {
	return parse_gfm;
}

void MarkdownLabel::set_parse_mermaid(bool p_enabled) {
	if (parse_mermaid == p_enabled) {
		return;
	}
	parse_mermaid = p_enabled;
	if (canvas != nullptr) {
		canvas->layout_prefix_reuse_requested = false;
		canvas->mark_layout_dirty();
	}
	notify_canvas_layout_changed();
}

bool MarkdownLabel::is_parsing_mermaid() const {
	return parse_mermaid;
}

void MarkdownLabel::set_image_base_dir(const String& p_dir) {
	if (image_base_dir == p_dir) {
		return;
	}
	image_base_dir = p_dir;
	if (canvas != nullptr) {
		canvas->layout_prefix_reuse_requested = false;
		canvas->mark_layout_dirty();
	}
	notify_canvas_layout_changed();
}

void MarkdownLabel::append_text(const String& p_delta) {
	ensure_controls();
	update_scroll_following();

	raw_text += p_delta;
	canvas->set_streaming_enabled(streaming_enabled);
	canvas->set_max_unstable_lines(max_unstable_lines);
	canvas->set_deferred_layout_enabled(deferred_layout_enabled);
	canvas->set_layout_flush_interval_msec(layout_flush_interval_msec);
	canvas->append_text_incremental(p_delta);
	if (!deferred_layout_enabled || !streaming_enabled) {
		notify_canvas_layout_changed();
	}
	emit_signal("text_changed");
}

void MarkdownLabel::finish_stream() {
	update_scroll_following();

	if (canvas != nullptr) {
		canvas->finish_stream();
	}
	notify_canvas_layout_changed();
}

void MarkdownLabel::clear() {
	raw_text = String();
	if (canvas != nullptr) {
		canvas->clear_document();
	}
	if (scroll_follow) {
		scroll_following = true;
	}
	notify_canvas_layout_changed();
	emit_signal("text_changed");
}

void MarkdownLabel::set_streaming_enabled(bool p_enabled) {
	streaming_enabled = p_enabled;
	if (canvas != nullptr) {
		canvas->set_streaming_enabled(p_enabled);
	}
}

bool MarkdownLabel::is_streaming_enabled() const {
	return streaming_enabled;
}

void MarkdownLabel::set_max_unstable_lines(int32_t p_lines) {
	max_unstable_lines = p_lines;
	if (canvas != nullptr) {
		canvas->set_max_unstable_lines(p_lines);
	}
}

int32_t MarkdownLabel::get_max_unstable_lines() const {
	return max_unstable_lines;
}

void MarkdownLabel::set_deferred_layout_enabled(bool p_enabled) {
	if (deferred_layout_enabled == p_enabled) {
		return;
	}

	deferred_layout_enabled = p_enabled;
	if (canvas != nullptr) {
		canvas->set_deferred_layout_enabled(p_enabled);
	}
}

bool MarkdownLabel::is_deferred_layout_enabled() const {
	return deferred_layout_enabled;
}

void MarkdownLabel::set_layout_flush_interval_msec(int32_t p_msec) {
	layout_flush_interval_msec = std::max<int32_t>(1, std::min<int32_t>(p_msec, 1000));
	if (canvas != nullptr) {
		canvas->set_layout_flush_interval_msec(layout_flush_interval_msec);
	}
}

int32_t MarkdownLabel::get_layout_flush_interval_msec() const {
	return layout_flush_interval_msec;
}

bool MarkdownLabel::flush_pending_layout() {
	if (canvas == nullptr) {
		return false;
	}
	return canvas->flush_pending_layout();
}

void MarkdownLabel::set_fit_content(bool p_enabled) {
	if (fit_content == p_enabled) {
		return;
	}

	fit_content = p_enabled;
	refresh();
	update_minimum_size();
}

bool MarkdownLabel::is_fit_content_enabled() const {
	return fit_content;
}

void MarkdownLabel::set_scroll_active(bool p_active) {
	if (scroll_active == p_active) {
		return;
	}

	scroll_active = p_active;
	apply_scroll_settings();
	queue_redraw();
	if (canvas != nullptr) {
		canvas->queue_redraw();
	}
}

bool MarkdownLabel::is_scroll_active() const {
	return scroll_active;
}

void MarkdownLabel::set_scroll_follow(bool p_follow) {
	scroll_follow = p_follow;
	if (scroll_follow) {
		if (scroll_container == nullptr) {
			scroll_following = true;
		}
		else {
			VScrollBar* v_scroll = scroll_container->get_v_scroll_bar();
			scroll_following = v_scroll == nullptr || !v_scroll->is_visible_in_tree() || v_scroll->get_value() > (v_scroll->get_max() - v_scroll->get_page() - 1.0);
		}
	}
	else {
		scroll_following = false;
	}
}

bool MarkdownLabel::is_scroll_following() const {
	return scroll_follow;
}

void MarkdownLabel::set_scroll_follow_visible_characters(bool p_follow) {
	scroll_follow_visible_characters = p_follow;
}

bool MarkdownLabel::is_scroll_following_visible_characters() const {
	return scroll_follow_visible_characters;
}

void MarkdownLabel::set_autowrap_mode(TextServer::AutowrapMode p_mode) {
	if (autowrap_mode == p_mode) {
		return;
	}

	autowrap_mode = p_mode;
	if (canvas != nullptr) {
		canvas->mark_layout_dirty();
	}
	update_minimum_size();
}

TextServer::AutowrapMode MarkdownLabel::get_autowrap_mode() const {
	return autowrap_mode;
}

void MarkdownLabel::set_autowrap_trim_flags(BitField<TextServer::LineBreakFlag> p_flags) {
	const BitField<TextServer::LineBreakFlag> trimmed_flags = p_flags & get_autowrap_trim_mask();
	if (autowrap_flags_trim == trimmed_flags) {
		return;
	}

	autowrap_flags_trim = trimmed_flags;
	if (canvas != nullptr) {
		canvas->mark_layout_dirty();
	}
	update_minimum_size();
}

BitField<TextServer::LineBreakFlag> MarkdownLabel::get_autowrap_trim_flags() const {
	return autowrap_flags_trim;
}

void MarkdownLabel::set_content_margin(int32_t p_margin) {
	content_margin = std::max<int32_t>(0, std::min<int32_t>(p_margin, 128));
	apply_theme_settings();
	if (fit_content) {
		update_minimum_size();
	}
}

int32_t MarkdownLabel::get_content_margin() const {
	return content_margin;
}

void MarkdownLabel::set_extra_font_size(int32_t p_extra) {
	extra_font_size = std::max<int32_t>(0, std::min<int32_t>(p_extra, 16));
	emit_signal("extra_font_size_changed", extra_font_size);
	if (canvas != nullptr) {
		canvas->layout_prefix_reuse_requested = false;
		canvas->mark_layout_dirty();
	}
}

int32_t MarkdownLabel::get_extra_font_size() const {
	return extra_font_size;
}

void MarkdownLabel::set_selection_enabled(bool p_enabled) {
	selection_enabled = p_enabled;
}

bool MarkdownLabel::is_selection_enabled() const {
	return selection_enabled;
}

void MarkdownLabel::set_deselect_on_focus_loss_enabled(bool p_enabled) {
	deselect_on_focus_loss_enabled = p_enabled;
}

bool MarkdownLabel::is_deselect_on_focus_loss_enabled() const {
	return deselect_on_focus_loss_enabled;
}

void MarkdownLabel::set_drag_and_drop_selection_enabled(bool p_enabled) {
	drag_and_drop_selection_enabled = p_enabled;
}

bool MarkdownLabel::is_drag_and_drop_selection_enabled() const {
	return drag_and_drop_selection_enabled;
}

void MarkdownLabel::set_context_menu_enabled(bool p_enabled) {
	context_menu_enabled = p_enabled;
}

bool MarkdownLabel::is_context_menu_enabled() const {
	return context_menu_enabled;
}

void MarkdownLabel::set_shortcut_keys_enabled(bool p_enabled) {
	shortcut_keys_enabled = p_enabled;
}

bool MarkdownLabel::is_shortcut_keys_enabled() const {
	return shortcut_keys_enabled;
}

void MarkdownLabel::set_open_external_links(bool p_enabled) {
	open_external_links = p_enabled;
}

bool MarkdownLabel::get_open_external_links() const {
	return open_external_links;
}

PopupMenu* MarkdownLabel::get_menu() {
	ensure_controls();
	return canvas->get_menu();
}

bool MarkdownLabel::is_menu_visible() const {
	if (canvas == nullptr) {
		return false;
	}
	return canvas->is_menu_visible();
}

void MarkdownLabel::menu_option(int p_option) {
	if (canvas == nullptr) {
		return;
	}
	canvas->menu_option(p_option);
}

void MarkdownLabel::select_all() {
	ensure_controls();
	canvas->select_all();
}

void MarkdownLabel::clear_selection() {
	if (canvas != nullptr) {
		canvas->clear_selection();
	}
}

String MarkdownLabel::get_selected_text() const {
	if (canvas == nullptr) {
		return String();
	}
	return canvas->get_selected_text();
}

Vector2i MarkdownLabel::search(const String& p_text, int32_t p_flags, int32_t p_from_line, int32_t p_from_column) {
	ensure_controls();
	return canvas->search(p_text, p_flags, p_from_line, p_from_column);
}

String MarkdownLabel::get_link_uri_at_point(const Vector2& p_position) {
	ensure_controls();
	return canvas->get_link_uri_at_point(p_position);
}

void MarkdownLabel::activate_link_uri(const String& p_uri) {
	if (p_uri.begins_with("#")) {
		scroll_to_anchor(docrender::decode_html_fragment(p_uri.substr(1)));
	}
	emit_signal("link_pressed", p_uri);
	if (open_external_links && (p_uri.begins_with("http://") || p_uri.begins_with("https://"))) {
		OS::get_singleton()->shell_open(p_uri);
	}
}

bool MarkdownLabel::has_anchor(const String& p_anchor) const {
	if (canvas == nullptr) {
		return false;
	}
	return canvas->has_anchor(p_anchor);
}

PackedStringArray MarkdownLabel::get_anchors() const {
	if (canvas == nullptr) {
		return PackedStringArray();
	}
	return canvas->get_anchors();
}

bool MarkdownLabel::scroll_to_anchor(const String& p_anchor, float p_align) {
	ensure_controls();
	if (scroll_container == nullptr || canvas == nullptr) {
		return false;
	}

	const float offset = canvas->get_anchor_offset(p_anchor);
	if (offset < 0.0f) {
		return false;
	}

	ScrollContainer* target_scroll = scroll_container;
	float target_offset = offset;
	if (is_inside_tree() && (fit_content || !scroll_active)) {
		Control* child = this;
		for (Control* parent = get_parent_control(); parent; parent = parent->get_parent_control()) {
			auto* ancestor = Object::cast_to<ScrollContainer>(parent);
			if (ancestor && ancestor->get_vertical_scroll_mode() != ScrollContainer::SCROLL_MODE_DISABLED) {
				target_scroll = ancestor;
				// Measure from the scrollable child so a deferred container sort cannot affect the offset.
				const Vector2 target = canvas->get_global_transform_with_canvas().xform(Vector2(0.0f, offset));
				const Vector2 origin = child->get_global_transform_with_canvas().get_origin();
				target_offset = ancestor->get_global_transform_with_canvas().affine_inverse().basis_xform(target - origin).y;
				break;
			}
			child = parent;
		}
	}
	VScrollBar* v_scroll = target_scroll->get_v_scroll_bar();
	const float align = std::isfinite(p_align) ? std::clamp(p_align, 0.0f, 1.0f) : 0.0f;
	v_scroll->set_value(target_offset - v_scroll->get_page() * align);
	return true;
}

bool MarkdownLabel::scroll_to_search_result(float p_align) {
	ensure_controls();
	if (scroll_container == nullptr || canvas == nullptr) {
		return false;
	}

	const float y = canvas->get_search_result_y();
	if (y < 0.0f) {
		return false;
	}

	VScrollBar* v_scroll = scroll_container->get_v_scroll_bar();
	if (v_scroll == nullptr) {
		return false;
	}

	const float viewport_height = scroll_container->get_size().y;
	const float target = std::max<float>(0.0f, y - viewport_height * p_align);
	v_scroll->set_value(std::min<float>(target, v_scroll->get_max() - v_scroll->get_page()));
	return true;
}

double MarkdownLabel::get_scroll_offset_for_source_line(double p_line, int32_t p_line_count) {
	return canvas ? canvas->get_offset_for_source_line(p_line, p_line_count) : 0.0;
}

double MarkdownLabel::get_source_line_at_scroll_offset(double p_offset, int32_t p_line_count) {
	return canvas ? canvas->get_source_line_at_offset(p_offset, p_line_count) : 0.0;
}

Error MarkdownLabel::load_markdown_file(const String& p_path) {
	const String path = ProjectSettings::get_singleton()->localize_path(ResourceUID::ensure_path(p_path));
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	const String content = file->get_as_text();
	file->close();

	set_markdown_source(content, path.begins_with("res://") ? path.get_base_dir() : String());
	return OK;
}

void MarkdownLabel::refresh() {
	if (canvas != nullptr) {
		canvas->mark_layout_dirty();
	}
}

void MarkdownLabel::notify_canvas_layout_changed() {
	if (fit_content) {
		update_minimum_size();
	}
	scroll_to_following();
}

int32_t MarkdownLabel::get_rendered_block_count() const {
	if (canvas == nullptr) {
		return 0;
	}
	return canvas->get_rendered_block_count();
}

float MarkdownLabel::get_content_height() const {
	if (canvas == nullptr) {
		return 1.0f;
	}
	return canvas->get_content_height();
}

HScrollBar* MarkdownLabel::get_h_scroll_bar() const {
	if (scroll_container == nullptr) {
		return nullptr;
	}
	return scroll_container->get_h_scroll_bar();
}

VScrollBar* MarkdownLabel::get_v_scroll_bar() const {
	if (scroll_container == nullptr) {
		return nullptr;
	}
	return scroll_container->get_v_scroll_bar();
}
