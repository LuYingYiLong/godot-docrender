#include "markdown_parser.h"
#include "markdown_html.h"

#include "formats/html/html_markup.h"

#include <algorithm>
#include <climits>
#include <cstdint>

using namespace godot;

namespace {
	template <typename Text>
	static bool is_escaped_at(const Text& p_text, int64_t p_position) {
		int64_t start = p_position;
		while (start > 0 && p_text[start - 1] == '\\') {
			start--;
		}
		return (p_position - start) % 2 != 0;
	}

	static bool is_latex_whitespace(char32_t p_character) {
		return p_character == ' ' || p_character == '\t' || p_character == '\n' || p_character == '\r';
	}

	template <typename Text>
	static int64_t find_inline_latex_end(const Text& p_text, int64_t p_start) {
		const int64_t length = p_text.length();
		if (p_start + 1 >= length || p_text[p_start + 1] == '$' || is_latex_whitespace(p_text[p_start + 1]) ||
			is_escaped_at(p_text, p_start) || (p_start > 0 && p_text[p_start - 1] == '$' && !is_escaped_at(p_text, p_start - 1))) {
			return -1;
		}

		for (int64_t i = p_start + 1; i < length; i++) {
			if (p_text[i] == '\n' || p_text[i] == '`') {
				break;
			}
			if (p_text[i] != '$' || is_escaped_at(p_text, i)) {
				continue;
			}
			const char32_t after = i + 1 < length ? p_text[i + 1] : 0;
			// Avoid treating two currency amounts as one formula.
			if (after == '$' || (after >= '0' && after <= '9') || is_latex_whitespace(p_text[i - 1])) {
				break;
			}
			return i;
		}
		return -1;
	}

	static int64_t find_display_latex_end(const String& p_text, int64_t p_start) {
		for (int64_t i = p_text.find("$$", p_start); i >= 0; i = p_text.find("$$", i + 2)) {
			if ((i == 0 || p_text[i - 1] != '$') && (i + 2 == p_text.length() || p_text[i + 2] != '$') &&
				!is_escaped_at(p_text, i)) {
				return i;
			}
		}
		return -1;
	}

	static bool is_blank_line(const String& p_line) {
		return p_line.strip_edges().is_empty();
	}

	static int32_t get_indent(const String& p_line) {
		int32_t indent = 0;

		while (indent < p_line.length()) {
			const char32_t character = p_line[indent];
			if (character != ' ' && character != '\t') {
				break;
			}
			indent++;
		}

		return indent;
	}

	static String trim_left_indent(const String& p_line) {
		return p_line.substr(get_indent(p_line));
	}

	static bool is_footnote_id_valid(const String& p_id) {
		if (p_id.is_empty()) {
			return false;
		}
		for (int32_t i = 0; i < p_id.length(); i++) {
			if (p_id[i] == ' ' || p_id[i] == '\t' || p_id[i] == '\n') {
				return false;
			}
		}
		return true;
	}

	static bool is_footnote_definition_start(const String& p_line, String& r_id, String& r_text) {
		if (get_indent(p_line) > 3) {
			return false;
		}

		const String trimmed_left = trim_left_indent(p_line);
		if (!trimmed_left.begins_with("[^")) {
			return false;
		}

		const int32_t close_pos = trimmed_left.find("]");
		if (close_pos <= 2 || close_pos + 1 >= trimmed_left.length() || trimmed_left[close_pos + 1] != ':') {
			return false;
		}

		const String id = trimmed_left.substr(2, close_pos - 2);
		if (!is_footnote_id_valid(id)) {
			return false;
		}

		r_id = id;
		r_text = trimmed_left.substr(close_pos + 2).strip_edges();
		return true;
	}

	static bool is_footnote_continuation_line(const String& p_line, String& r_text) {
		if (is_blank_line(p_line)) {
			r_text = String();
			return true;
		}

		const int32_t indent = get_indent(p_line);
		if (indent < 4) {
			return false;
		}

		r_text = p_line.substr(std::min<int32_t>(4, p_line.length())).rstrip(" \t");
		return true;
	}

	static String join_wrapped_line(const String& p_current, const String& p_next) {
		if (p_current.is_empty()) {
			return p_next.strip_edges();
		}

		return p_current + String(" ") + p_next.strip_edges();
	}

	static bool is_atx_heading(const String& p_line, String& r_text, int32_t& r_level) {
		int32_t count = 0;
		int32_t i = 0;

		while (i < p_line.length() && p_line[i] == '#') {
			count++;
			i++;
		}

		if (count < 1 || count > 6) {
			return false;
		}

		r_level = count;

		if (i < p_line.length()) {
			if (p_line[i] != ' ' && p_line[i] != '\t') {
				return false;
			}
			i++;
		}

		String content = p_line.substr(i).strip_edges();
		content = content.rstrip("#").rstrip(" \t");
		r_text = content;
		return true;
	}

	static bool is_setext_heading_underline(const String& p_line, char32_t& r_marker) {
		const String stripped = p_line.strip_edges();
		if (stripped.length() < 2) {
			return false;
		}

		const char32_t first = stripped[0];
		if (first != '=' && first != '-') {
			return false;
		}

		for (int32_t i = 1; i < stripped.length(); i++) {
			if (stripped[i] != first) {
				return false;
			}
		}

		r_marker = first;
		return true;
	}

	static bool is_thematic_break(const String& p_line) {
		const String stripped = p_line.strip_edges();
		if (stripped.length() < 3) {
			return false;
		}

		const char32_t marker = stripped[0];
		if (marker != '-' && marker != '*' && marker != '_') {
			return false;
		}

		int32_t count = 0;
		for (int32_t i = 0; i < stripped.length(); i++) {
			if (stripped[i] == marker) {
				count++;
			}
			else if (stripped[i] != ' ') {
				return false;
			}
		}

		return count >= 3;
	}

	static bool is_fenced_code_open(const String& p_line, String& r_fence_char, int32_t& r_fence_count, String& r_language, int32_t& r_indent) {
		r_indent = get_indent(p_line);
		if (r_indent > 3) {
			return false;
		}

		const String trimmed = trim_left_indent(p_line);
		if (trimmed.length() < 3) {
			return false;
		}

		const char32_t fence_char = trimmed[0];
		if (fence_char != '`' && fence_char != '~') {
			return false;
		}

		int32_t count = 0;
		while (count < trimmed.length() && trimmed[count] == fence_char) {
			count++;
		}

		if (count < 3) {
			return false;
		}

		r_fence_char = String::chr(fence_char);
		r_fence_count = count;

		String after_fence = trimmed.substr(count);
		r_language = after_fence.strip_edges();
		return true;
	}

	static bool is_fenced_code_close(const String& p_line, const String& p_fence_char, int32_t p_min_count) {
		const int32_t indent = get_indent(p_line);
		if (indent > 3) {
			return false;
		}

		const String trimmed = trim_left_indent(p_line);
		if (trimmed.length() < static_cast<int64_t>(p_min_count)) {
			return false;
		}

		const char32_t fence_char = p_fence_char[0];

		int32_t count = 0;
		while (count < trimmed.length() && trimmed[count] == fence_char) {
			count++;
		}

		if (count < p_min_count) {
			return false;
		}

		for (int32_t i = count; i < trimmed.length(); i++) {
			if (trimmed[i] != ' ' && trimmed[i] != '\t') {
				return false;
			}
		}

		return true;
	}

	static bool is_blockquote_line(const String& p_line, String& r_content) {
		const String trimmed_left = trim_left_indent(p_line);
		if (!trimmed_left.begins_with(">")) {
			return false;
		}

		int32_t pos = 1;
		if (pos < trimmed_left.length() && (trimmed_left[pos] == ' ' || trimmed_left[pos] == '\t')) {
			pos++;
		}

		r_content = trimmed_left.substr(pos);
		return true;
	}

