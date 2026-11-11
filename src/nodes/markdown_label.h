#pragma once

#include "core/markdown_parser.h"
#include "core/mermaid_render.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/h_scroll_bar.hpp>
#include <godot_cpp/classes/http_request.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/text_paragraph.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <vector>

namespace godot {
	class PopupMenu;
}

using namespace godot;

class MarkdownLabel;

struct MarkdownThemeCache {
	float base_scale = 1.0f;

	struct Constants {
		int32_t line_separation = 2;
		int32_t paragraph_separation = 10;
		int32_t list_indent = 26;
		int32_t list_marker_gap = 6;
		int32_t table_striped = 1;
		int32_t footnote_space_before = 16;
		int32_t footnote_line_separation = 2;
	} constant;

	struct FontSizes {
		int32_t text = 16;
		int32_t code = 16;
		int32_t code_block = 16;
		int32_t list_marker = 16;
		int32_t heading[6] = { 36, 26, 20, 18, 12, 10 };
		int32_t table_header = 16;
		int32_t table_cell = 16;
		int32_t footnote_ref = 12;
		int32_t footnote_text = 16;
	} font_size;

	struct Colors {
		Color text = Color(1.0f, 1.0f, 1.0f, 1.0f);
		Color link = Color(0.36f, 0.62f, 1.0f, 1.0f);
		Color code_text = Color(1.0f, 1.0f, 1.0f, 1.0f);
		Color code_block = Color(1.0f, 1.0f, 1.0f, 1.0f);
		Color code_block_background = Color(0.08f, 0.09f, 0.11f, 1.0f);
		Color highlight = Color(1.0f, 0.92f, 0.35f, 0.5f);
		Color highlight_font = Color(1.0f, 0.700f, 0.211f, 1.0f);
		Color strikethrough = Color(1.0f, 1.0f, 1.0f, 1.0f);
		Color heading[6];
		Color selection_color = Color(0.102f, 0.102f, 1.0f, 0.8f);
		Color list_marker = Color(1.0f, 1.0f, 1.0f, 1.0f);
		Color table_header_font = Color(1.0f, 1.0f, 1.0f, 1.0f);
		Color table_cell_font = Color(1.0f, 1.0f, 1.0f, 1.0f);
		Color footnote_ref = Color(0.809f, 0.809f, 0.809f, 1.0f);
		Color footnote_text = Color(0.875f, 0.875f, 0.875f, 0.502f);
	} color;

	struct Fonts {
		Ref<Font> text;
		Ref<Font> bold;
		Ref<Font> bold_italic;
		Ref<Font> italic;
		Ref<Font> code;
		Ref<Font> code_block;
		Ref<Font> heading[6];
		Ref<Font> list_marker;
		Ref<Font> table_header;
		Ref<Font> table_cell;
		Ref<Font> footnote_ref;
		Ref<Font> footnote_text;
	} font;

	struct StyleBoxes {
		Ref<StyleBox> inline_code;
		Ref<StyleBox> code_block_panel;
		Ref<StyleBox> blockquote;
		Ref<StyleBox> blockquote_nested;
		Ref<StyleBox> selection;
		Ref<StyleBox> search_result;
		Ref<StyleBox> separator;
		Ref<StyleBox> highlight;
		Ref<StyleBox> table_panel;
		Ref<StyleBox> table_header_panel;
		Ref<StyleBox> table_cell_panel;
		Ref<StyleBox> table_striped_panel;
		Ref<StyleBox> footnote_ref;
	} stylebox;

	struct Icons {
		Ref<Texture2D> task_checked;
		Ref<Texture2D> task_unchecked;
		Ref<Texture2D> file_broken;
	} icon;

	void build(Control* p_owner, int32_t p_extra_font_size);
};


class MarkdownLabelCanvas : public Control {
	GDCLASS(MarkdownLabelCanvas, Control);

	friend class MarkdownLabel;

private:
	MarkdownLabel* label = nullptr;
	String raw_text;
	String plain_text;
	std::vector<MarkdownCanvasItem> items;
	std::vector<MarkdownCanvasLine> lines;
	bool layout_dirty = true;
	bool dragging_selection = false;
	bool selection_dragged = false;
	bool selection_drag_attempt = false;
	bool internal_processing_enabled = false;
	PopupMenu* menu = nullptr;
	int64_t selection_anchor = 0;
	int64_t selection_caret = 0;
	int64_t search_match_start = -1;
	int64_t search_match_end = -1;
	Vector2 press_position;
	int32_t rendered_block_count = 0;
	float content_height = 1.0f;
	float layout_width = 0.0f;
	Dictionary anchor_offsets;
	HashMap<String, float> footnote_reference_offsets;
	HashMap<String, float> footnote_definition_offsets;

	MarkdownThemeCache theme_cache;
	MermaidRenderCache mermaid_cache;

	MarkdownParserState parser_state;
	std::vector<MarkdownBlock> cached_blocks;
	bool streaming_enabled = false;
	bool deferred_layout_enabled = false;
	bool pending_stream_parse = false;
	bool layout_prefix_reuse_requested = false;
	int32_t max_unstable_lines = 32;
	int32_t layout_flush_interval_msec = 33;
	double pending_layout_elapsed_msec = 0.0;

