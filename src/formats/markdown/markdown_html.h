#pragma once

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <optional>
#include <string>
#include <vector>

namespace docrender {
	struct MarkdownHTMLImage {
		godot::String src;
		godot::String alt;
		godot::String title;
		godot::Vector2i size;
	};

	struct MarkdownHTMLTag {
		std::string name;
		godot::String href;
		godot::String style;
		MarkdownHTMLImage image;
		size_t end = 0;
		bool closing = false;
		bool self_closing = false;
	};

	struct MarkdownHTMLBlockRange {
		int32_t end = 0;
		bool open = false;
	};

	bool scan_markdown_html_tag(const std::string& p_text, size_t p_start, MarkdownHTMLTag& r_tag); // 从指定字节位置读取一个 HTML 标签及其属性
	std::optional<MarkdownHTMLBlockRange> scan_markdown_html_block(const std::vector<godot::String>& p_lines,
		int32_t p_start, int32_t p_limit, bool p_in_paragraph); // 识别 HTML 块并返回结束行位置及块是否尚未闭合
	size_t find_markdown_html_close(const std::string& p_text, const MarkdownHTMLTag& p_opening, size_t& r_end); // 查找配对结束标签，返回起始字节位置并写出结束位置
}