	static int32_t get_blockquote_depth(const String& p_line) {
		const String trimmed_left = trim_left_indent(p_line);
		int32_t depth = 0;
		int32_t pos = 0;

		while (pos < trimmed_left.length() && trimmed_left[pos] == '>') {
			depth++;
			pos++;
			if (pos < trimmed_left.length() && (trimmed_left[pos] == ' ' || trimmed_left[pos] == '\t')) {
				pos++;
			}
		}

		return depth;
	}

	static bool is_unordered_list_item(const String& p_line, String& r_item) {
		const String trimmed_left = trim_left_indent(p_line);
		if (trimmed_left.length() < 2) {
			return false;
		}

		const char32_t marker = trimmed_left[0];
		if ((marker != '-' && marker != '*' && marker != '+') || trimmed_left[1] != ' ') {
			return false;
		}

		r_item = trimmed_left.substr(2).strip_edges();
		return true;
	}

	static bool is_ordered_list_item(const String& p_line, String& r_item, int32_t& r_number) {
		const String trimmed_left = trim_left_indent(p_line);
		if (trimmed_left.length() < 3) {
			return false;
		}

		int32_t i = 0;
		int32_t number = 0;
		while (i < trimmed_left.length() && trimmed_left[i] >= '0' && trimmed_left[i] <= '9') {
			number = number * 10 + static_cast<int32_t>(trimmed_left[i] - '0');
			i++;
		}

		if (i == 0 || i + 2 > trimmed_left.length()) {
			return false;
		}

		if (trimmed_left[i] != '.' || trimmed_left[i + 1] != ' ') {
			return false;
		}

		r_number = std::max<int32_t>(1, number);
		r_item = trimmed_left.substr(i + 2).strip_edges();
		return true;
	}

	static bool is_task_list_item(const String& p_line, String& r_item, bool& r_checked) {
		const String trimmed_left = trim_left_indent(p_line);
		if (trimmed_left.length() < 5) {
			return false;
		}

		const char32_t marker = trimmed_left[0];
		if ((marker != '-' && marker != '*' && marker != '+') || trimmed_left[1] != ' ') {
			return false;
		}

		if (trimmed_left[2] != '[' || trimmed_left[4] != ']') {
			return false;
		}

		const char32_t check_char = trimmed_left[3];
		if (check_char == 'x' || check_char == 'X') {
			r_checked = true;
		}
		else if (check_char == ' ') {
			r_checked = false;
		}
		else {
			return false;
		}

		if (trimmed_left.length() <= 5 || trimmed_left[5] != ' ') {
			return false;
		}

		r_item = trimmed_left.substr(6).strip_edges();
		return true;
	}

	static bool is_lazy_continuation(const std::vector<String>& p_lines, int32_t p_index) {
		if (p_index >= static_cast<int32_t>(p_lines.size())) {
			return false;
		}

		const String line = p_lines[p_index];
		if (is_blank_line(line)) {
			return false;
		}

		String dummy_text;
		int32_t dummy_level;

		if (is_atx_heading(line, dummy_text, dummy_level)) {
			return false;
		}
		if (is_thematic_break(line)) {
			return false;
		}

		String dummy_fence_char;
		int32_t dummy_fence_count;
		String dummy_language;
		int32_t dummy_indent;
		if (is_fenced_code_open(line, dummy_fence_char, dummy_fence_count, dummy_language, dummy_indent)) {
			return false;
		}

		String dummy_item;
		int32_t dummy_number = 1;
		if (is_unordered_list_item(line, dummy_item) || is_ordered_list_item(line, dummy_item, dummy_number)) {
			return false;
		}
		if (is_blockquote_line(line, dummy_item)) {
			return false;
		}

		return !is_blank_line(line);
	}

	struct ListMarker {
		int32_t indent = 0;
		int32_t content_column = 0;
		int32_t start_number = 1;
		char32_t marker = 0;
		MarkdownBlockType type = MARKDOWN_BLOCK_UNORDERED_LIST;
		bool checked = false;
		String text;
	};

	static int32_t list_indent_columns(const String& p_line) {
		int32_t columns = 0;
		const int32_t indent = get_indent(p_line);
		for (int32_t i = 0; i < indent; ++i) {
			columns += p_line[i] == '\t' ? 4 - columns % 4 : 1;
		}
		return columns;
	}

	static std::optional<ListMarker> read_list_marker(const String& p_line, bool p_gfm) {
		ListMarker item;
		const String trimmed = trim_left_indent(p_line);
		if (trimmed.is_empty()) {
			return std::nullopt;
		}
		item.indent = list_indent_columns(p_line);
		int32_t end = 0;
		while (end < trimmed.length() && end < 9 && trimmed[end] >= '0' && trimmed[end] <= '9') {
			++end;
		}
		if (end > 0 && end < trimmed.length() && (trimmed[end] == '.' || trimmed[end] == ')')) {
			item.type = MARKDOWN_BLOCK_ORDERED_LIST;
			item.start_number = static_cast<int32_t>(trimmed.substr(0, end).to_int());
			item.marker = trimmed[end++];
		}
		else if (end == 0 && (trimmed[0] == '-' || trimmed[0] == '+' || trimmed[0] == '*') && !is_thematic_break(p_line)) {
			item.marker = trimmed[end++];
		}
		else {
			return std::nullopt;
		}
		if (end < trimmed.length() && trimmed[end] != ' ' && trimmed[end] != '\t') {
			return std::nullopt;
		}
		int32_t padding = 0;
		int32_t text_start = end;
		while (text_start < trimmed.length() && (trimmed[text_start] == ' ' || trimmed[text_start] == '\t')) {
			padding += trimmed[text_start++] == '\t' ? 4 - ((item.indent + end + padding) % 4) : 1;
		}
		if (text_start == trimmed.length()) {
			padding = 1;
		}
		else if (padding > 4) {
			padding = 1;
			text_start = end + 1;
		}
		item.content_column = item.indent + end + padding;
		item.text = trimmed.substr(text_start);
		if (item.type == MARKDOWN_BLOCK_UNORDERED_LIST && p_gfm && item.text.length() >= 4 &&
			item.text[0] == '[' && item.text[2] == ']' && (item.text[3] == ' ' || item.text[3] == '\t')) {
			const char32_t check = item.text[1];
			if (check == ' ' || check == 'x' || check == 'X') {
				item.type = MARKDOWN_BLOCK_TASK_LIST;
				item.checked = check != ' ';
				item.text = item.text.substr(4);
			}
		}
		return item;
	}

	static String strip_list_indent(const String& p_line, int32_t p_columns) {
		int32_t index = 0;
		int32_t columns = 0;
		while (index < p_line.length() && columns < p_columns) {
			const char32_t c = p_line[index++];
			columns += c == '\t' ? 4 - columns % 4 : 1;
		}
		return String(" ").repeat(std::max(0, columns - p_columns)) + p_line.substr(index);
	}

	static bool has_html_block(const std::vector<MarkdownBlock>& p_blocks) {
		for (const MarkdownBlock& block : p_blocks) {
			if (block.type == MARKDOWN_BLOCK_HTML || !block.list_items.empty() || has_html_block(block.children)) {
				return true;
			}
		}
		return false;
	}

