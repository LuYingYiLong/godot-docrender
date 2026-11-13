#pragma once

#include "formats/html/html_layout.h"
#include "formats/latex/tex_render.h"

#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/text_paragraph.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/templates/hash_map.hpp>

#include <cstdint>
#include <vector>

using namespace godot;

enum MarkdownBlockType {
	MARKDOWN_BLOCK_PARAGRAPH,
	MARKDOWN_BLOCK_HEADING,
	MARKDOWN_BLOCK_UNORDERED_LIST,
	MARKDOWN_BLOCK_ORDERED_LIST,
	MARKDOWN_BLOCK_CODE,
	MARKDOWN_BLOCK_BLOCKQUOTE,
	MARKDOWN_BLOCK_THEMATIC_BREAK,
	MARKDOWN_BLOCK_IMAGE,
	MARKDOWN_BLOCK_TABLE,
	MARKDOWN_BLOCK_TASK_LIST,
	MARKDOWN_BLOCK_FOOTNOTES,
	MARKDOWN_BLOCK_LATEX,
	MARKDOWN_BLOCK_HTML,
};

struct MarkdownFootnoteDefinition {
	String id;
	String text;
};

struct MarkdownListItem;

struct MarkdownBlock {
	MarkdownBlockType type = MARKDOWN_BLOCK_PARAGRAPH;
	String text;
	String argument;
	PackedStringArray items;
	std::vector<MarkdownFootnoteDefinition> footnotes;
	std::vector<PackedStringArray> rows;
	std::vector<int32_t> column_alignments;
	int32_t level = 0;
	int32_t columns = 0;
	int32_t header_rows = 0;
	int32_t line = 0;
	int32_t end_line = 0;
	String anchor;
	int32_t indent = 0;
	int32_t list_start = 1;
	std::vector<int32_t> item_levels;
	std::vector<bool> task_checked;
	std::vector<MarkdownBlock> children;
	std::vector<MarkdownListItem> list_items;
};

struct MarkdownListItem {
	int32_t line = 0;
	int32_t end_line = 0;
	bool checked = false;
	std::vector<MarkdownBlock> children;
};

struct MarkdownInlineSpan {
	String text;
	String link_uri;
	String image_uri;
	String image_title;
	String footnote_id;
	String latex;
	int32_t footnote_number = 0;
	int64_t start = 0;
	int64_t end = 0;
	Color color;
	Color background_color;
	Vector2i image_size;
	bool bold = false;
	bool italic = false;
	bool code = false;
	bool strikethrough = false;
	bool highlight = false;
	bool custom_color = false;
	bool custom_background = false;
	bool footnote_ref = false;
	bool display_math = false;
	bool html_image = false;
};

struct MarkdownParagraphObjects {
	HashMap<String, Ref<Texture2D>> images;
	HashMap<String, Ref<TeXRender>> formulas;
};

struct MarkdownTableCell {
	Ref<TextParagraph> paragraph;
	Ref<StyleBox> stylebox;
	Rect2 rect;
	Rect2 text_rect;
	String text;
	std::vector<MarkdownInlineSpan> spans;
	MarkdownParagraphObjects objects;
	int64_t global_start = 0;
	int64_t global_end = 0;
	Color text_color;
	bool header = false;
};

struct MarkdownListMarker {
	String text;
	Vector2 position;
	Ref<Font> font;
	int32_t font_size = 16;
	Color color;
	bool task_checked = false;
	Ref<Texture2D> icon;
};

struct MarkdownContainerLine {
	MarkdownBlockType type = MARKDOWN_BLOCK_PARAGRAPH;
	Ref<TextParagraph> paragraph;
	Ref<HTMLLayout> html_layout;
	Ref<StyleBox> stylebox;
	Rect2 rect;
	Rect2 text_rect;
	String text;
	String anchor;
	std::vector<MarkdownInlineSpan> spans;
	MarkdownParagraphObjects objects;
	std::vector<MarkdownListMarker> list_markers;
	int64_t global_start = 0;
	int64_t global_end = 0;
	int32_t depth = 1;
	Color text_color;
};

struct MarkdownCanvasLine {
	int32_t item_index = -1;
	int32_t paragraph_line = -1;
	int64_t global_start = 0;
	int64_t global_end = 0;
	Vector2 position;
	Vector2 size;
};

