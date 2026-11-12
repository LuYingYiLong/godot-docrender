#pragma once

#include <godot_cpp/classes/resource.hpp>

class MermaidDocument : public godot::Resource {
	GDCLASS(MermaidDocument, godot::Resource)

public:
	godot::String get_text() const;
	void set_text(const godot::String& p_text);

protected:
	static void _bind_methods();

private:
	godot::String text;
};