	// The regular list path remains useful for text-only items. Collect child blocks
	// when a list contains raw HTML, preserving the list's marker and source ranges.
	static std::optional<MarkdownBlock> parse_html_list(const std::vector<String>& p_lines, int32_t p_start,
		int32_t p_limit, const MarkdownParseOptions& p_options, int32_t& r_scan_end) {
		const auto first = read_list_marker(p_lines[p_start], p_options.parse_gfm);
		if (!first) {
			return std::nullopt;
		}
		MarkdownBlock list;
		list.line = p_start + 1;
		list.type = first->type;
		list.list_start = first->start_number;
		list.indent = first->indent / 2;
		int32_t index = p_start;
		bool contains_html = false;
		bool contains_continuation = false;
		while (index < p_limit) {
			const auto marker = read_list_marker(p_lines[index], p_options.parse_gfm);
			if (!marker || marker->indent != first->indent || marker->marker != first->marker || marker->type != first->type) {
				break;
			}
			MarkdownListItem item;
			item.line = index + 1;
			item.checked = marker->checked;
			String source = marker->text;
			++index;
			while (index < p_limit) {
				int32_t next = index;
				while (next < p_limit && is_blank_line(p_lines[next])) {
					++next;
				}
				if (next == p_limit || list_indent_columns(p_lines[next]) < marker->content_column) {
					break;
				}
				contains_continuation = true;
				while (index < next) {
					source += "\n";
					++index;
				}
				source += "\n";
				source += strip_list_indent(p_lines[index], marker->content_column);
				++index;
			}
			item.end_line = index;
			item.children = parse_markdown_blocks(source, nullptr, INT_MAX, p_options);
			contains_html = contains_html || has_html_block(item.children);
			list.list_items.push_back(std::move(item));
			list.end_line = index;
			while (index < p_limit && is_blank_line(p_lines[index])) {
				++index;
			}
		}
		r_scan_end = index;
		if (!contains_html && !contains_continuation) {
			return std::nullopt;
		}
		return list;
	}

	static String strip_code_fence_indent(const String& p_text, int32_t p_indent) {
		if (p_indent <= 0 || p_text.is_empty()) {
			return p_text;
		}

		const int32_t indent = std::min<int32_t>(p_indent, 3);

		PackedStringArray lines = p_text.split("\n");
		String result;

		for (int32_t i = 0; i < lines.size(); i++) {
			String line = lines[i];
			if (line.is_empty()) {
				result += "\n";
				continue;
			}

			int32_t line_indent = 0;
			while (line_indent < indent && line_indent < line.length() && line[line_indent] == ' ') {
				line_indent++;
			}

			line = line.substr(line_indent);

			if (!result.is_empty()) {
				result += "\n";
			}
			result += line;
		}

		return result;
	}

	static String slugify_once(const String& p_text) {
		String result;

		for (int32_t i = 0; i < p_text.length(); i++) {
			const char32_t c = p_text[i];
			if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
				result += String::chr(c);
			}
			else if (c >= 'A' && c <= 'Z') {
				result += String::chr(c + 32);
			}
			else if (c == ' ' || c == '\t' || c == '-' || c == '_') {
				if (result.is_empty() || result[result.length() - 1] != '-') {
					result += '-';
				}
			}
		}

		return result.rstrip("-");
	}

} // namespace

std::vector<String> split_lines(const String& p_text, bool p_keep_trailing_space) {
	const String normalized = p_text.replace("\r\n", "\n").replace("\r", "\n");
	const PackedStringArray split = normalized.split("\n", true);
	std::vector<String> lines;

	lines.reserve(split.size());
	for (int32_t i = 0; i < split.size(); i++) {
		lines.push_back(p_keep_trailing_space ? split[i] : split[i].rstrip(" \t"));
	}

	return lines;
}

String slugify_heading(const String& p_text) {
	return slugify_once(p_text);
}

uint64_t compute_block_hash(const MarkdownBlock& p_block) {
	String to_hash;
	to_hash += String::num_int64(static_cast<int64_t>(p_block.type));
	to_hash += "|";
	to_hash += p_block.text;
	to_hash += "|";
	to_hash += p_block.argument;
	to_hash += "|";
	to_hash += String::num_int64(p_block.level);
	to_hash += "|";
	to_hash += String::num_int64(p_block.indent);
	to_hash += "|";
	to_hash += String::num_int64(p_block.list_start);
	if (p_block.type == MARKDOWN_BLOCK_CODE) {
		to_hash += p_block.end_line > 0 ? "|closed" : "|open";
	}
	for (int32_t i = 0; i < p_block.items.size(); i++) {
		to_hash += "|i" + p_block.items[i];
	}
	for (const MarkdownFootnoteDefinition& footnote : p_block.footnotes) {
		to_hash += "|f" + footnote.id + ":" + footnote.text;
	}
	for (int32_t i = 0; i < static_cast<int32_t>(p_block.item_levels.size()); i++) {
		to_hash += "|l" + String::num_int64(p_block.item_levels[i]);
	}
	for (const MarkdownBlock& child : p_block.children) {
		to_hash += "|c" + String::num_uint64(compute_block_hash(child));
	}
	for (const MarkdownListItem& item : p_block.list_items) {
		to_hash += item.checked ? "|checked" : "|item";
		for (const MarkdownBlock& child : item.children) {
			to_hash += "|c" + String::num_uint64(compute_block_hash(child));
		}
	}

	return to_hash.hash();
}

