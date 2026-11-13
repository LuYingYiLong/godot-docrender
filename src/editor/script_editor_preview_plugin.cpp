#include "script_editor_preview_plugin.h"

#include "docrender_source_editor.h"

#include <godot_cpp/classes/code_edit.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_split_container.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/menu_button.hpp>
#include <godot_cpp/classes/popup_menu.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/script_editor.hpp>
#include <godot_cpp/classes/script_editor_base.hpp>
#include <godot_cpp/classes/syntax_highlighter.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/v_split_container.hpp>
#include <godot_cpp/core/object.hpp>

namespace godot {
	namespace {
		template <typename T>
		T* find_child_by_type(Node* p_parent) {
			T* match = nullptr;
			if (p_parent == nullptr) {
				return match;
			}
			for (int32_t i = 0; i < p_parent->get_child_count(); ++i) {
				if (T* child = Object::cast_to<T>(p_parent->get_child(i))) {
					if (match != nullptr) {
						return nullptr;
					}
					match = child;
				}
			}
			return match;
		}

		template <typename T>
		T* get_node_by_id(uint64_t p_id) {
			return Object::cast_to<T>(ObjectDB::get_instance(p_id));
		}
	} // namespace

	void ScriptEditorPreviewPlugin::_enter_tree() {
		set_translation_domain("godot.editor");
		const Dictionary version = Engine::get_singleton()->get_version_info();
		if (int64_t(version["major"]) == 4 && int64_t(version["minor"]) == 7 && int64_t(version["patch"]) == 2) {
			callable_mp(this, &ScriptEditorPreviewPlugin::attach).call_deferred();
		}
	}

	void ScriptEditorPreviewPlugin::_exit_tree() {
		set_preview_source(nullptr, nullptr);
		if (ScriptEditorBase* tab = get_node_by_id<ScriptEditorBase>(active_tab_id)) {
			tab->disconnect("name_changed", callable_mp(this, &ScriptEditorPreviewPlugin::queue_tab_sync));
		}
		if (ScriptEditor* editor = get_node_by_id<ScriptEditor>(script_editor_id)) {
			editor->disconnect("visibility_changed", callable_mp(this, &ScriptEditorPreviewPlugin::queue_tab_sync));
		}
		if (TabContainer* tabs = get_node_by_id<TabContainer>(tab_container_id)) {
			tabs->disconnect("tab_changed", callable_mp(this, &ScriptEditorPreviewPlugin::queue_tab_sync).unbind(1));
			tabs->disconnect("child_order_changed", callable_mp(this, &ScriptEditorPreviewPlugin::queue_tab_sync));
		}
		if (LineEdit* filter = get_node_by_id<LineEdit>(filter_id)) {
			filter->disconnect("text_changed", callable_mp(this, &ScriptEditorPreviewPlugin::queue_tab_sync).unbind(1));
		}
		if (MenuButton* mode_menu = get_node_by_id<MenuButton>(mode_menu_id)) {
			memdelete(mode_menu);
		}
		for (const KeyValue<uint64_t, PreviewTab>& entry : previews) {
			restore_preview_tab(entry.key, entry.value);
		}
		previews.clear();
		active_path = String();
		active_tab_id = 0;
		mode_menu_id = 0;
		filter_id = 0;
		script_list_id = 0;
		tab_container_id = 0;
		script_editor_id = 0;
		sync_queued = false;
	}

