#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/templates/hash_map.hpp>

namespace godot {
	class CodeEdit;
	class ScriptEditorBase;
	class Timer;

	class ScriptEditorPreviewPlugin : public EditorPlugin {
		GDCLASS(ScriptEditorPreviewPlugin, EditorPlugin)

	public:
		void _enter_tree() override;
		void _exit_tree() override;

	protected:
		static void _bind_methods() {}

		virtual const char* get_source_extension() const = 0;
		virtual const char* get_menu_name() const = 0;
		virtual Control* create_preview_panel() = 0;
		virtual void update_preview(CodeEdit* p_source, Control* p_preview, const String& p_path) = 0;
		virtual void set_scroll_connections(CodeEdit*, Control*, bool) {}
		virtual void align_preview_to_source() {}

		CodeEdit* get_active_source() const;
		Control* get_active_preview() const;
		bool is_refresh_pending() const;

	private:
		enum ViewMode {
			VIEW_SOURCE,
			VIEW_SPLIT,
			VIEW_PREVIEW,
		};

		struct PreviewTab {
			uint64_t split_id = 0;
			uint64_t editor_area_id = 0;
			uint64_t preview_id = 0;
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
		String active_path;
		Timer* refresh_timer = nullptr;
		bool sync_queued = false;

		void attach();
		void queue_tab_sync();
		void sync_current_tab();
		String get_document_path(ScriptEditorBase* p_tab, CodeEdit* p_source) const;
		void set_preview_source(CodeEdit* p_source, Control* p_preview);
		PreviewTab* create_preview(ScriptEditorBase* p_tab, CodeEdit* p_source);
		void restore_preview_tab(uint64_t p_tab_id, const PreviewTab& p_preview);
		void select_view_mode(int32_t p_id);
		void queue_preview_refresh();
		void refresh_preview();
	};
} // namespace godot
