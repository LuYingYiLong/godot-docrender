#include "html_preview_editor_plugin.h"

#include "nodes/html_label.h"

#include <godot_cpp/classes/code_edit.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>

namespace godot {
	Control* HTMLPreviewEditorPlugin::create_preview_panel() {
		PanelContainer* panel = memnew(PanelContainer);
		panel->set_name("HTMLPreviewPanel");
		Ref<StyleBoxFlat> background;
		background.instantiate();
		background->set_bg_color(Color(1.0f, 1.0f, 1.0f));
		panel->add_theme_stylebox_override("panel", background);

		HTMLLabel* label = memnew(HTMLLabel);
		label->set_name("HTMLPreview");
		label->set_css_diagnostics_enabled(true);
		panel->add_child(label);
		return panel;
	}

	void HTMLPreviewEditorPlugin::update_preview(CodeEdit* p_source, Control* p_preview, const String& p_path) {
		HTMLLabel* label = p_preview->get_node<HTMLLabel>("HTMLPreview");
		label->set_base_dir(p_path.get_base_dir());
		label->set_text(p_source->get_text());
	}
} // namespace godot
