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

	// signal changed() // Resource 提供的资源变更信号
	void set_text(const godot::String& p_text); // 设置资源保存的 HTML 源文本
	godot::String get_text() const; // 返回资源保存的 HTML 源文本
	void set_base_dir(const godot::String& p_dir); // 设置文档的相对资源路径基准目录
	godot::String get_base_dir() const; // 返回文档的资源基准目录
	void set_stylesheet(const godot::String& p_stylesheet); // 设置文档的附加 CSS
	godot::String get_stylesheet() const; // 返回文档的附加 CSS
	void set_textures(const godot::Dictionary& p_textures);
	godot::Dictionary get_textures() const;
	void set_local_stylesheets(const godot::Dictionary& p_stylesheets);
	godot::Dictionary get_local_stylesheets() const;
	godot::Ref<HTMLLayout> create_layout() const; // 从资源当前内容创建独立的已解析布局实例
};
