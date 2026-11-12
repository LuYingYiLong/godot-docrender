#pragma once

#include <godot_cpp/classes/translation.hpp>
#include <godot_cpp/templates/hash_map.hpp>

namespace godot {
	class DocRenderEditorTranslations {
		HashMap<StringName, Ref<Translation>> translations;

	public:
		void load();
		void unload();
	};
} // namespace godot