	void ScriptEditorPreviewPlugin::attach() {
		if (!is_inside_tree() || script_editor_id != 0) {
			return;
		}
		ScriptEditor* editor = EditorInterface::get_singleton()->get_script_editor();
		// This is the private ScriptEditor layout in Godot 4.7.2.
		VBoxContainer* main = find_child_by_type<VBoxContainer>(editor);
		HBoxContainer* toolbar = find_child_by_type<HBoxContainer>(main);
		HSplitContainer* body = find_child_by_type<HSplitContainer>(main);
		VBoxContainer* editor_column = find_child_by_type<VBoxContainer>(body);
		TabContainer* tabs = find_child_by_type<TabContainer>(editor_column);
		VSplitContainer* sidebar = find_child_by_type<VSplitContainer>(body);
		VBoxContainer* file_column = Object::cast_to<VBoxContainer>(sidebar && sidebar->get_child_count() > 0 ? sidebar->get_child(0) : nullptr);
		ItemList* script_list = find_child_by_type<ItemList>(file_column);
		LineEdit* filter = find_child_by_type<LineEdit>(file_column);
		if (toolbar == nullptr || tabs == nullptr || tabs->are_tabs_visible() || script_list == nullptr || filter == nullptr) {
			return;
		}

		if (refresh_timer == nullptr) {
			refresh_timer = memnew(Timer);
			refresh_timer->set_one_shot(true);
			refresh_timer->set_wait_time(0.15);
			refresh_timer->connect("timeout", callable_mp(this, &ScriptEditorPreviewPlugin::refresh_preview));
			add_child(refresh_timer);
		}
		MenuButton* mode_menu = memnew(MenuButton);
		mode_menu->set_name(get_menu_name());
		mode_menu->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
		mode_menu->set_text(tr("Preview", "Godot DocRender"));
		mode_menu->set_flat(false);
		mode_menu->set_theme_type_variation("FlatMenuButton");
		mode_menu->set_switch_on_hover(true);
		PopupMenu* popup = mode_menu->get_popup();
		popup->add_radio_check_item(tr("View Source", "Godot DocRender"), VIEW_SOURCE);
		popup->add_radio_check_item(tr("Open Preview to the Right", "Godot DocRender"), VIEW_SPLIT);
		popup->add_radio_check_item(tr("Preview Only", "Godot DocRender"), VIEW_PREVIEW);
		popup->connect("id_pressed", callable_mp(this, &ScriptEditorPreviewPlugin::select_view_mode));
		mode_menu->hide();
		toolbar->add_child(mode_menu);
		// The expanding document title follows the native Debug menu.
		for (int32_t i = 1; i < toolbar->get_child_count(); ++i) {
			const HBoxContainer* title = Object::cast_to<HBoxContainer>(toolbar->get_child(i));
			if (title != nullptr && title->get_h_size_flags().has_flag(Control::SIZE_EXPAND) && Object::cast_to<MenuButton>(toolbar->get_child(i - 1)) != nullptr) {
				toolbar->move_child(mode_menu, i);
				break;
			}
		}

		script_editor_id = editor->get_instance_id();
		tab_container_id = tabs->get_instance_id();
		script_list_id = script_list->get_instance_id();
		filter_id = filter->get_instance_id();
		mode_menu_id = mode_menu->get_instance_id();
		editor->connect("visibility_changed", callable_mp(this, &ScriptEditorPreviewPlugin::queue_tab_sync));
		tabs->connect("tab_changed", callable_mp(this, &ScriptEditorPreviewPlugin::queue_tab_sync).unbind(1));
		tabs->connect("child_order_changed", callable_mp(this, &ScriptEditorPreviewPlugin::queue_tab_sync));
		filter->connect("text_changed", callable_mp(this, &ScriptEditorPreviewPlugin::queue_tab_sync).unbind(1));
		sync_current_tab();
	}

	void ScriptEditorPreviewPlugin::queue_tab_sync() {
		if (is_inside_tree() && !sync_queued) {
			sync_queued = true;
			callable_mp(this, &ScriptEditorPreviewPlugin::sync_current_tab).call_deferred();
		}
	}

