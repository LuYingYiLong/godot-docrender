#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

class MarkdownDocument : public Resource {
	GDCLASS(MarkdownDocument, Resource);

private:
	String text;
	String source_path;

protected:
	static void _bind_methods();

public:
	void set_text(const String& p_text);
	String get_text() const;

	void set_source_path(const String& p_path);
	String get_source_path() const;
};
