#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/templates/hash_map.hpp>

class MarkdownLabel;

namespace godot {

class CodeEdit;
class ScriptEditorBase;
class Timer;

class MarkdownPreviewEditorPlugin : public EditorPlugin {
	GDCLASS(MarkdownPreviewEditorPlugin, EditorPlugin);

	enum ViewMode {
		VIEW_SOURCE,
		VIEW_SPLIT,
		VIEW_PREVIEW,
	};

	struct PreviewTab {
		uint64_t split_id = 0;
		uint64_t editor_area_id = 0;
		uint64_t label_id = 0;
		BitField<Control::SizeFlags> editor_area_h_flags = 0;
		ViewMode mode = VIEW_SOURCE;
	};

	HashMap<uint64_t, PreviewTab> previews;
	uint64_t script_editor_id = 0;
	uint64_t tab_container_id = 0;
	uint64_t script_list_id = 0;
	uint64_t filter_id = 0;
	uint64_t mode_menu_id = 0;
	uint64_t active_tab_id = 0;
	uint64_t source_id = 0;
	uint64_t active_preview_id = 0;
	uint32_t preview_source_version = 0;
	String active_path;
	Timer *refresh_timer = nullptr;
	bool sync_queued = false;
	bool scroll_sync_queued = false;
	bool scroll_from_preview = false;
	bool syncing_scroll = false;

	void attach();
	void queue_tab_sync();
	void sync_current_tab();
	String get_document_path(ScriptEditorBase *p_tab, CodeEdit *p_source) const;
	void set_preview_source(CodeEdit *p_source, MarkdownLabel *p_label);
	PreviewTab *create_preview(ScriptEditorBase *p_tab, CodeEdit *p_source);
	void select_view_mode(int32_t p_id);
	void queue_preview_refresh();
	void refresh_preview();
	void queue_scroll_sync(bool p_from_preview);
	void sync_scroll();

protected:
	static void _bind_methods() {}

public:
	void _enter_tree() override;
	void _exit_tree() override;
};

} // namespace godot
