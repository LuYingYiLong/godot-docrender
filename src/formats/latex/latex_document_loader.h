#pragma once

#include <godot_cpp/classes/resource_format_loader.hpp>

class DocRenderTexLoader : public godot::ResourceFormatLoader {
	GDCLASS(DocRenderTexLoader, godot::ResourceFormatLoader)

public:
	bool _recognize_path(
		const godot::String& p_path,
		const godot::StringName& p_type
	)const override;																// 检查路径和请求的资源类型是否匹配 LaTeX 源文件
	godot::String _get_resource_type(const godot::String& p_path) const override;	// 为识别的 .tex 源文件返回 LatexDocument 类型
	int64_t _get_resource_uid(const godot::String& p_path) const override;			// 从 .tex.import 的重映射配置读取资源 UID
	godot::Variant _load(
		const godot::String& p_path,
		const godot::String& p_original_path,
		bool p_use_sub_threads,
		int32_t p_cache_mode
	) const override;																// 加载已导入的 LaTeX 资源，或从源文本构造资源

protected:
	static void _bind_methods() {}
};
