#pragma once

#include <godot_cpp/classes/resource_format_loader.hpp>

class DocRenderTexLoader : public godot::ResourceFormatLoader {
	GDCLASS(DocRenderTexLoader, godot::ResourceFormatLoader)

public:
	bool _recognize_path(const godot::String& p_path, const godot::StringName& p_type) const override;
	godot::String _get_resource_type(const godot::String& p_path) const override;
	int64_t _get_resource_uid(const godot::String& p_path) const override;
	godot::Variant _load(const godot::String& p_path, const godot::String& p_original_path,
		bool p_use_sub_threads, int32_t p_cache_mode) const override;

protected:
	static void _bind_methods() {}
};