	void mark_layout_dirty();
	MarkdownParseOptions get_parse_options() const;
	void parse_current_text_incremental();
	void build_theme_cache();
	void _generate_context_menu();
	void _update_context_menu();
	Key _get_menu_action_accelerator(const String& p_action);
	void rebuild_layout();
	double get_offset_for_source_line(double p_line, int32_t p_line_count);
	double get_source_line_at_offset(double p_offset, int32_t p_line_count);
	void draw_selection_for_item(const MarkdownCanvasItem& p_item);
	void draw_selection_for_paragraph(const Ref<TextParagraph>& p_paragraph, const Rect2& p_text_rect, int64_t p_global_start, int64_t p_global_end);
	void draw_search_highlight_for_item(const MarkdownCanvasItem& p_item);
	void draw_range_background_for_paragraph(const Ref<TextParagraph>& p_paragraph, const Rect2& p_text_rect, int64_t p_global_start, int64_t p_global_end, int64_t p_range_start, int64_t p_range_end, const Ref<StyleBox>& p_stylebox);
	void draw_stylebox_or_rect(const Ref<StyleBox>& p_stylebox, const Rect2& p_rect, const Color& p_fallback_color);
	void draw_paragraph_objects(const Ref<TextParagraph>& p_paragraph, const Rect2& p_text_rect, const MarkdownParagraphObjects& p_objects);
	void draw_span_decorations(const Ref<TextParagraph>& p_paragraph, const Rect2& p_text_rect, const std::vector<MarkdownInlineSpan>& p_spans, int64_t p_global_start, bool p_draw_code_backgrounds, bool p_draw_link_underlines);
	void draw_paragraph_text(const Ref<TextParagraph>& p_paragraph, const Rect2& p_text_rect, const std::vector<MarkdownInlineSpan>& p_spans, int64_t p_global_start, const Color& p_default_color);
	Rect2 get_visible_content_rect() const;
	bool is_rect_visible(const Rect2& p_rect, const Rect2& p_visible_rect) const;
	int64_t hit_test_document(const Vector2& p_position) const;
	String link_uri_at_position(const Vector2& p_position) const;
	String link_uri_at_character(int64_t p_character) const;
	String image_tooltip_at_position(const Vector2& p_position) const;
	float footnote_target_at_position(const Vector2& p_position) const;
	String footnote_id_at_character(int64_t p_character, bool p_reference) const;
	int64_t line_column_to_offset(int32_t p_line, int32_t p_column) const;
	Vector2i offset_to_line_column(int64_t p_offset) const;
	bool has_selection() const;
	bool is_character_selected(int64_t p_character) const;
	bool is_position_inside_selection(const Vector2& p_position);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	MarkdownLabelCanvas();
	~MarkdownLabelCanvas();

	void _gui_input(const Ref<InputEvent>& p_event) override;
	Variant _get_drag_data(const Vector2& p_at_position) override;
	Vector2 _get_minimum_size() const override;

	void set_label(MarkdownLabel* p_label);
	void set_markdown_text(const String& p_text);
	String resolve_image_uri(const String& p_uri) const;
	void append_text_incremental(const String& p_delta);
	void finish_stream();
	void clear_document();
	void set_streaming_enabled(bool p_enabled);
	void set_max_unstable_lines(int32_t p_lines);
	void set_deferred_layout_enabled(bool p_enabled);
	bool is_deferred_layout_enabled() const;
	void set_layout_flush_interval_msec(int32_t p_msec);
	int32_t get_layout_flush_interval_msec() const;
	bool flush_pending_layout();
	void request_local_image(const String& p_uri);
	void request_remote_image(const String& p_uri);
	void on_image_request_completed(int32_t p_result, int32_t p_response_code, const PackedStringArray& p_headers, const PackedByteArray& p_body, const String& p_uri, uint64_t p_request_id);
	void refresh_image_content();
	void update_internal_processing();
	bool process_pending_local_images();
	bool update_gif_animations(double p_delta);
	void select_all();
	void select_between_points(const Vector2& p_from, const Vector2& p_to);
	String get_link_uri_at_point(const Vector2& p_position);
	void clear_selection();
	Vector2i search(const String& p_text, int32_t p_flags, int32_t p_from_line, int32_t p_from_column);
	String get_selected_text() const;
	int32_t get_rendered_block_count();
	float get_content_height();
	bool has_anchor(const String& p_anchor) const;
	PackedStringArray get_anchors() const;
	float get_anchor_offset(const String& p_anchor) const;
	float get_search_result_y() const;
	float get_bottom_position() const;
	PopupMenu* get_menu();
	bool is_menu_visible() const;
	void menu_option(int p_option);
};

class MarkdownLabel : public Control {
	GDCLASS(MarkdownLabel, Control);

public:
	enum MenuItems {
		MENU_COPY,
		MENU_SELECT_ALL,
		MENU_MAX
	};

private:
	friend class MarkdownLabelCanvas;

