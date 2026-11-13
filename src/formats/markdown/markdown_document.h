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
	void set_text(const String& p_text);											// 设置 Markdown 源文本并发出 changed 信号
	String get_text() const;														// 返回资源保存的 Markdown 源文本

	void set_source_path(const String& p_path);										// 记录 Markdown 源文件路径，用于解析相对资源路径
	String get_source_path() const;													// 返回资源记录的 Markdown 源文件路径
};
