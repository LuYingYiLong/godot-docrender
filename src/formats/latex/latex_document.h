#pragma once

#include <godot_cpp/classes/resource.hpp>

class LatexDocument : public godot::Resource {
	GDCLASS(LatexDocument, godot::Resource)

public:
	godot::String get_text() const;													// 返回资源保存的 LaTeX 源文本
	void set_text(const godot::String& p_text);										// 更新 LaTeX 源文本并在内容改变时发出 changed 信号

protected:
	static void _bind_methods();

private:
	godot::String text;
};