	String raw_text;
	String image_base_dir;
	ScrollContainer* scroll_container = nullptr;
	MarginContainer* margin_container = nullptr;
	MarkdownLabelCanvas* canvas = nullptr;
	int32_t content_margin = 16;
	int32_t extra_font_size = 0;
	bool open_external_links = true;
	bool parse_latex = false;
	bool parse_gfm = true;
	bool parse_mermaid = false;
	bool selection_enabled = true;
	bool deselect_on_focus_loss_enabled = true;
	bool drag_and_drop_selection_enabled = true;
	bool context_menu_enabled = false;
	bool shortcut_keys_enabled = true;
	bool streaming_enabled = false;
	bool deferred_layout_enabled = false;
	int32_t max_unstable_lines = 32;
	int32_t layout_flush_interval_msec = 33;
	bool fit_content = false;
	bool scroll_active = true;
	bool scroll_follow = false;
	bool scroll_following = false;
	bool scroll_follow_visible_characters = false;
	TextServer::AutowrapMode autowrap_mode = TextServer::AUTOWRAP_WORD_SMART;
	BitField<TextServer::LineBreakFlag> autowrap_flags_trim = TextServer::BREAK_TRIM_START_EDGE_SPACES | TextServer::BREAK_TRIM_END_EDGE_SPACES;

	void ensure_controls();
	void apply_theme_settings();
	void apply_scroll_settings();
	void update_scroll_following();
	void scroll_to_following();
	void on_scroll_value_changed(double p_value);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	Vector2 _get_minimum_size() const override;

	void set_markdown_text(const String& p_text);
	String get_markdown_text() const;
	void set_image_base_dir(const String& p_dir);
	void set_parse_latex(bool p_enabled);
	bool is_parsing_latex() const;
	void set_parse_gfm(bool p_enabled);
	bool is_parsing_gfm() const;
	void set_parse_mermaid(bool p_enabled);
	bool is_parsing_mermaid() const;

	void append_text(const String& p_delta);
	void finish_stream();
	void clear();

	void set_streaming_enabled(bool p_enabled);
	bool is_streaming_enabled() const;

	void set_max_unstable_lines(int32_t p_lines);
	int32_t get_max_unstable_lines() const;

	void set_deferred_layout_enabled(bool p_enabled);
	bool is_deferred_layout_enabled() const;

	void set_layout_flush_interval_msec(int32_t p_msec);
	int32_t get_layout_flush_interval_msec() const;

	bool flush_pending_layout();

	void set_fit_content(bool p_enabled);
	bool is_fit_content_enabled() const;

	void set_scroll_active(bool p_active);
	bool is_scroll_active() const;

	void set_scroll_follow(bool p_follow);
	bool is_scroll_following() const;

	void set_scroll_follow_visible_characters(bool p_follow);
	bool is_scroll_following_visible_characters() const;

	void set_autowrap_mode(TextServer::AutowrapMode p_mode);
	TextServer::AutowrapMode get_autowrap_mode() const;

	void set_autowrap_trim_flags(BitField<TextServer::LineBreakFlag> p_flags);
	BitField<TextServer::LineBreakFlag> get_autowrap_trim_flags() const;

	void set_content_margin(int32_t p_margin);
	int32_t get_content_margin() const;

	void set_extra_font_size(int32_t p_extra);
	int32_t get_extra_font_size() const;

	void set_selection_enabled(bool p_enabled);
	bool is_selection_enabled() const;

	void set_deselect_on_focus_loss_enabled(bool p_enabled);
	bool is_deselect_on_focus_loss_enabled() const;

	void set_drag_and_drop_selection_enabled(bool p_enabled);
	bool is_drag_and_drop_selection_enabled() const;

	void set_context_menu_enabled(bool p_enabled);
	bool is_context_menu_enabled() const;

	void set_shortcut_keys_enabled(bool p_enabled);
	bool is_shortcut_keys_enabled() const;

	void set_open_external_links(bool p_enabled);
	bool get_open_external_links() const;

	void select_all();
	void clear_selection();
	String get_selected_text() const;

	Vector2i search(const String& p_text, int32_t p_flags, int32_t p_from_line, int32_t p_from_column);

	String get_link_uri_at_point(const Vector2& p_position);
	void activate_link_uri(const String& p_uri);

	bool has_anchor(const String& p_anchor) const;
	PackedStringArray get_anchors() const;
	bool scroll_to_anchor(const String& p_anchor, float p_align = 0.0f);
	bool scroll_to_search_result(float p_align = 0.0f);
	double get_scroll_offset_for_source_line(double p_line, int32_t p_line_count);
	double get_source_line_at_scroll_offset(double p_offset, int32_t p_line_count);

	Error load_markdown_file(const String& p_path);
	void refresh();
	void notify_canvas_layout_changed();

	int32_t get_rendered_block_count() const;
	float get_content_height() const;

	HScrollBar* get_h_scroll_bar() const;
	VScrollBar* get_v_scroll_bar() const;

	PopupMenu* get_menu();
	bool is_menu_visible() const;
	void menu_option(int p_option);
};

VARIANT_ENUM_CAST(MarkdownLabel::MenuItems);
