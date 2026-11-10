#include "markdown_preview_editor_plugin.h"

#include "markdown_label.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/code_edit.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_split_container.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/script_editor.hpp>
#include <godot_cpp/classes/script_editor_base.hpp>
#include <godot_cpp/classes/syntax_highlighter.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/classes/translation_server.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>
#include <godot_cpp/classes/v_split_container.hpp>
#include <godot_cpp/core/object.hpp>

#include <algorithm>

namespace godot {

namespace {

template <typename T>
T *find_child_by_type(Node *p_parent) {
	T *match = nullptr;
	if (p_parent == nullptr) {
		return match;
	}
	for (int32_t i = 0; i < p_parent->get_child_count(); ++i) {
		if (T *child = Object::cast_to<T>(p_parent->get_child(i))) {
			if (match != nullptr) {
				return nullptr;
			}
			match = child;
		}
	}
	return match;
}

template <typename T>
T *get_node_by_id(uint64_t p_id) {
	return Object::cast_to<T>(ObjectDB::get_instance(p_id));
}

} // namespace

void MarkdownPreviewEditorPlugin::_enter_tree() {
	const Dictionary version = Engine::get_singleton()->get_version_info();
	if (int64_t(version["major"]) == 4 && int64_t(version["minor"]) == 7 && int64_t(version["patch"]) == 2) {
		callable_mp(this, &MarkdownPreviewEditorPlugin::attach).call_deferred();
	}
}

void MarkdownPreviewEditorPlugin::_exit_tree() {
	set_preview_source(nullptr, nullptr);
	if (ScriptEditorBase *tab = get_node_by_id<ScriptEditorBase>(active_tab_id)) {
		tab->disconnect("name_changed", callable_mp(this, &MarkdownPreviewEditorPlugin::queue_tab_sync));
	}
	if (ScriptEditor *editor = get_node_by_id<ScriptEditor>(script_editor_id)) {
		editor->disconnect("visibility_changed", callable_mp(this, &MarkdownPreviewEditorPlugin::queue_tab_sync));
	}
	if (TabContainer *tabs = get_node_by_id<TabContainer>(tab_container_id)) {
		tabs->disconnect("tab_changed", callable_mp(this, &MarkdownPreviewEditorPlugin::queue_tab_sync).unbind(1));
		tabs->disconnect("child_order_changed", callable_mp(this, &MarkdownPreviewEditorPlugin::queue_tab_sync));
	}
	if (LineEdit *filter = get_node_by_id<LineEdit>(filter_id)) {
		filter->disconnect("text_changed", callable_mp(this, &MarkdownPreviewEditorPlugin::queue_tab_sync).unbind(1));
	}
	if (Button *button = get_node_by_id<Button>(button_id)) {
		memdelete(button);
	}
	for (const KeyValue<uint64_t, PreviewTab> &entry : previews) {
		ScriptEditorBase *tab = get_node_by_id<ScriptEditorBase>(entry.key);
		HSplitContainer *split = get_node_by_id<HSplitContainer>(entry.value.split_id);
		Control *editor_area = get_node_by_id<Control>(entry.value.editor_area_id);
		if (tab == nullptr || split == nullptr || editor_area == nullptr || split->get_parent() != tab || editor_area->get_parent() != split) {
			continue;
		}
		const int32_t original_index = split->get_index();
		editor_area->reparent(tab, false);
		editor_area->set_h_size_flags(entry.value.editor_area_h_flags);
		tab->move_child(editor_area, original_index);
		memdelete(split);
	}
	previews.clear();
	active_path = String();
	active_tab_id = 0;
	button_id = 0;
	filter_id = 0;
	script_list_id = 0;
	tab_container_id = 0;
	script_editor_id = 0;
	sync_queued = false;
	scroll_sync_queued = false;
}

void MarkdownPreviewEditorPlugin::attach() {
	if (!is_inside_tree() || script_editor_id != 0) {
		return;
	}
	ScriptEditor *editor = EditorInterface::get_singleton()->get_script_editor();
	// This is the private ScriptEditor layout in Godot 4.7.2.
	VBoxContainer *main = find_child_by_type<VBoxContainer>(editor);
	HBoxContainer *menu = find_child_by_type<HBoxContainer>(main);
	HSplitContainer *body = find_child_by_type<HSplitContainer>(main);
	VBoxContainer *editor_column = find_child_by_type<VBoxContainer>(body);
	TabContainer *tabs = find_child_by_type<TabContainer>(editor_column);
	VSplitContainer *sidebar = find_child_by_type<VSplitContainer>(body);
	VBoxContainer *file_column = Object::cast_to<VBoxContainer>(sidebar && sidebar->get_child_count() > 0 ? sidebar->get_child(0) : nullptr);
	ItemList *script_list = find_child_by_type<ItemList>(file_column);
	LineEdit *filter = find_child_by_type<LineEdit>(file_column);
	if (menu == nullptr || tabs == nullptr || tabs->are_tabs_visible() || script_list == nullptr || filter == nullptr) {
		return;
	}

	if (refresh_timer == nullptr) {
		refresh_timer = memnew(Timer);
		refresh_timer->set_one_shot(true);
		refresh_timer->set_wait_time(0.15);
		refresh_timer->connect("timeout", callable_mp(this, &MarkdownPreviewEditorPlugin::refresh_preview));
		add_child(refresh_timer);
	}
	Button *button = memnew(Button);
	button->set_name("MarkdownPreviewToggle");
	const String locale = TranslationServer::get_singleton()->get_tool_locale();
	button->set_text(locale.begins_with("zh") ? String::utf8("预览") : String("Preview"));
	button->set_theme_type_variation("FlatButton");
	button->set_toggle_mode(true);
	button->hide();
	button->connect("toggled", callable_mp(this, &MarkdownPreviewEditorPlugin::toggle_preview));
	menu->add_child(button);
	const Ref<Texture2D> online_docs_icon = menu->get_theme_icon("ExternalLink", "EditorIcons");
	for (int32_t i = 0; i < menu->get_child_count(); ++i) {
		const Button *menu_button = Object::cast_to<Button>(menu->get_child(i));
		if (menu_button != nullptr && online_docs_icon.is_valid() && menu_button->get_button_icon() == online_docs_icon) {
			menu->move_child(button, i);
			break;
		}
	}

	script_editor_id = editor->get_instance_id();
	tab_container_id = tabs->get_instance_id();
	script_list_id = script_list->get_instance_id();
	filter_id = filter->get_instance_id();
	button_id = button->get_instance_id();
	editor->connect("visibility_changed", callable_mp(this, &MarkdownPreviewEditorPlugin::queue_tab_sync));
	tabs->connect("tab_changed", callable_mp(this, &MarkdownPreviewEditorPlugin::queue_tab_sync).unbind(1));
	tabs->connect("child_order_changed", callable_mp(this, &MarkdownPreviewEditorPlugin::queue_tab_sync));
	filter->connect("text_changed", callable_mp(this, &MarkdownPreviewEditorPlugin::queue_tab_sync).unbind(1));
	sync_current_tab();
}

void MarkdownPreviewEditorPlugin::queue_tab_sync() {
	if (!sync_queued) {
		sync_queued = true;
		callable_mp(this, &MarkdownPreviewEditorPlugin::sync_current_tab).call_deferred();
	}
}

void MarkdownPreviewEditorPlugin::sync_current_tab() {
	sync_queued = false;
	for (auto it = previews.begin(); it != previews.end();) {
		const uint64_t tab_id = it->key;
		++it;
		if (ObjectDB::get_instance(tab_id) == nullptr) {
			previews.erase(tab_id);
		}
	}
	Button *button = get_node_by_id<Button>(button_id);
	ScriptEditor *editor = get_node_by_id<ScriptEditor>(script_editor_id);
	ScriptEditorBase *tab = editor && editor->is_visible_in_tree() ? editor->get_current_editor() : nullptr;
	const uint64_t tab_id = tab ? tab->get_instance_id() : 0;
	if (tab_id != active_tab_id) {
		if (ScriptEditorBase *previous = get_node_by_id<ScriptEditorBase>(active_tab_id)) {
			previous->disconnect("name_changed", callable_mp(this, &MarkdownPreviewEditorPlugin::queue_tab_sync));
		}
		active_tab_id = tab_id;
		if (tab != nullptr) {
			tab->connect("name_changed", callable_mp(this, &MarkdownPreviewEditorPlugin::queue_tab_sync));
		}
	}
	CodeEdit *source = tab ? Object::cast_to<CodeEdit>(tab->get_base_editor()) : nullptr;
	const String path = source ? get_document_path(tab, source) : String();
	const bool is_markdown = path.get_extension().nocasecmp_to("md") == 0;
	const PreviewTab *preview_tab = previews.getptr(tab_id);
	MarkdownLabel *label = preview_tab ? get_node_by_id<MarkdownLabel>(preview_tab->label_id) : nullptr;
	if (label != nullptr && !is_markdown) {
		label->hide();
	}
	const bool enabled = is_markdown && label && label->is_visible();
	if (button != nullptr) {
		button->set_pressed_no_signal(enabled);
		button->set_visible(is_markdown);
	}
	const String previous_path = active_path;
	const uint64_t previous_source_id = source_id;
	active_path = is_markdown ? path : String();
	set_preview_source(enabled ? source : nullptr, enabled ? label : nullptr);
	if (enabled && (source_id != previous_source_id || active_path != previous_path)) {
		refresh_preview();
	}
}

String MarkdownPreviewEditorPlugin::get_document_path(ScriptEditorBase *p_tab, CodeEdit *p_source) const {
	const Ref<SyntaxHighlighter> highlighter = p_source->get_syntax_highlighter();
	if (highlighter.is_valid() && highlighter->has_method("_get_edited_resource")) {
		const Ref<Resource> resource = highlighter->call("_get_edited_resource");
		if (resource.is_valid() && !resource->get_path().is_empty()) {
			return resource->get_path();
		}
	}
	TabContainer *tabs = get_node_by_id<TabContainer>(tab_container_id);
	ItemList *list = get_node_by_id<ItemList>(script_list_id);
	if (tabs != nullptr && list != nullptr && p_tab->get_parent() == tabs) {
		const int32_t tab_index = tabs->get_tab_idx_from_control(p_tab);
		for (int32_t i = 0; i < list->get_item_count(); ++i) {
			const Variant metadata = list->get_item_metadata(i);
			if (metadata.get_type() == Variant::INT && int64_t(metadata) == tab_index) {
				return list->get_item_tooltip(i);
			}
		}
	}
	return String();
}

void MarkdownPreviewEditorPlugin::set_preview_source(CodeEdit *p_source, MarkdownLabel *p_label) {
	const uint64_t next_id = p_source ? p_source->get_instance_id() : 0;
	const uint64_t next_preview_id = p_label ? p_label->get_instance_id() : 0;
	if (next_id == source_id && next_preview_id == active_preview_id) {
		return;
	}
	refresh_timer->stop();
	const Callable text_changed = callable_mp(this, &MarkdownPreviewEditorPlugin::queue_preview_refresh);
	const Callable align_from_source = callable_mp(this, &MarkdownPreviewEditorPlugin::queue_scroll_sync).bind(false);
	const Callable source_scrolled = align_from_source.unbind(1);
	const Callable preview_scrolled = callable_mp(this, &MarkdownPreviewEditorPlugin::queue_scroll_sync).bind(true).unbind(1);
	if (CodeEdit *old_source = get_node_by_id<CodeEdit>(source_id)) {
		old_source->disconnect("text_changed", text_changed);
		old_source->disconnect("text_set", text_changed);
		old_source->get_v_scroll_bar()->disconnect("value_changed", source_scrolled);
		old_source->get_v_scroll_bar()->disconnect("changed", align_from_source);
	}
	if (MarkdownLabel *old_label = get_node_by_id<MarkdownLabel>(active_preview_id)) {
		old_label->get_v_scroll_bar()->disconnect("value_changed", preview_scrolled);
		old_label->get_v_scroll_bar()->disconnect("changed", align_from_source);
	}
	source_id = next_id;
	active_preview_id = next_preview_id;
	if (p_source != nullptr) {
		p_source->connect("text_changed", text_changed);
		p_source->connect("text_set", text_changed);
		p_source->get_v_scroll_bar()->connect("value_changed", source_scrolled);
		p_source->get_v_scroll_bar()->connect("changed", align_from_source);
	}
	if (p_label != nullptr) {
		p_label->get_v_scroll_bar()->connect("value_changed", preview_scrolled);
		p_label->get_v_scroll_bar()->connect("changed", align_from_source);
	}
}

MarkdownLabel *MarkdownPreviewEditorPlugin::create_preview(ScriptEditorBase *p_tab, CodeEdit *p_source) {
	VBoxContainer *editor_area = Object::cast_to<VBoxContainer>(p_source->get_parent());
	if (editor_area == nullptr || editor_area->get_parent() != p_tab) {
		return nullptr;
	}
	const int32_t original_index = editor_area->get_index();
	const BitField<Control::SizeFlags> original_h_flags = editor_area->get_h_size_flags();
	HSplitContainer *split = memnew(HSplitContainer);
	split->set_v_size_flags(editor_area->get_v_size_flags());
	p_tab->add_child(split);
	p_tab->move_child(split, original_index);
	editor_area->reparent(split, false);
	editor_area->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	MarkdownLabel *label = memnew(MarkdownLabel);
	label->set_name("MarkdownPreview");
	label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	label->set_custom_minimum_size(Vector2(80.0, 0.0));
	split->add_child(label);
	previews.insert(p_tab->get_instance_id(), { split->get_instance_id(), editor_area->get_instance_id(), label->get_instance_id(), original_h_flags });
	return label;
}

void MarkdownPreviewEditorPlugin::toggle_preview(bool p_pressed) {
	sync_current_tab();
	ScriptEditorBase *tab = get_node_by_id<ScriptEditorBase>(active_tab_id);
	if (tab == nullptr || active_path.is_empty()) {
		return;
	}
	CodeEdit *source = Object::cast_to<CodeEdit>(tab->get_base_editor());
	if (source == nullptr) {
		return;
	}
	const PreviewTab *preview_tab = previews.getptr(active_tab_id);
	MarkdownLabel *label = preview_tab ? get_node_by_id<MarkdownLabel>(preview_tab->label_id) : nullptr;
	if (p_pressed && label == nullptr) {
		label = create_preview(tab, source);
	}
	if (label != nullptr) {
		label->set_visible(p_pressed);
		sync_current_tab();
		source->grab_focus();
	}
}

void MarkdownPreviewEditorPlugin::queue_preview_refresh() {
	refresh_timer->start();
}

void MarkdownPreviewEditorPlugin::refresh_preview() {
	ScriptEditor *editor = get_node_by_id<ScriptEditor>(script_editor_id);
	ScriptEditorBase *tab = editor && editor->is_visible_in_tree() ? editor->get_current_editor() : nullptr;
	if (tab == nullptr || tab->get_instance_id() != active_tab_id) {
		return;
	}
	CodeEdit *source = get_node_by_id<CodeEdit>(source_id);
	MarkdownLabel *label = get_node_by_id<MarkdownLabel>(active_preview_id);
	if (source != nullptr && label != nullptr && label->is_visible_in_tree()) {
		label->set_image_base_dir(active_path.get_base_dir());
		const String text = source->get_text();
		if (text != label->get_markdown_text()) {
			label->set_markdown_text(text);
		}
		preview_source_version = source->get_version();
		queue_scroll_sync(false);
	}
}

void MarkdownPreviewEditorPlugin::queue_scroll_sync(bool p_from_preview) {
	if (syncing_scroll || source_id == 0) {
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
	CodeEdit *source = get_node_by_id<CodeEdit>(source_id);
	MarkdownLabel *label = get_node_by_id<MarkdownLabel>(active_preview_id);
	if (source == nullptr || label == nullptr || !label->is_visible_in_tree() || !refresh_timer->is_stopped() || source->get_version() != preview_source_version) {
		return;
	}
	VScrollBar *source_scroll = source->get_v_scroll_bar();
	VScrollBar *preview_scroll = label->get_v_scroll_bar();
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
		} else {
			const double source_position = label->get_source_line_at_scroll_offset(offset, line_count);
			const int32_t line = std::clamp(static_cast<int32_t>(source_position), 0, line_count - 1);
			const int32_t wraps = source->get_line_wrap_count(line);
			const int32_t wrap = std::clamp(static_cast<int32_t>((source_position - line) * (wraps + 1)), 0, wraps);
			source->set_line_as_first_visible(line, wrap);
		}
	} else if (source_end > 0.0 && source_scroll->get_value() >= source_end) {
		preview_scroll->set_value(preview_end);
	} else {
		const int32_t line = source->get_first_visible_line();
		const int32_t wraps = source->get_line_wrap_count(line);
		const double wrap_offset = source_scroll->get_value() - source->get_scroll_pos_for_line(line);
		const double fraction = std::clamp(wrap_offset / (wraps + 1), 0.0, 1.0);
		preview_scroll->set_value(label->get_scroll_offset_for_source_line(line + fraction, line_count));
	}
	syncing_scroll = false;
}

} // namespace godot
