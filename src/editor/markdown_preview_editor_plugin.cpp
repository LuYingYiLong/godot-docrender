#include "markdown_preview_editor_plugin.h"

#include "nodes/markdown_label.h"

#include <godot_cpp/classes/code_edit.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>

#include <algorithm>

namespace godot {
	Control* MarkdownPreviewEditorPlugin::create_preview_panel() {
		MarkdownLabel* label = memnew(MarkdownLabel);
		label->set_name("MarkdownPreview");
		label->set_parse_html(true);
		label->set_parse_latex(true);
		label->set_parse_mermaid(true);
		return label;
	}

	void MarkdownPreviewEditorPlugin::update_preview(CodeEdit* p_source, Control* p_preview, const String& p_path) {
		MarkdownLabel* label = Object::cast_to<MarkdownLabel>(p_preview);
		label->set_image_base_dir(p_path.get_base_dir());
		const String text = p_source->get_text();
		if (text != label->get_markdown_text()) {
			label->set_markdown_text(text);
		}
		preview_source_version = p_source->get_version();
		align_preview_to_source();
	}

	void MarkdownPreviewEditorPlugin::set_scroll_connections(CodeEdit* p_source, Control* p_preview, bool p_connect) {
		const Callable align_from_source = callable_mp(this, &MarkdownPreviewEditorPlugin::queue_scroll_sync).bind(false);
		const Callable source_scrolled = align_from_source.unbind(1);
		const Callable preview_scrolled = callable_mp(this, &MarkdownPreviewEditorPlugin::queue_scroll_sync).bind(true).unbind(1);

		const auto update_signals = [&](VScrollBar* p_scrollbar, const Callable& p_scrolled) {
			if (p_connect) {
				p_scrollbar->connect("value_changed", p_scrolled);
				p_scrollbar->connect("changed", align_from_source);
			} else {
				p_scrollbar->disconnect("value_changed", p_scrolled);
				p_scrollbar->disconnect("changed", align_from_source);
			}
		};
		if (p_source != nullptr) {
			update_signals(p_source->get_v_scroll_bar(), source_scrolled);
		}
		if (MarkdownLabel* label = Object::cast_to<MarkdownLabel>(p_preview)) {
			update_signals(label->get_v_scroll_bar(), preview_scrolled);
		}
	}

	void MarkdownPreviewEditorPlugin::align_preview_to_source() {
		queue_scroll_sync(false);
	}

	void MarkdownPreviewEditorPlugin::queue_scroll_sync(bool p_from_preview) {
		if (syncing_scroll || get_active_source() == nullptr) {
			return;
		}
		scroll_from_preview = p_from_preview;
		if (!scroll_sync_queued) {
			scroll_sync_queued = true;
			callable_mp(this, &MarkdownPreviewEditorPlugin::sync_scroll).call_deferred();
		}
	}

	void MarkdownPreviewEditorPlugin::sync_scroll() {
		scroll_sync_queued = false;
		CodeEdit* source = get_active_source();
		MarkdownLabel* label = Object::cast_to<MarkdownLabel>(get_active_preview());
		if (source == nullptr || label == nullptr || !source->is_visible_in_tree() || !label->is_visible_in_tree() || is_refresh_pending() || source->get_version() != preview_source_version) {
			return;
		}
		VScrollBar* source_scroll = source->get_v_scroll_bar();
		VScrollBar* preview_scroll = label->get_v_scroll_bar();
		const double source_end = std::max(0.0, source_scroll->get_max() - source_scroll->get_page());
		const double preview_end = preview_scroll->get_max() - preview_scroll->get_page();
		if (preview_scroll->get_page() <= 0.0 || preview_end <= 0.0) {
			return;
		}
		const int32_t line_count = source->get_line_count();

		// Updating a scrollbar also emits its value_changed signal.
		syncing_scroll = true;
		if (scroll_from_preview) {
			const double offset = preview_scroll->get_value();
			if (offset >= preview_end) {
				source->set_v_scroll(source_end);
			}
			else {
				const double source_position = label->get_source_line_at_scroll_offset(offset, line_count);
				const int32_t line = CLAMP(static_cast<int32_t>(source_position), 0, line_count - 1);
				const int32_t wraps = source->get_line_wrap_count(line);
				const int32_t wrap = CLAMP(static_cast<int32_t>((source_position - line) * (wraps + 1)), 0, wraps);
				source->set_line_as_first_visible(line, wrap);
			}
		}
		else if (source_end > 0.0 && source_scroll->get_value() >= source_end) {
			preview_scroll->set_value(preview_end);
		}
		else {
			const int32_t line = source->get_first_visible_line();
			const int32_t wraps = source->get_line_wrap_count(line);
			const double wrap_offset = source_scroll->get_value() - source->get_scroll_pos_for_line(line);
			const double fraction = CLAMP(wrap_offset / (wraps + 1), 0.0, 1.0);
			preview_scroll->set_value(label->get_scroll_offset_for_source_line(line + fraction, line_count));
		}
		syncing_scroll = false;
	}
} // namespace godot
