#ifndef DOCRENDER_HTML_DOCUMENT_H
#define DOCRENDER_HTML_DOCUMENT_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <cstdint>
#include <memory>

namespace godot {
	class CanvasItem;
}

class HTMLDocument : public godot::RefCounted {
	GDCLASS(HTMLDocument, godot::RefCounted)

		struct State;
	std::unique_ptr<State> state;

	void _on_canvas_redraw(uint64_t p_id);
	void _detach_canvas(uint64_t p_id);

protected:
	static void _bind_methods();

public:
	HTMLDocument();
	~HTMLDocument() override;

	godot::Error parse(const godot::String& p_html, const godot::String& p_base_dir = "res://",
		const godot::String& p_stylesheet = "");
	godot::Error layout(const godot::Vector2i& p_viewport_size);
	godot::Vector2i get_content_size() const;
	void draw(godot::CanvasItem* p_canvas, const godot::Vector2& p_position = {},
		const godot::Rect2& p_clip = {});
	godot::String get_link_at(const godot::Vector2& p_position) const;
	godot::String get_render_error() const;
	void clear();
};

#endif
