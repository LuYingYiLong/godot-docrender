#pragma once

#include "html_layout.h"

#include <godot_cpp/classes/resource.hpp>

class HTMLDocument : public godot::Resource {
	GDCLASS(HTMLDocument, godot::Resource)
	friend class HTMLLabel;

	godot::String text;
	godot::String base_dir = "res://";
	godot::String stylesheet;
	godot::Dictionary textures;
	godot::Dictionary local_stylesheets;
	void parse_into(HTMLLayout& p_layout) const;

protected:
	static void _bind_methods();

public:

	void set_text(const godot::String& p_text);										// 设置资源保存的 HTML 源文本
	godot::String get_text() const;													// 返回资源保存的 HTML 源文本
	
	void set_base_dir(const godot::String& p_dir);									// 设置文档的相对资源路径基准目录
	godot::String get_base_dir() const;												// 返回文档的资源基准目录
	
	void set_stylesheet(const godot::String& p_stylesheet);							// 设置文档的附加 CSS
	godot::String get_stylesheet() const;											// 返回文档的附加 CSS
	
	void set_textures(const godot::Dictionary& p_textures);							// 设置导入时收集的项目图片纹理依赖
	godot::Dictionary get_textures() const;											// 返回图片路径到纹理资源映射的副本
	
	void set_local_stylesheets(const godot::Dictionary& p_stylesheets);				// 设置导入时收集的本地样式表文本
	godot::Dictionary get_local_stylesheets() const;								// 返回本地样式表路径到 CSS 文本映射的副本
	
	godot::Ref<HTMLLayout> create_layout() const;									// 从资源当前内容创建独立的已解析布局实例
};