	void ScriptEditorPreviewPlugin::sync_current_tab() {
		sync_queued = false;
		if (!is_inside_tree()) {
			return;
		}
		for (auto it = previews.begin(); it != previews.end();) {
			const uint64_t tab_id = it->key;
			++it;
			if (ObjectDB::get_instance(tab_id) == nullptr) {
				previews.erase(tab_id);
			}
		}
		MenuButton* mode_menu = get_node_by_id<MenuButton>(mode_menu_id);
		ScriptEditor* editor = get_node_by_id<ScriptEditor>(script_editor_id);
		ScriptEditorBase* tab = editor && editor->is_visible_in_tree() ? editor->get_current_editor() : nullptr;
		const uint64_t tab_id = tab ? tab->get_instance_id() : 0;
		if (tab_id != active_tab_id) {
			if (ScriptEditorBase* previous = get_node_by_id<ScriptEditorBase>(active_tab_id)) {
				previous->disconnect("name_changed", callable_mp(this, &ScriptEditorPreviewPlugin::queue_tab_sync));
			}
			active_tab_id = tab_id;
			if (tab != nullptr) {
				tab->connect("name_changed", callable_mp(this, &ScriptEditorPreviewPlugin::queue_tab_sync));
			}
		}
		CodeEdit* source = tab ? Object::cast_to<CodeEdit>(tab->get_base_editor()) : nullptr;
		const String path = source ? get_document_path(tab, source) : String();
		const bool matches_source = supports_source_extension(path.get_extension().to_lower());
		const PreviewTab* preview_tab = previews.getptr(tab_id);
		if (!matches_source && !path.is_empty() && preview_tab != nullptr) {
			// A tab can adopt a new extension after Save As.
			set_preview_source(nullptr, nullptr);
			restore_preview_tab(tab_id, *preview_tab);
			previews.erase(tab_id);
			preview_tab = nullptr;
		}
		Control* preview = preview_tab ? get_node_by_id<Control>(preview_tab->preview_id) : nullptr;
		Control* editor_area = preview_tab ? get_node_by_id<Control>(preview_tab->editor_area_id) : nullptr;
		const ViewMode mode = matches_source && preview && editor_area ? preview_tab->mode : VIEW_SOURCE;
		const bool enabled = mode != VIEW_SOURCE;
		if (editor_area != nullptr) {
			editor_area->set_visible(mode != VIEW_PREVIEW);
		}
		if (preview != nullptr) {
			preview->set_visible(enabled);
		}
		if (mode_menu != nullptr) {
			PopupMenu* popup = mode_menu->get_popup();
			for (int32_t i = 0; i < popup->get_item_count(); ++i) {
				popup->set_item_checked(i, popup->get_item_id(i) == mode);
			}
			mode_menu->set_visible(matches_source);
		}
		const String previous_path = active_path;
		const uint64_t previous_source_id = source_id;
		active_path = matches_source ? path : String();
		set_preview_source(enabled ? source : nullptr, enabled ? preview : nullptr);
		if (enabled && (source_id != previous_source_id || active_path != previous_path)) {
			refresh_preview();
		}
	}

	String ScriptEditorPreviewPlugin::get_document_path(ScriptEditorBase* p_tab, CodeEdit* p_source) const {
		const Ref<SyntaxHighlighter> highlighter = p_source->get_syntax_highlighter();
		if (highlighter.is_valid() && highlighter->has_method("_get_edited_resource")) {
			const Ref<Resource> resource = highlighter->call("_get_edited_resource");
			if (resource.is_valid() && !resource->get_path().is_empty()) {
				return resource->get_path();
			}
		}
		TabContainer* tabs = get_node_by_id<TabContainer>(tab_container_id);
		ItemList* list = get_node_by_id<ItemList>(script_list_id);
		if (tabs != nullptr && list != nullptr && p_tab->get_parent() == tabs) {
			const int32_t tab_index = tabs->get_tab_idx_from_control(p_tab);
			for (int32_t i = 0; i < list->get_item_count(); ++i) {
				const Variant metadata = list->get_item_metadata(i);
				if (metadata.get_type() == Variant::INT && int64_t(metadata) == tab_index) {
					return list->get_item_tooltip(i);
				}
			}
		}
		// Imported sources are tracked even when ScriptEditor's list is filtered.
		return DocRenderSourceEditorPlugin::get_source_path(p_tab->get_instance_id());
	}

	void ScriptEditorPreviewPlugin::set_preview_source(CodeEdit* p_source, Control* p_preview) {
		const uint64_t next_id = p_source ? p_source->get_instance_id() : 0;
		const uint64_t next_preview_id = p_preview ? p_preview->get_instance_id() : 0;
		if (next_id == source_id && next_preview_id == active_preview_id) {
			return;
		}
		refresh_timer->stop();
		const Callable text_changed = callable_mp(this, &ScriptEditorPreviewPlugin::queue_preview_refresh);
		CodeEdit* old_source = get_active_source();
		if (old_source != nullptr) {
			old_source->disconnect("text_changed", text_changed);
			old_source->disconnect("text_set", text_changed);
		}
		set_scroll_connections(old_source, get_active_preview(), false);
		source_id = next_id;
		active_preview_id = next_preview_id;
		if (p_source != nullptr) {
			p_source->connect("text_changed", text_changed);
			p_source->connect("text_set", text_changed);
		}
		set_scroll_connections(p_source, p_preview, true);
	}

