#pragma once

#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <optional>

namespace docrender {
	struct HTMLInlineColors {
		std::optional<godot::Color> text;
		std::optional<godot::Color> background;
	};

	struct HTMLAssets {
		godot::Dictionary textures;
		godot::Dictionary stylesheets;
	};

	HTMLInlineColors parse_html_inline_colors(const godot::String& p_style); // 从行内 CSS 读取前景色和背景色
	godot::String decode_html_entities(const godot::String& p_text, bool p_attribute = false); // 按普通文本或属性值的规则解码 HTML 字符引用
	godot::String decode_html_fragment(const godot::String& p_fragment); // 解码链接片段中的百分号编码
	std::optional<godot::String> normalize_html_resource_path(const godot::String& p_path); // 规范化项目内的 res:// 路径并拒绝越界路径
	godot::String resolve_html_image_path(const godot::String& p_src, const godot::String& p_base_dir); // 按基目录解析并校验支持的本地图片路径
	godot::String resolve_html_stylesheet_path(const godot::String& p_src, const godot::String& p_base_dir); // 按基目录解析并校验本地 CSS 文件路径
	std::optional<godot::String> read_html_stylesheet(const godot::String& p_path); // 读取大小受限的本地 UTF-8 样式表
	HTMLAssets collect_html_assets(const godot::String& p_html, const godot::String& p_base_dir); // 从 HTML 和关联样式表中收集纹理与 CSS 依赖
}