MarkdownInlineSpanParseResult parse_inline_spans(const String& p_text, const MarkdownParseOptions& p_options) {
	MarkdownInlineSpanParseResult result;
	if (p_text.is_empty()) {
		return result;
	}

	const std::string raw = p_text.utf8().get_data();
	const size_t len = raw.size();
	String& output = result.output_text;

	auto append_span = [&result](const MarkdownInlineSpan& span) {
		if (!result.spans.empty()) {
			const MarkdownInlineSpan& last = result.spans.back();
			if (last.latex.is_empty() && span.latex.is_empty() && !last.html_image && !span.html_image && last.link_uri == span.link_uri && last.image_uri == span.image_uri && last.image_title == span.image_title && last.footnote_id == span.footnote_id && last.footnote_number == span.footnote_number && last.footnote_ref == span.footnote_ref && last.bold == span.bold && last.italic == span.italic && last.code == span.code && last.strikethrough == span.strikethrough && last.highlight == span.highlight && last.custom_color == span.custom_color && (!last.custom_color || last.color == span.color) &&
				last.custom_background == span.custom_background && (!last.custom_background || last.background_color == span.background_color) && last.end == span.start) {
				result.spans.back().text += span.text;
				result.spans.back().end = span.end;
				return;
			}
		}
		result.spans.push_back(span);
		};

	auto emit_plain = [&](size_t start, size_t end, bool p_decode = true) {
		if (end <= start) {
			return;
		}
		MarkdownInlineSpan span;
		span.text = String::utf8(raw.data() + start, static_cast<int64_t>(end - start));
		if (p_options.parse_html && p_decode) {
			span.text = docrender::decode_html_entities(span.text);
		}
		span.start = static_cast<int64_t>(output.length());
		output += span.text;
		span.end = static_cast<int64_t>(output.length());
		append_span(span);
		};

	auto skip_inline_latex = [&](size_t& r_pos) {
		if (p_options.parse_latex && raw[r_pos] == '$') {
			const int64_t end = find_inline_latex_end(raw, r_pos);
			if (end >= 0) {
				r_pos = static_cast<size_t>(end);
				return true;
			}
		}
		return false;
	};

	auto find_format_close = [&](size_t start, const std::string& marker) -> size_t {
		if (!p_options.parse_latex && !p_options.parse_html) {
			return raw.find(marker, start);
		}
		for (size_t i = start; i < len; i++) {
			if (skip_inline_latex(i)) {
				continue;
			}
			docrender::MarkdownHTMLTag tag;
			if (p_options.parse_html && raw[i] == '<' && docrender::scan_markdown_html_tag(raw, i, tag)) {
				i = tag.end - 1;
				continue;
			}
			if (raw.compare(i, marker.length(), marker) == 0) {
				return i;
			}
		}
		return std::string::npos;
	};

	auto find_str = [&](size_t start, const std::string& needle) -> size_t {
		if (start >= len) {
			return std::string::npos;
		}
		size_t pos = raw.find(needle, start);
		return pos;
		};

	auto find_bracket_close = [&](size_t start) -> size_t {
		int32_t bracket_depth = 1;
		for (size_t i = start; i < len; i++) {
			if (skip_inline_latex(i)) {
				continue;
			}
			docrender::MarkdownHTMLTag tag;
			if (p_options.parse_html && raw[i] == '<' && docrender::scan_markdown_html_tag(raw, i, tag)) {
				i = tag.end - 1;
				continue;
			}
			unsigned char byte = static_cast<unsigned char>(raw[i]);
			if (byte >= 0x80) {
				continue;
			}
			if (raw[i] == '[') {
				bracket_depth++;
			}
			else if (raw[i] == ']') {
				bracket_depth--;
				if (bracket_depth == 0) {
					return i;
				}
			}
		}
		return std::string::npos;
		};

	auto find_url_close = [&](size_t start) -> size_t {
		for (size_t i = start; i < len; i++) {
			unsigned char byte = static_cast<unsigned char>(raw[i]);
			if (byte >= 0x80) {
				continue;
			}
			if (raw[i] == ')') {
				return i;
			}
		}
		return std::string::npos;
		};

	size_t pos = 0;
	while (pos < len) {
		unsigned char byte = static_cast<unsigned char>(raw[pos]);
		if (byte >= 0x80) {
			size_t end = pos;
			while (end < len && static_cast<unsigned char>(raw[end]) >= 0x80) {
				end++;
			}
			emit_plain(pos, end);
			pos = end;
			continue;
		}

		if (raw[pos] == '\\' && pos + 1 < len) {
			const char32_t next = raw[pos + 1];
			if (next == '\\' || next == '`' || next == '*' || next == '_' || next == '{' || next == '}' || next == '[' || next == ']' || next == '(' || next == ')' || next == '#' || next == '+' || next == '-' || next == '.' || next == '!' || next == '|' || (p_options.parse_latex && next == '$') || (p_options.parse_html && (next == '<' || next == '&'))) {
				emit_plain(pos + 1, pos + 2);
				pos += 2;
				continue;
			}
		}

		if (p_options.parse_html && raw[pos] == '<') {
			docrender::MarkdownHTMLTag tag;
			if (docrender::scan_markdown_html_tag(raw, pos, tag)) {
				const std::string& name = tag.name;
				if (!tag.closing && name == "img") {
					MarkdownInlineSpan span;
					span.html_image = true;
					span.image_uri = docrender::decode_html_entities(tag.image.src, true);
					span.image_title = docrender::decode_html_entities(tag.image.title, true);
					span.image_size = tag.image.size;
					span.text = docrender::decode_html_entities(tag.image.alt, true);
					if (span.text.is_empty()) {
						span.text = String::chr(0xfffc);
					}
					span.start = output.length();
					output += span.text;
					span.end = output.length();
					append_span(span);
					pos = tag.end;
					continue;
				}
				if (!tag.closing && name == "br") {
					MarkdownInlineSpan span;
					span.text = "\n";
					span.start = output.length();
					output += span.text;
					span.end = output.length();
					append_span(span);
					pos = tag.end;
					continue;
				}
				const bool bold = name == "b" || name == "strong";
				const bool italic = name == "i" || name == "em";
				const bool strike = name == "s" || name == "del";
				const bool code = name == "code";
				const bool highlight = name == "mark";
				const bool link = name == "a" && !tag.href.is_empty();
				const bool styled = name == "span";
				size_t close_end = 0;
				const size_t close = !tag.closing && !tag.self_closing && (bold || italic || strike || code || highlight || link || styled)
					? docrender::find_markdown_html_close(raw, tag, close_end) : std::string::npos;
				if (close == std::string::npos) {
					emit_plain(pos, tag.end, false);
					pos = tag.end;
					continue;
				}

				const String inner = String::utf8(raw.data() + tag.end, close - tag.end);
				MarkdownInlineSpanParseResult parsed;
				if (code) {
					parsed.output_text = docrender::decode_html_entities(inner);
					MarkdownInlineSpan span;
					span.text = parsed.output_text;
					span.end = span.text.length();
					parsed.spans.push_back(span);
				}
				else {
					parsed = parse_inline_spans(inner, p_options);
				}
				const docrender::HTMLInlineColors colors = styled
					? docrender::parse_html_inline_colors(docrender::decode_html_entities(tag.style, true)) : docrender::HTMLInlineColors{};
				const String uri = link ? docrender::decode_html_entities(tag.href, true) : String();
				const int64_t start = output.length();
				output += parsed.output_text;
				for (MarkdownInlineSpan& span : parsed.spans) {
					span.start += start;
					span.end += start;
					span.bold |= bold;
					span.italic |= italic;
					span.strikethrough |= strike;
					span.code |= code;
					span.highlight |= highlight;
					if (link && span.link_uri.is_empty()) {
						span.link_uri = uri;
					}
					if (colors.text && !span.custom_color) {
						span.custom_color = true;
						span.color = *colors.text;
					}
					if (colors.background && !span.custom_background) {
						span.custom_background = true;
						span.background_color = *colors.background;
					}
					append_span(span);
				}
				pos = close_end;
				continue;
			}
		}

		if (p_options.parse_latex && raw[pos] == '$') {
			const int64_t close = find_inline_latex_end(raw, pos);
			if (close >= 0) {
				MarkdownInlineSpan span;
				span.latex = String::utf8(raw.data() + pos + 1, close - static_cast<int64_t>(pos) - 1);
				span.text = String::utf8(raw.data() + pos, close - static_cast<int64_t>(pos) + 1);
				span.start = output.length();
				output += span.text;
				span.end = output.length();
				append_span(span);
				pos = static_cast<size_t>(close + 1);
				continue;
			}
		}

		if (pos + 3 < len && raw[pos] == '[' && raw[pos + 1] == '^') {
			size_t close_pos = pos + 2;
			while (close_pos < len && raw[close_pos] != ']' && raw[close_pos] != ' ' && raw[close_pos] != '\t' && raw[close_pos] != '\n') {
				close_pos++;
			}
			if (close_pos < len && raw[close_pos] == ']' && close_pos > pos + 2) {
				MarkdownInlineSpan span;
				span.footnote_ref = true;
				span.footnote_id = String::utf8(raw.data() + pos + 2, static_cast<int64_t>(close_pos - pos - 2));
				span.text = String::chr(0xfffc);
				span.start = static_cast<int64_t>(output.length());
				output += span.text;
				span.end = static_cast<int64_t>(output.length());
				append_span(span);
				pos = close_pos + 1;
				continue;
			}
		}

		if (pos + 1 < len && raw[pos] == '!' && raw[pos + 1] == '[') {
			size_t bracket_end = find_bracket_close(pos + 2);
			if (bracket_end != std::string::npos && bracket_end + 1 < len && raw[bracket_end + 1] == '(') {
				size_t url_end = find_url_close(bracket_end + 2);
				if (url_end != std::string::npos) {
					MarkdownInlineSpan span;
					String full_url = String::utf8(raw.data() + bracket_end + 2, static_cast<int64_t>(url_end - bracket_end - 2)).strip_edges();
					int64_t title_start = -1;
					for (int64_t ui = 0; ui < full_url.length(); ui++) {
						if (full_url[ui] == '"') {
							title_start = ui;
							break;
						}
					}
					if (title_start >= 0) {
						span.image_uri = full_url.substr(0, static_cast<int64_t>(title_start)).strip_edges();
						int64_t title_end = full_url.rfind("\"");
						if (title_end > title_start) {
							span.image_title = full_url.substr(title_start + 1, title_end - title_start - 1);
						}
					}
					else {
						span.image_uri = full_url;
					}
					String alt_text = String::utf8(raw.data() + pos + 2, static_cast<int64_t>(bracket_end - pos - 2));
					const String display_text = alt_text.is_empty() ? String::chr(0xfffc) : alt_text;
					span.text = display_text;
					output += display_text;
					span.start = static_cast<int64_t>(output.length()) - static_cast<int64_t>(display_text.length());
					span.end = static_cast<int64_t>(output.length());
					append_span(span);
					pos = url_end + 1;
					continue;
				}
			}
			emit_plain(pos, pos + 1);
			pos++;
			continue;
		}

		// Autolink: <url> or <email>
		if (raw[pos] == '<') {
			size_t gt = pos + 1;
			while (gt < len && raw[gt] != '>' && raw[gt] != '\n' && raw[gt] != ' ' && raw[gt] != '<') gt++;
			if (gt < len && raw[gt] == '>' && gt > pos + 1) {
				String url = String::utf8(raw.data() + pos + 1, static_cast<int64_t>(gt - pos - 1));
				bool is_url_or_email = false;
				if (url.contains("://") || (p_options.parse_gfm && url.begins_with("www."))) is_url_or_email = true;
				if (url.contains("@") && url.contains(".")) is_url_or_email = true;
				if (is_url_or_email) {
					MarkdownInlineSpan span;
					span.text = url;
					span.link_uri = url;
					span.start = static_cast<int64_t>(output.length());
					output += url;
					span.end = static_cast<int64_t>(output.length());
					append_span(span);
					pos = gt + 1;
					continue;
				}
			}
			emit_plain(pos, pos + 1);
			pos++;
			continue;
		}

		if (raw[pos] == '[') {
			size_t bracket_end = find_bracket_close(pos + 1);
			if (bracket_end != std::string::npos && bracket_end > pos + 1 && bracket_end + 1 < len && raw[bracket_end + 1] == '(') {
				size_t url_end = find_url_close(bracket_end + 2);
				if (url_end != std::string::npos && url_end > bracket_end + 2) {
					const String link_text = String::utf8(raw.data() + pos + 1, static_cast<int64_t>(bracket_end - pos - 1));
					const String link_uri = String::utf8(raw.data() + bracket_end + 2, static_cast<int64_t>(url_end - bracket_end - 2)).strip_edges();
					MarkdownInlineSpanParseResult inner_result = parse_inline_spans(link_text, p_options);
					if (inner_result.spans.empty()) {
						MarkdownInlineSpan span;
						span.text = inner_result.output_text.is_empty() ? link_text : inner_result.output_text;
						span.link_uri = link_uri;
						span.start = static_cast<int64_t>(output.length());
						output += span.text;
						span.end = static_cast<int64_t>(output.length());
						append_span(span);
					}
					else {
						const int64_t output_start = static_cast<int64_t>(output.length());
						output += inner_result.output_text;
						for (MarkdownInlineSpan& span : inner_result.spans) {
							span.link_uri = link_uri;
							span.start += output_start;
							span.end += output_start;
							append_span(span);
						}
					}
					pos = url_end + 1;
					continue;
				}
			}
			emit_plain(pos, pos + 1);
			pos++;
			continue;
		}

		if (p_options.parse_gfm && raw[pos] == '~' && pos + 1 < len && raw[pos + 1] == '~') {
			const size_t end = find_format_close(pos + 2, "~~");
			if (end != std::string::npos) {
				size_t inner_start = pos + 2;
				String inner = String::utf8(raw.data() + inner_start, static_cast<int64_t>(end - inner_start));
				MarkdownInlineSpanParseResult inner_result = parse_inline_spans(inner, p_options);
				for (MarkdownInlineSpan& span : inner_result.spans) {
					span.strikethrough = true;
					span.start += static_cast<int64_t>(output.length());
					span.end += static_cast<int64_t>(output.length());
				}
				output += inner_result.output_text;
				for (const MarkdownInlineSpan& span : inner_result.spans) {
					append_span(span);
				}
				pos = end + 2;
				continue;
			}
			emit_plain(pos, pos + 2);
			pos += 2;
			continue;
		}

		if (raw[pos] == '=' && pos + 1 < len && raw[pos + 1] == '=') {
			const size_t end = find_format_close(pos + 2, "==");
			if (end != std::string::npos) {
				size_t inner_start = pos + 2;
				String inner = String::utf8(raw.data() + inner_start, static_cast<int64_t>(end - inner_start));
				MarkdownInlineSpanParseResult inner_result = parse_inline_spans(inner, p_options);
				for (MarkdownInlineSpan& span : inner_result.spans) {
					span.highlight = true;
					span.start += static_cast<int64_t>(output.length());
					span.end += static_cast<int64_t>(output.length());
				}
				output += inner_result.output_text;
				for (const MarkdownInlineSpan& span : inner_result.spans) {
					append_span(span);
				}
				pos = end + 2;
				continue;
			}
			emit_plain(pos, pos + 2);
			pos += 2;
			continue;
		}

		if (raw[pos] == '`') {
			size_t end = pos + 1;
			while (end < len && raw[end] == '`') {
				end++;
			}
			int32_t backtick_count = static_cast<int32_t>(end - pos);
			std::string closer(backtick_count, '`');
			size_t close_pos = find_str(end, closer);
			if (close_pos != std::string::npos) {
				bool valid_close = true;
				for (size_t i = close_pos + backtick_count; i < len; i++) {
					if (raw[i] == '`') {
						valid_close = false;
						break;
					}
					if (raw[i] != ' ') {
						break;
					}
				}
				if (valid_close) {
					String code_text = String::utf8(raw.data() + end, static_cast<int64_t>(close_pos - end));
					MarkdownInlineSpan span;
					span.text = code_text;
					span.code = true;
					span.start = static_cast<int64_t>(output.length());
					output += span.text;
					span.end = static_cast<int64_t>(output.length());
					append_span(span);
					pos = close_pos + backtick_count;
					continue;
				}
			}
			emit_plain(pos, static_cast<size_t>(end));
			pos = end;
			continue;
		}

		if (pos + 2 < len && raw[pos] == '*' && raw[pos + 1] == '*' && raw[pos + 2] == '*' && (pos + 3 >= len || raw[pos + 3] != '*')) {
			size_t close_pos = find_format_close(pos + 3, "***");
			if (close_pos != std::string::npos && close_pos > pos + 3) {
				String inner = String::utf8(raw.data() + pos + 3, static_cast<int64_t>(close_pos - pos - 3));
				MarkdownInlineSpanParseResult inner_result = parse_inline_spans(inner, p_options);
				for (MarkdownInlineSpan& span : inner_result.spans) {
					span.bold = true;
					span.italic = true;
					span.start += static_cast<int64_t>(output.length());
					span.end += static_cast<int64_t>(output.length());
				}
				output += inner_result.output_text;
				for (const MarkdownInlineSpan& span : inner_result.spans) {
					append_span(span);
				}
				pos = close_pos + 3;
				continue;
			}
		}

		if (pos + 1 < len && raw[pos] == '*' && raw[pos + 1] == '*') {
			size_t close_pos = find_format_close(pos + 2, "**");
			if (close_pos != std::string::npos && close_pos > pos + 2) {
				String inner = String::utf8(raw.data() + pos + 2, static_cast<int64_t>(close_pos - pos - 2));
				MarkdownInlineSpanParseResult inner_result = parse_inline_spans(inner, p_options);
				for (MarkdownInlineSpan& span : inner_result.spans) {
					span.bold = true;
					span.start += static_cast<int64_t>(output.length());
					span.end += static_cast<int64_t>(output.length());
				}
				output += inner_result.output_text;
				for (const MarkdownInlineSpan& span : inner_result.spans) {
					append_span(span);
				}
				pos = close_pos + 2;
				continue;
			}
		}

		if (raw[pos] == '*') {
			size_t close_pos = find_format_close(pos + 1, "*");
			if (close_pos < len && close_pos > pos + 1) {
				String inner = String::utf8(raw.data() + pos + 1, static_cast<int64_t>(close_pos - pos - 1));
				MarkdownInlineSpanParseResult inner_result = parse_inline_spans(inner, p_options);
				for (MarkdownInlineSpan& span : inner_result.spans) {
					span.italic = true;
					span.start += static_cast<int64_t>(output.length());
					span.end += static_cast<int64_t>(output.length());
				}
				output += inner_result.output_text;
				for (const MarkdownInlineSpan& span : inner_result.spans) {
					append_span(span);
				}
				pos = close_pos + 1;
				continue;
			}
		}

		size_t next_special = len;
		for (size_t i = pos + 1; i < len; i++) {
			unsigned char b = static_cast<unsigned char>(raw[i]);
			if (b >= 0x80) {
				continue;
			}
			if (raw[i] == '\\' || raw[i] == '*' || raw[i] == '[' || raw[i] == '!' || raw[i] == '`' || raw[i] == '=' || raw[i] == '<' ||
				(p_options.parse_gfm && raw[i] == '~') || (p_options.parse_latex && raw[i] == '$')) {
				next_special = i;
				break;
			}
		}
		emit_plain(pos, next_special);
		pos = next_special;
	}

	return result;
}

