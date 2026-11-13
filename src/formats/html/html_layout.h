#ifndef DOCRENDER_HTML_LAYOUT_H
#define DOCRENDER_HTML_LAYOUT_H

#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <cstdint>
#include <memory>

namespace godot {
	class CanvasItem;
	class StyleBox;
}

class HTMLLayout : public godot::RefCounted {
	GDCLASS(HTMLLayout, godot::RefCounted)

	struct State;
	std::unique_ptr<State> state;

	void _on_canvas_redraw(uint64_t p_id);
	void _detach_canvas(uint64_t p_id);

protected:
	static void _bind_methods();

public:
	struct Fonts {
		godot::Ref<godot::Font> text;
		godot::Ref<godot::Font> bold;
		godot::Ref<godot::Font> italic;
		godot::Ref<godot::Font> bold_italic;
		godot::Ref<godot::Font> code;
		godot::Ref<godot::Font> code_block;
	};

	struct LinkHit {
		godot::String uri;
		uintptr_t id = 0;
	};

	HTMLLayout();
	~HTMLLayout() override;
	static void release_paint_shader();

	godot::Error parse(const godot::String& p_html, const godot::String& p_base_dir = "res://",
		const godot::String& p_stylesheet = "");
	godot::Error parse_source(const godot::String& p_html, const godot::String& p_base_dir,
		const godot::String& p_stylesheet, const godot::Dictionary& p_textures, bool p_fit_images = false,
		const godot::String& p_default_stylesheet = {}, const Fonts& p_fonts = {},
		const godot::Dictionary& p_local_stylesheets = {});
	godot::Error layout(const godot::Vector2i& p_viewport_size);
	godot::Vector2i get_content_size() const;
	godot::String get_plain_text() const;
	int64_t hit_test_text(const godot::Vector2& p_position) const;
	godot::TypedArray<godot::Rect2> get_selection_rects(int64_t p_from, int64_t p_to) const;
	void draw(godot::CanvasItem* p_canvas, const godot::Vector2& p_position = {},
		const godot::Rect2& p_clip = {});
	void draw_with_highlights(godot::CanvasItem* p_canvas, const godot::Vector2& p_position, const godot::Rect2& p_clip,
		const godot::Vector2i& p_selection, const godot::StyleBox* p_selection_style, const godot::Color& p_selection_color,
		const godot::Vector2i& p_search = {}, const godot::StyleBox* p_search_style = nullptr);
	godot::String get_link_at(const godot::Vector2& p_position) const;
	LinkHit find_link_at(const godot::Vector2& p_position) const;
	godot::Rect2 get_anchor_rect(const godot::String& p_id) const;
	const godot::HashMap<godot::String, godot::Rect2>& get_anchor_rects() const;
	godot::String get_render_error() const;
	void clear();
};

#endif
