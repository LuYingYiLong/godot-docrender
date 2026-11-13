#pragma once

#include "html_layout.h"

#include <godot_cpp/classes/resource.hpp>

class HTMLDocument : public godot::Resource {
	GDCLASS(HTMLDocument, godot::Resource)

	godot::String text;
	godot::String base_dir = "res://";
	godot::String stylesheet;
	godot::Dictionary textures;
	godot::Dictionary local_stylesheets;

protected:
	static void _bind_methods();

public:
	void set_text(const godot::String& p_text);
	godot::String get_text() const;
	void set_base_dir(const godot::String& p_dir);
	godot::String get_base_dir() const;
	void set_stylesheet(const godot::String& p_stylesheet);
	godot::String get_stylesheet() const;
	void set_textures(const godot::Dictionary& p_textures);
	godot::Dictionary get_textures() const;
	void set_local_stylesheets(const godot::Dictionary& p_stylesheets);
	godot::Dictionary get_local_stylesheets() const;
	godot::Ref<HTMLLayout> create_layout() const;
};
