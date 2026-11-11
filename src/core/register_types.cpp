#include "register_types.h"

#include "nodes/latex_label.h"
#include "nodes/latex_label_3d.h"
#include "nodes/markdown_label.h"
#include "nodes/mermaid_label.h"
#include "editor/markdown_preview_editor_plugin.h"
#include "mermaid_render.h"
#include "tex_render.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

namespace {
	bool preview_plugin_registered = false;
}

void initialize_docrender_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_INTERNAL_CLASS(MarkdownLabelCanvas);
		GDREGISTER_CLASS(MarkdownLabel);
		GDREGISTER_CLASS(TeXRender);
		GDREGISTER_CLASS(LatexLabel);
		GDREGISTER_CLASS(LatexLabel3D);
		GDREGISTER_CLASS(MermaidRender);
		GDREGISTER_CLASS(MermaidLabel);
	}

	else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR && Engine::get_singleton()->is_editor_hint()) {
		GDREGISTER_INTERNAL_CLASS(MarkdownPreviewEditorPlugin);
		EditorPlugins::add_by_type<MarkdownPreviewEditorPlugin>();
		preview_plugin_registered = true;
	}
}

void uninitialize_docrender_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR && preview_plugin_registered) {
		EditorPlugins::remove_by_type<MarkdownPreviewEditorPlugin>();
		preview_plugin_registered = false;
	}
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		TeXRender::shutdown_renderer();
	}
}

extern "C" {
	GDExtensionBool GDE_EXPORT docrender_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization* r_initialization) {
		godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

		init_obj.register_initializer(initialize_docrender_module);
		init_obj.register_terminator(uninitialize_docrender_module);
		init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

		return init_obj.init();
	}
}