MarkdownParseResult parse_markdown_document(const String& p_text, const MarkdownParserState* p_initial_state, int32_t p_max_lines, const MarkdownParseOptions& p_options) {
	std::vector<String> lines = split_lines(p_text);
	const std::vector<String> html_source_lines = p_options.parse_html ? split_lines(p_text, true) : std::vector<String>();
	MarkdownParseResult result;
	std::vector<MarkdownBlock>& blocks = result.blocks;

	MarkdownParserState state;
	if (p_initial_state != nullptr) {
		state = *p_initial_state;
	}

	String paragraph_accumulator;
	int32_t paragraph_line = 0;
	String code_accumulator;
	int32_t code_line = 0;
	std::vector<String> blockquote_lines;
	std::vector<int32_t> blockquote_depths;
	int32_t blockquote_line = 0;
	std::vector<String> task_items;
	std::vector<bool> task_checked_vec;
	int32_t task_line = 0;
	std::vector<String> list_items;
	int32_t list_base_indent = 0;
	int32_t list_line = 0;
	bool list_ordered = false;
	int32_t list_start = 1;
	std::vector<MarkdownFootnoteDefinition> footnote_definitions;

	int32_t max_index = std::min<int32_t>(static_cast<int32_t>(lines.size()), p_max_lines);
	int32_t index = 0;
	int32_t latex_scan_boundary = 0;
	int32_t html_list_scan_end = 0;

	auto flush_paragraph = [&]() {
		if (!paragraph_accumulator.is_empty()) {
			MarkdownBlock block;
			block.type = MARKDOWN_BLOCK_PARAGRAPH;
			block.text = paragraph_accumulator;
			block.line = paragraph_line;
			blocks.push_back(block);
			paragraph_accumulator = String();
			paragraph_line = 0;
		}
		};

	auto flush_blockquote = [&]() {
		if (!blockquote_lines.empty()) {
			MarkdownBlock block;
			block.type = MARKDOWN_BLOCK_BLOCKQUOTE;
			String text;
			for (int32_t i = 0; i < static_cast<int32_t>(blockquote_lines.size()); i++) {
				if (i > 0 && (p_options.parse_html || !text.is_empty())) {
					text += "\n";
				}
				text += blockquote_lines[i];
			}
			block.text = text;
			block.line = blockquote_line;
			if (p_options.parse_html) {
				block.end_line = blockquote_line + static_cast<int32_t>(blockquote_lines.size()) - 1;
			}
			block.level = 1;
			for (int32_t di = 0; di < static_cast<int32_t>(blockquote_depths.size()); di++) {
				block.item_levels.push_back(blockquote_depths[di]);
			}
			block.children = parse_markdown_blocks(text, nullptr, INT_MAX, p_options);
			blocks.push_back(block);
			blockquote_lines.clear();
			blockquote_depths.clear();
			blockquote_line = 0;
		}
		};

	auto flush_task_list = [&]() {
		if (!task_items.empty()) {
			MarkdownBlock block;
			block.type = MARKDOWN_BLOCK_TASK_LIST;
			block.line = task_line;
			for (const String& item : task_items) {
				block.items.push_back(item);
			}
			block.task_checked = task_checked_vec;
			blocks.push_back(block);
			task_items.clear();
			task_checked_vec.clear();
			task_line = 0;
		}
		};

	auto flush_list = [&]() {
		if (!list_items.empty()) {
			MarkdownBlock block;
			block.type = state.list_ordered ? MARKDOWN_BLOCK_ORDERED_LIST : MARKDOWN_BLOCK_UNORDERED_LIST;
			block.line = list_line;
			block.indent = std::max<int32_t>(0, list_base_indent / 2);
			block.list_start = state.list_ordered ? list_start : 1;
			for (const String& item : list_items) {
				block.items.push_back(item);
				block.item_levels.push_back(block.indent);
			}
			blocks.push_back(block);
			list_items.clear();
			list_line = 0;
		}
		};

	auto flush_code = [&](int32_t p_end_line = 0) {
		if (!code_accumulator.is_empty() || state.in_fenced_code) {
			MarkdownBlock block;
			block.type = MARKDOWN_BLOCK_CODE;
			block.line = code_line;
			block.end_line = p_end_line;
			block.text = strip_code_fence_indent(code_accumulator, state.fence_indent);
			block.argument = state.fence_language;
			block.indent = state.fence_indent;
			blocks.push_back(block);
			code_accumulator = String();
			code_line = 0;
		}
		};

	auto flush_all = [&]() {
		flush_paragraph();
		flush_blockquote();
		flush_task_list();
		flush_list();
		flush_code();
		};

	if (state.in_fenced_code) {
		code_line = 1;
	}
	if (state.in_blockquote) {
		blockquote_line = 1;
	}
	if (state.in_list) {
		list_line = 1;
		list_ordered = state.list_ordered;
	}

	while (index < max_index) {
		const String line = lines[index];

		// Blank line
		if (is_blank_line(line)) {
			if (state.in_fenced_code) {
				code_accumulator += "\n";
			}
			else {
				flush_paragraph();
				flush_blockquote();
				if (state.in_list && !list_items.empty()) {
					flush_list();
					state.in_list = false;
				}
			}
			index++;
			continue;
		}

		// Fenced code - close
		if (state.in_fenced_code && is_fenced_code_close(line, state.fence_marker, state.fence_count)) {
			flush_code(index + 1);
			state.in_fenced_code = false;
			state.fence_marker = String();
			state.fence_count = 0;
			state.fence_indent = 0;
			state.fence_language = String();
			index++;
			continue;
		}

		// Fenced code - body
		if (state.in_fenced_code) {
			if (!code_accumulator.is_empty()) {
				code_accumulator += "\n";
			}
			code_accumulator += line;
			index++;
			continue;
		}

		// Fenced code - open
		String fence_char;
		int32_t fence_count = 0;
		String fence_language;
		int32_t fence_indent = 0;
		if (is_fenced_code_open(line, fence_char, fence_count, fence_language, fence_indent)) {
			flush_all();
			state.in_fenced_code = true;
			state.fence_marker = fence_char;
			state.fence_count = fence_count;
			state.fence_indent = fence_indent;
			state.fence_language = fence_language;
			code_accumulator = String();
			code_line = index + 1;
			index++;
			continue;
		}

		if (p_options.parse_html) {
			if (const auto range = docrender::scan_markdown_html_block(lines, index, max_index, !paragraph_accumulator.is_empty())) {
				flush_all();
				state.in_list = false;
				state.in_blockquote = false;
				MarkdownBlock block;
				block.type = MARKDOWN_BLOCK_HTML;
				block.line = index + 1;
				block.end_line = range->end;
				if (range->open) {
					state.unclosed_html_line = block.line;
				}
				for (; index < range->end; ++index) {
					if (!block.text.is_empty()) {
						block.text += "\n";
					}
					block.text += html_source_lines[index];
				}
				blocks.push_back(std::move(block));
				continue;
			}
			if (index >= html_list_scan_end) {
				if (auto list = parse_html_list(html_source_lines, index, max_index, p_options, html_list_scan_end)) {
					flush_all();
					state.in_list = false;
					state.in_blockquote = false;
					if (html_list_scan_end == max_index) {
						state.unclosed_html_line = list->line;
					}
					index = list->end_line;
					blocks.push_back(std::move(*list));
					continue;
				}
			}
		}

		if (p_options.parse_latex && get_indent(line) <= 3) {
			const String trimmed = trim_left_indent(line);
			if (trimmed.begins_with("$$") && !trimmed.begins_with("$$$")) {
				int32_t closing_line = index;
				int64_t close = find_display_latex_end(trimmed, 2);
				bool closed = close >= 0 && close == trimmed.length() - 2;
				if (close < 0 && index >= latex_scan_boundary) {
					int32_t candidate = index + 1;
					for (; candidate < max_index; candidate++) {
						const String& candidate_line = lines[candidate];
						const String candidate_text = trim_left_indent(candidate_line);
						if (get_indent(candidate_line) <= 3 &&
							(candidate_text.begins_with("```") || candidate_text.begins_with("~~~"))) {
							break;
						}
						close = find_display_latex_end(candidate_line, 0);
						if (close >= 0) {
							closing_line = candidate;
							closed = close == candidate_line.length() - 2;
							break;
						}
					}
					latex_scan_boundary = candidate;
					if (candidate == max_index && state.unclosed_latex_line == 0) {
						state.unclosed_latex_line = paragraph_line > 0 ? paragraph_line : index + 1;
					}
				}

				if (closed) {
					PackedStringArray source_lines;
					source_lines.push_back(trimmed);
					for (int32_t i = index + 1; i <= closing_line; i++) {
						source_lines.push_back(lines[i]);
					}
					flush_all();
					state.in_list = false;
					state.in_blockquote = false;
					MarkdownBlock block;
					block.type = MARKDOWN_BLOCK_LATEX;
					block.line = index + 1;
					block.end_line = closing_line + 1;
					block.text = String("\n").join(source_lines);
					block.argument = block.text.substr(2, block.text.length() - 4).strip_edges();
					blocks.push_back(block);
					index = closing_line + 1;
					continue;
				}
			}
		}

		// Footnote definition
		String footnote_id;
		String footnote_text;
		if (is_footnote_definition_start(line, footnote_id, footnote_text)) {
			flush_all();
			MarkdownFootnoteDefinition definition;
			definition.id = footnote_id;
			definition.text = footnote_text;
			index++;

			while (index < max_index) {
				String continuation_text;
				if (!is_footnote_continuation_line(lines[index], continuation_text)) {
					break;
				}

				if (continuation_text.is_empty()) {
					if (!definition.text.is_empty() && !definition.text.ends_with("\n\n")) {
						definition.text += "\n\n";
					}
				}
				else {
					if (!definition.text.is_empty() && !definition.text.ends_with("\n\n")) {
						definition.text += "\n";
					}
					definition.text += continuation_text.strip_edges();
				}
				index++;
			}

			definition.text = definition.text.rstrip("\n \t");
			footnote_definitions.push_back(definition);
			continue;
		}

		// Helper: table separator line check
		auto is_table_separator = [](const String& p_line) -> bool {
			int32_t pipe_count = 0;
			int32_t sep_count = 0;
			for (int32_t j = 0; j < p_line.length(); j++) {
				const char32_t ch = p_line[j];
				if (ch == '|') pipe_count++;
				else if (ch == '-' || ch == ':' || ch == ' ') sep_count++;
				else return false;
			}
			return pipe_count >= 2 && sep_count >= 2;
			};

		// Helper: parse table cell row
		auto parse_table_cells = [&p_options](const String& p_line) -> PackedStringArray {
			PackedStringArray cells;
			int32_t start = 0;
			if (p_line.length() > 0 && p_line[0] == '|') start = 1;
			int32_t pos = start;
			for (int32_t j = start; j <= p_line.length(); j++) {
				if (p_options.parse_latex && j < p_line.length() && p_line[j] == '$') {
					const int64_t end = find_inline_latex_end(p_line, j);
					if (end >= 0) {
						j = static_cast<int32_t>(end);
						continue;
					}
				}
				if (j == p_line.length() || p_line[j] == '|') {
					String cell = p_line.substr(pos, j - pos).strip_edges();
					cells.push_back(cell);
					pos = j + 1;
				}
			}
			if (cells.size() > 0 && cells[cells.size() - 1].is_empty()) {
				cells.resize(cells.size() - 1);
			}
			return cells;
			};

		// Helper: parse column alignment from a separator cell
		auto parse_alignment = [](const String& p_cell) -> int32_t {
			String trimmed = p_cell.strip_edges();
			const bool col_start = trimmed.length() > 0 && trimmed[0] == ':';
			const bool col_end = trimmed.length() > 1 && trimmed[trimmed.length() - 1] == ':';
			if (col_start && col_end) return 1; // HORIZONTAL_ALIGNMENT_CENTER
			if (col_end) return 2;              // HORIZONTAL_ALIGNMENT_RIGHT
			return 0;                           // HORIZONTAL_ALIGNMENT_LEFT (default)
			};

		// Table detection
		if (p_options.parse_gfm && line.strip_edges().begins_with("|")) {
			PackedStringArray first_cells = parse_table_cells(line);
			if (first_cells.size() >= 2 && index + 1 < lines.size() && is_table_separator(lines[index + 1])) {
				flush_all();
				MarkdownBlock block;
				block.type = MARKDOWN_BLOCK_TABLE;
				block.line = index + 1;
				block.header_rows = 1;
				block.columns = first_cells.size();

				PackedStringArray sep_cells = parse_table_cells(lines[index + 1]);
				for (int32_t c = 0; c < block.columns; c++) {
					block.column_alignments.push_back(c < sep_cells.size() ? parse_alignment(sep_cells[c]) : 0);
				}

				block.rows.push_back(first_cells);
				index += 2;
				while (index < lines.size()) {
					const String& row_line = lines[index];
					if (!row_line.strip_edges().begins_with("|")) break;
					PackedStringArray cells = parse_table_cells(row_line);
					if (cells.is_empty()) break;
					block.rows.push_back(cells);
					index++;
				}
				blocks.push_back(block);
				continue;
			}
		}

		// Thematic break
		if (is_thematic_break(line)) {
			flush_all();
			MarkdownBlock block;
			block.type = MARKDOWN_BLOCK_THEMATIC_BREAK;
			block.line = index + 1;
			blocks.push_back(block);
			index++;
			continue;
		}

		// ATX heading
		String heading_text;
		int32_t heading_level = 0;
		if (is_atx_heading(line, heading_text, heading_level)) {
			flush_all();
			MarkdownBlock block;
			block.type = MARKDOWN_BLOCK_HEADING;
			block.text = heading_text;
			block.level = heading_level;
			block.line = index + 1;
			block.anchor = slugify_heading(heading_text);
			blocks.push_back(block);
			index++;
			continue;
		}

		// Setext heading (check next line for underline)
		char32_t setext_marker;
		if (index + 1 < max_index && !paragraph_accumulator.is_empty() && is_setext_heading_underline(lines[index + 1], setext_marker) && !is_blank_line(lines[index + 1])) {
			flush_blockquote();
			flush_list();
			MarkdownBlock block;
			block.type = MARKDOWN_BLOCK_HEADING;
			block.text = paragraph_accumulator;
			block.level = (setext_marker == '=') ? 1 : 2;
			block.line = paragraph_line;
			block.anchor = slugify_heading(paragraph_accumulator);
			blocks.push_back(block);
			paragraph_accumulator = String();
			paragraph_line = 0;
			index += 2;
			continue;
		}

		// Blockquote
		String blockquote_content;
		if (is_blockquote_line(line, blockquote_content)) {
			if (p_options.parse_html) {
				is_blockquote_line(html_source_lines[index], blockquote_content);
			}
			flush_paragraph();
			flush_list();
			if (!state.in_blockquote) {
				state.in_blockquote = true;
				blockquote_line = index + 1;
			}
			int32_t bq_depth = get_blockquote_depth(line);
			blockquote_depths.push_back(bq_depth);
			blockquote_lines.push_back(blockquote_content);
			index++;
			continue;
		}

		if (state.in_blockquote) {
			flush_blockquote();
			state.in_blockquote = false;
		}

		// Task list
		String task_item_text;
		bool task_checked = false;
		if (p_options.parse_gfm && is_task_list_item(line, task_item_text, task_checked)) {
			flush_paragraph();
			flush_blockquote();
			flush_list();
			if (task_items.empty()) {
				task_line = index + 1;
			}
			task_items.push_back(task_item_text);
			task_checked_vec.push_back(task_checked);
			index++;
			continue;
		}

		// Unordered list
		String list_item_text;
		if (is_unordered_list_item(line, list_item_text)) {
			flush_task_list();
			flush_paragraph();
			flush_blockquote();
			int32_t item_indent = 0;
			while (item_indent < line.length() && line[item_indent] == ' ') item_indent++;
			if (!state.in_list) {
				state.in_list = true;
				state.list_ordered = false;
				list_base_indent = item_indent;
				list_line = index + 1;
				list_start = 1;
			}
			else if (state.list_ordered || item_indent != list_base_indent) {
				flush_list();
				state.list_ordered = false;
				list_base_indent = item_indent;
				list_line = index + 1;
				list_start = 1;
			}
			list_items.push_back(list_item_text);
			index++;
			continue;
		}

		// Ordered list
		String ordered_item_text;
		int32_t ordered_item_number = 1;
		if (is_ordered_list_item(line, ordered_item_text, ordered_item_number)) {
			flush_paragraph();
			flush_blockquote();
			int32_t item_indent = 0;
			while (item_indent < line.length() && line[item_indent] == ' ') item_indent++;
			if (!state.in_list) {
				state.in_list = true;
				state.list_ordered = true;
				list_base_indent = item_indent;
				list_line = index + 1;
				list_start = ordered_item_number;
			}
			else if (!state.list_ordered || item_indent != list_base_indent) {
				flush_list();
				state.list_ordered = true;
				list_base_indent = item_indent;
				list_line = index + 1;
				list_start = ordered_item_number;
			}
			list_items.push_back(ordered_item_text);
			index++;
			continue;
		}

		if (state.in_list) {
			flush_list();
			state.in_list = false;
		}

		// Paragraph continuation or start
		if (!paragraph_accumulator.is_empty()) {
			paragraph_accumulator = join_wrapped_line(paragraph_accumulator, line);
		}
		else {
			paragraph_accumulator = line.strip_edges();
			paragraph_line = index + 1;
		}
		index++;
	}

	flush_all();
	if (!footnote_definitions.empty()) {
		MarkdownBlock footnote_block;
		footnote_block.type = MARKDOWN_BLOCK_FOOTNOTES;
		footnote_block.line = max_index;
		footnote_block.footnotes = footnote_definitions;
		blocks.push_back(footnote_block);
	}

	state.stable_block_count = static_cast<int32_t>(blocks.size());
	state.stable_source_offset = p_text.length();
	result.state = state;
	return result;
}

