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

	bool scan_markdown_html_tag(const std::string& p_text, size_t p_start, MarkdownHTMLTag& r_tag);
	std::optional<MarkdownHTMLBlockRange> scan_markdown_html_block(const std::vector<godot::String>& p_lines,
		int32_t p_start, int32_t p_limit, bool p_in_paragraph);
	size_t find_markdown_html_close(const std::string& p_text, const MarkdownHTMLTag& p_opening, size_t& r_end);
}
