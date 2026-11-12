#pragma once

#include <godot_cpp/classes/resource.hpp>

class LatexDocument : public godot::Resource {
	GDCLASS(LatexDocument, godot::Resource)

public:
	godot::String get_text() const;
	void set_text(const godot::String& p_text);

protected:
	static void _bind_methods();

private:
	godot::String text;
};