std::vector<MarkdownBlock> parse_markdown_blocks(const String& p_text, const MarkdownParserState* p_initial_state, int32_t p_max_lines, const MarkdownParseOptions& p_options) {
	return parse_markdown_document(p_text, p_initial_state, p_max_lines, p_options).blocks;
}

int32_t find_reparse_line(const std::vector<MarkdownBlock>& p_blocks, const std::vector<String>& p_lines, const MarkdownParserState& p_state, int32_t p_max_unstable_lines) {
	auto include_open_block_start = [&](int32_t p_start) {
		if (p_state.unclosed_latex_line > 0) {
			p_start = std::min(p_start, p_state.unclosed_latex_line - 1);
		}
		if (p_state.unclosed_html_line > 0) {
			p_start = std::min(p_start, p_state.unclosed_html_line - 1);
		}
		const auto next = std::upper_bound(p_blocks.begin(), p_blocks.end(), p_start + 1,
			[](int32_t p_line, const MarkdownBlock& p_block) { return p_line < p_block.line; });
		if (next != p_blocks.begin()) {
			const MarkdownBlock& block = *(next - 1);
			if ((block.type == MARKDOWN_BLOCK_LATEX || block.type == MARKDOWN_BLOCK_HTML || !block.list_items.empty() ||
				block.type == MARKDOWN_BLOCK_BLOCKQUOTE) && p_start + 1 <= block.end_line) {
				return block.line - 1;
			}
		}
		return p_start;
	};

	if (p_state.in_fenced_code || p_state.in_list || p_state.in_blockquote) {
		for (int32_t i = static_cast<int32_t>(p_blocks.size()) - 1; i >= 0; i--) {
			if ((p_state.in_fenced_code && p_blocks[i].type == MARKDOWN_BLOCK_CODE) || (p_state.in_list && (p_blocks[i].type == MARKDOWN_BLOCK_UNORDERED_LIST || p_blocks[i].type == MARKDOWN_BLOCK_ORDERED_LIST)) || (p_state.in_blockquote && p_blocks[i].type == MARKDOWN_BLOCK_BLOCKQUOTE)) {
				return include_open_block_start(std::max<int32_t>(0, p_blocks[i].line - 1));
			}
		}
	}

	if (p_blocks.empty()) {
		return 0;
	}

	if (p_blocks.back().type == MARKDOWN_BLOCK_TABLE) {
		return include_open_block_start(std::max<int32_t>(0, p_blocks.back().line - 1));
	}

	int32_t last_block_line = p_blocks.back().line - 1;
	int32_t tail_lines = std::min<int32_t>(p_max_unstable_lines, static_cast<int32_t>(p_lines.size()) - last_block_line);
	int32_t reparse_start = std::max<int32_t>(0, static_cast<int32_t>(p_lines.size()) - tail_lines);

	return include_open_block_start(reparse_start);
}

