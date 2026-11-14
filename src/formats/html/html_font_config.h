#pragma once

#include <godot_cpp/classes/font.hpp>

namespace docrender {
	struct HTMLFontConfig {
		godot::Ref<godot::Font> text;
		godot::Ref<godot::Font> bold;
		godot::Ref<godot::Font> italic;
		godot::Ref<godot::Font> bold_italic;
		godot::Ref<godot::Font> code;
		godot::Ref<godot::Font> code_block;
	};
}
