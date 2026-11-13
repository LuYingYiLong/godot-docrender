#pragma once

#include "script_editor_preview_plugin.h"

namespace godot {
	class MarkdownPreviewEditorPlugin : public ScriptEditorPreviewPlugin {
		GDCLASS(MarkdownPreviewEditorPlugin, ScriptEditorPreviewPlugin)

	protected:
		static void _bind_methods() {}

		bool supports_source_extension(const String& p_extension) const override { return p_extension == "md"; }
		const char* get_menu_name() const override { return "MarkdownPreviewMenu"; }
		Control* create_preview_panel() override;
		void update_preview(CodeEdit* p_source, Control* p_preview, const String& p_path) override;
		void set_scroll_connections(CodeEdit* p_source, Control* p_preview, bool p_connect) override;
		void align_preview_to_source() override;

	private:
		uint32_t preview_source_version = 0;
		bool scroll_sync_queued = false;
		bool scroll_from_preview = false;
		bool syncing_scroll = false;

		void queue_scroll_sync(bool p_from_preview);
		void sync_scroll();
	};
} // namespace godot
