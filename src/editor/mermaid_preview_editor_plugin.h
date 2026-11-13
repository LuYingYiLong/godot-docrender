#pragma once

#include "script_editor_preview_plugin.h"

namespace godot {
	class MermaidPreviewEditorPlugin : public ScriptEditorPreviewPlugin {
		GDCLASS(MermaidPreviewEditorPlugin, ScriptEditorPreviewPlugin)

	protected:
		static void _bind_methods() {}

		bool supports_source_extension(const String& p_extension) const override { return p_extension == "mmd"; }
		const char* get_menu_name() const override { return "MermaidPreviewMenu"; }
		Control* create_preview_panel() override;
		void update_preview(CodeEdit* p_source, Control* p_preview, const String& p_path) override;

	private:
		static void update_diagnostic(uint64_t p_diagram_id, uint64_t p_diagnostic_id);
	};
} // namespace godot