struct MarkdownCanvasItem {
	MarkdownBlockType type = MARKDOWN_BLOCK_PARAGRAPH;
	int32_t source_line = 0;
	Ref<TextParagraph> paragraph;
	Ref<HTMLLayout> html_layout;
	Ref<TextParagraph> header_paragraph;
	Ref<StyleBox> panel_stylebox;
	Ref<StyleBox> header_stylebox;
	std::vector<MarkdownInlineSpan> spans;
	std::vector<MarkdownInlineSpan> header_spans;
	Rect2 rect;
	Rect2 text_rect;
	Rect2 header_text_rect;
	String text;
	String header_text;
	int64_t global_start = 0;
	int64_t global_end = 0;
	int64_t paragraph_global_start = 0;
	int64_t paragraph_global_end = 0;
	int64_t header_global_start = 0;
	int64_t header_global_end = 0;
	Color text_color;
	Color header_text_color;
	Color background_color;
	Color border_color;
	Color header_color;
	bool framed = false;
	bool code = false;
	bool heading = false;
	int32_t heading_level = 0;
	int32_t blockquote_depth = 0;
	int32_t border_width = 0;
	int32_t header_height = 0;
	String anchor;
	String code_language;
	uint64_t content_hash = 0;
	std::vector<MarkdownListMarker> list_markers;
	std::vector<MarkdownTableCell> cells;
	MarkdownParagraphObjects objects;
	std::vector<MarkdownContainerLine> container_lines;
	std::vector<String> footnote_ids;
	std::vector<int32_t> footnote_numbers;
	std::vector<int64_t> footnote_starts;
	std::vector<int64_t> footnote_ends;
};

struct MarkdownParserState {
	bool in_fenced_code = false;
	String fence_marker;
	int32_t fence_count = 0;
	int32_t fence_indent = 0;
	String fence_language;
	bool in_list = false;
	int32_t list_indent = 0;
	bool list_ordered = false;
	bool in_blockquote = false;
	int32_t blockquote_depth = 0;
	int32_t unclosed_latex_line = 0;
	int32_t unclosed_html_line = 0;
	int64_t stable_source_offset = 0;
	int32_t stable_block_count = 0;

	void reset() { // 将 Markdown 流式解析状态重置为初始值
		in_fenced_code = false;
		fence_marker = String();
		fence_count = 0;
		fence_indent = 0;
		fence_language = String();
		in_list = false;
		list_indent = 0;
		list_ordered = false;
		in_blockquote = false;
		blockquote_depth = 0;
		unclosed_latex_line = 0;
		unclosed_html_line = 0;
		stable_source_offset = 0;
		stable_block_count = 0;
	}
};

struct MarkdownParseResult {
	std::vector<MarkdownBlock> blocks;
	MarkdownParserState state;
};

struct MarkdownParseOptions {
	bool parse_latex = false;
	bool parse_gfm = true;
	bool parse_html = false;
};

// Parser functions
std::vector<String> split_lines(const String& p_text, bool p_keep_trailing_space = false); // 按行拆分源文本，可保留行尾空白
MarkdownParseResult parse_markdown_document(const String& p_text, const MarkdownParserState* p_initial_state = nullptr, int32_t p_max_lines = INT_MAX, const MarkdownParseOptions& p_options = {}); // 解析 Markdown 块并返回后续流式解析所需的状态
std::vector<MarkdownBlock> parse_markdown_blocks(const String& p_text, const MarkdownParserState* p_initial_state = nullptr, int32_t p_max_lines = INT_MAX, const MarkdownParseOptions& p_options = {}); // 按指定选项解析 Markdown 并返回块列表

// Inline span parsing result
struct MarkdownInlineSpanParseResult {
	String output_text;
	std::vector<MarkdownInlineSpan> spans;
};

MarkdownInlineSpanParseResult parse_inline_spans(const String& p_text, const MarkdownParseOptions& p_options = {}); // 解析行内语法并返回显示文本及各样式片段

// Streaming helpers
int32_t find_reparse_line(const std::vector<MarkdownBlock>& p_blocks, const std::vector<String>& p_lines, const MarkdownParserState& p_state, int32_t p_max_unstable_lines); // 计算流式追加时需要重新解析的起始行索引
void rebuild_tail(const String& p_raw_text, int32_t p_reparse_line, std::vector<MarkdownBlock>& r_blocks, MarkdownParserState& r_state, const MarkdownParseOptions& p_options = {}); // 从指定行重新解析尾部，替换受影响的块并更新解析状态
uint64_t compute_block_hash(const MarkdownBlock& p_block); // 计算用于识别 Markdown 块内容变化的哈希值
String slugify_heading(const String& p_text); // 将标题文本转换为可用于锚点的标识
