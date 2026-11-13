#pragma once

#include "script_editor_preview_plugin.h"

namespace godot {
	class HTMLPreviewEditorPlugin : public ScriptEditorPreviewPlugin {
		GDCLASS(HTMLPreviewEditorPlugin, ScriptEditorPreviewPlugin)

	protected:
		static void _bind_methods() {}

		bool supports_source_extension(const String& p_extension) const override {
			return p_extension == "html" || p_extension == "htm";
		}
		const char* get_menu_name() const override { return "HTMLPreviewMenu"; }
		Control* create_preview_panel() override;
		void update_preview(CodeEdit* p_source, Control* p_preview, const String& p_path) override;
	};
} // namespace godot
