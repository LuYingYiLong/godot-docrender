#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include <memory>

namespace litehtml {
	class element;
}

class HTMLLayout;

class HTMLElement : public godot::RefCounted {
	GDCLASS(HTMLElement, godot::RefCounted)

	friend class HTMLLayout;

	godot::ObjectID layout_id;
	uint64_t parse_version = 0;
	std::weak_ptr<litehtml::element> element;

	HTMLLayout* get_owner() const;
	std::shared_ptr<litehtml::element> lock_element() const;

protected:
	static void _bind_methods();

public:
	bool is_valid() const;
	godot::String get_tag_name() const;
	bool has_attribute(const godot::String& p_name) const;
	godot::String get_attribute(const godot::String& p_name, const godot::String& p_default_value = {}) const;
	godot::String get_text_content() const;
	godot::Ref<HTMLElement> get_parent() const;
	godot::TypedArray<HTMLElement> get_children() const;
	godot::TypedArray<godot::Rect2> get_client_rects() const;
	godot::Rect2 get_bounding_rect() const;
};
