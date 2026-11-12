#include "register_types.h"

#include "editor/docrender_source_editor.h"
#include "editor/latex_document_importer.h"
#include "editor/mermaid_document_importer.h"
#include "editor/pdf_texture_importer.h"
#include "latex_document.h"
#include "latex_document_loader.h"
#include "mermaid_document.h"
#include "pdf_reader.h"
#include "pdf_texture.h"
#include "pdf_writer.h"

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
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

namespace {
	bool editor_plugins_registered = false;
	Ref<DocRenderTexLoader> tex_loader;
}

void initialize_docrender_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_INTERNAL_CLASS(MarkdownLabelCanvas);
		GDREGISTER_CLASS(LatexDocument);
		GDREGISTER_CLASS(MermaidDocument);
		GDREGISTER_CLASS(PDFReader);
		GDREGISTER_CLASS(PDFWriter);
		GDREGISTER_CLASS(PDFTexture);
		GDREGISTER_INTERNAL_CLASS(DocRenderTexLoader);
		GDREGISTER_CLASS(MarkdownLabel);
		GDREGISTER_CLASS(TeXRender);
		GDREGISTER_CLASS(LatexLabel);
		GDREGISTER_CLASS(LatexLabel3D);
		GDREGISTER_CLASS(MermaidRender);
		GDREGISTER_CLASS(MermaidLabel);

		tex_loader.instantiate();
		ResourceLoader::get_singleton()->add_resource_format_loader(tex_loader, true);
	}

	else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR && Engine::get_singleton()->is_editor_hint()) {
		GDREGISTER_INTERNAL_CLASS(MarkdownPreviewEditorPlugin);
		EditorPlugins::add_by_type<MarkdownPreviewEditorPlugin>();
		GDREGISTER_INTERNAL_CLASS(LatexDocumentImporter);
		GDREGISTER_INTERNAL_CLASS(MermaidDocumentImporter);
		if (PDFTextureImporter::is_supported()) {
			GDREGISTER_INTERNAL_CLASS(PDFTextureImporter);
		}
		GDREGISTER_INTERNAL_CLASS(DocRenderSourceEditorPlugin);
		DocRenderSourceEditorPlugin::prepare_registration();
		EditorPlugins::add_by_type<DocRenderSourceEditorPlugin>();
		editor_plugins_registered = true;
	}
}

void uninitialize_docrender_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR && editor_plugins_registered) {
		EditorPlugins::remove_by_type<DocRenderSourceEditorPlugin>();
		EditorPlugins::remove_by_type<MarkdownPreviewEditorPlugin>();
		editor_plugins_registered = false;
	}
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		ResourceLoader::get_singleton()->remove_resource_format_loader(tex_loader);
		tex_loader.unref();
		PDFReader::shutdown_backend();
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
