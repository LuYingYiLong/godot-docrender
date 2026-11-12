#include "mermaid_preview_editor_plugin.h"

#include "nodes/mermaid_label.h"

#include <godot_cpp/classes/code_edit.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/object.hpp>

namespace godot {
	Control* MermaidPreviewEditorPlugin::create_preview_panel() {
		ScrollContainer* panel = memnew(ScrollContainer);
		panel->set_name("MermaidPreviewPanel");

		VBoxContainer* content = memnew(VBoxContainer);
		content->set_name("Content");
		content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		content->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		panel->add_child(content);

		MermaidLabel* diagram = memnew(MermaidLabel);
		diagram->set_name("MermaidPreview");
		diagram->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		content->add_child(diagram);

		Label* diagnostic = memnew(Label);
		diagnostic->set_name("RenderError");
		diagnostic->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
		diagnostic->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		diagnostic->hide();
		content->add_child(diagnostic);
		diagram->connect("render_finished", callable_mp_static(&MermaidPreviewEditorPlugin::update_diagnostic)
			.bind(diagram->get_instance_id(), diagnostic->get_instance_id()));
		return panel;
	}

	void MermaidPreviewEditorPlugin::update_preview(CodeEdit* p_source, Control* p_preview, const String&) {
		MermaidLabel* diagram = p_preview->get_node<MermaidLabel>("Content/MermaidPreview");
		Label* diagnostic = p_preview->get_node<Label>("Content/RenderError");
		diagram->set_text(p_source->get_text());
		update_diagnostic(diagram->get_instance_id(), diagnostic->get_instance_id());
	}

	void MermaidPreviewEditorPlugin::update_diagnostic(uint64_t p_diagram_id, uint64_t p_diagnostic_id) {
		const MermaidLabel* diagram = Object::cast_to<MermaidLabel>(ObjectDB::get_instance(p_diagram_id));
		Label* diagnostic = Object::cast_to<Label>(ObjectDB::get_instance(p_diagnostic_id));
		if (diagram != nullptr && diagnostic != nullptr) {
			const String error = diagram->get_render_error();
			diagnostic->set_text(error);
			diagnostic->set_visible(!error.is_empty());
		}
	}
} // namespace godot