	CodeEdit* ScriptEditorPreviewPlugin::get_active_source() const {
		return get_node_by_id<CodeEdit>(source_id);
	}

	Control* ScriptEditorPreviewPlugin::get_active_preview() const {
		return get_node_by_id<Control>(active_preview_id);
	}

	bool ScriptEditorPreviewPlugin::is_refresh_pending() const {
		return refresh_timer != nullptr && !refresh_timer->is_stopped();
	}

	ScriptEditorPreviewPlugin::PreviewTab* ScriptEditorPreviewPlugin::create_preview(ScriptEditorBase* p_tab, CodeEdit* p_source) {
		VBoxContainer* editor_area = Object::cast_to<VBoxContainer>(p_source->get_parent());
		if (editor_area == nullptr || editor_area->get_parent() != p_tab) {
			return nullptr;
		}
		const int32_t original_index = editor_area->get_index();
		const BitField<Control::SizeFlags> original_h_flags = editor_area->get_h_size_flags();
		HSplitContainer* split = memnew(HSplitContainer);
		split->set_v_size_flags(editor_area->get_v_size_flags());
		p_tab->add_child(split);
		p_tab->move_child(split, original_index);
		editor_area->reparent(split, false);
		editor_area->set_h_size_flags(Control::SIZE_EXPAND_FILL);

		Control* preview = create_preview_panel();
		preview->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		preview->set_custom_minimum_size(Vector2(80.0, 0.0));
		split->add_child(preview);
		return &previews.insert(p_tab->get_instance_id(), {
			split->get_instance_id(), editor_area->get_instance_id(), preview->get_instance_id(), original_h_flags
		})->value;
	}

	void ScriptEditorPreviewPlugin::restore_preview_tab(uint64_t p_tab_id, const PreviewTab& p_preview) {
		ScriptEditorBase* tab = get_node_by_id<ScriptEditorBase>(p_tab_id);
		HSplitContainer* split = get_node_by_id<HSplitContainer>(p_preview.split_id);
		Control* editor_area = get_node_by_id<Control>(p_preview.editor_area_id);
		if (tab == nullptr || split == nullptr || editor_area == nullptr ||
				split->get_parent() != tab || editor_area->get_parent() != split) {
			return;
		}
		const int32_t original_index = split->get_index();
		editor_area->reparent(tab, false);
		editor_area->set_h_size_flags(p_preview.editor_area_h_flags);
		editor_area->show();
		tab->move_child(editor_area, original_index);
		memdelete(split);
	}

	void ScriptEditorPreviewPlugin::select_view_mode(int32_t p_id) {
		if (p_id < VIEW_SOURCE || p_id > VIEW_PREVIEW) {
			return;
		}
		sync_current_tab();
		ScriptEditorBase* tab = get_node_by_id<ScriptEditorBase>(active_tab_id);
		if (tab == nullptr || active_path.is_empty()) {
			return;
		}
		CodeEdit* source = Object::cast_to<CodeEdit>(tab->get_base_editor());
		if (source == nullptr) {
			return;
		}
		const ViewMode mode = static_cast<ViewMode>(p_id);
		PreviewTab* preview_tab = previews.getptr(active_tab_id);
		if (preview_tab == nullptr && mode != VIEW_SOURCE) {
			preview_tab = create_preview(tab, source);
		}
		if (preview_tab != nullptr) {
			preview_tab->mode = mode;
			sync_current_tab();
		}
		if (mode == VIEW_SPLIT) {
			align_preview_to_source();
		}
		if (source->is_visible_in_tree()) {
			source->grab_focus();
		}
	}

	void ScriptEditorPreviewPlugin::queue_preview_refresh() {
		refresh_timer->start();
	}

	void ScriptEditorPreviewPlugin::refresh_preview() {
		ScriptEditor* editor = get_node_by_id<ScriptEditor>(script_editor_id);
		ScriptEditorBase* tab = editor && editor->is_visible_in_tree() ? editor->get_current_editor() : nullptr;
		if (tab == nullptr || tab->get_instance_id() != active_tab_id) {
			return;
		}
		CodeEdit* source = get_active_source();
		Control* preview = get_active_preview();
		if (source != nullptr && preview != nullptr && preview->is_visible_in_tree()) {
			update_preview(source, preview, active_path);
		}
	}
} // namespace godot