void rebuild_tail(const String& p_raw_text, int32_t p_reparse_line, std::vector<MarkdownBlock>& r_blocks, MarkdownParserState& r_state, const MarkdownParseOptions& p_options) {
	r_blocks.erase(std::remove_if(r_blocks.begin(), r_blocks.end(), [p_reparse_line](const MarkdownBlock& b) { return b.line >= p_reparse_line + 1; }), r_blocks.end());

	r_state.reset();

	std::vector<String> lines = split_lines(p_raw_text, p_options.parse_html);
	String tail_text;
	for (int32_t i = p_reparse_line; i < static_cast<int32_t>(lines.size()); i++) {
		if (i > p_reparse_line) {
			tail_text += "\n";
		}
		tail_text += lines[i];
	}

	MarkdownParseResult tail_result = parse_markdown_document(tail_text, nullptr, INT_MAX, p_options);
	std::vector<MarkdownBlock> tail_blocks = tail_result.blocks;

	for (MarkdownBlock& block : tail_blocks) {
		block.line += p_reparse_line;
		if (block.end_line > 0) {
			block.end_line += p_reparse_line;
		}
	}

	r_state = tail_result.state;
	if (r_state.unclosed_latex_line > 0) {
		r_state.unclosed_latex_line += p_reparse_line;
	}
	if (r_state.unclosed_html_line > 0) {
		r_state.unclosed_html_line += p_reparse_line;
	}
	r_state.stable_block_count = static_cast<int32_t>(r_blocks.size() + tail_blocks.size());
	r_state.stable_source_offset = p_raw_text.length();
	r_blocks.insert(r_blocks.end(), tail_blocks.begin(), tail_blocks.end());
}
