#include "register_types.h"

#include "editor/docrender_source_editor.h"
#include "editor/html_preview_editor_plugin.h"
#include "editor/importers/html_document_importer.h"
#include "editor/importers/latex_document_importer.h"
#include "editor/importers/mermaid_document_importer.h"
#include "editor/importers/pdf_texture_importer.h"
#include "editor/markdown_preview_editor_plugin.h"
#include "editor/mermaid_preview_editor_plugin.h"
#include "editor/script_editor_preview_plugin.h"
#include "formats/html/html_document.h"
#include "formats/latex/latex_document.h"
#include "formats/latex/latex_document_loader.h"
#include "formats/mermaid/mermaid_document.h"
#include "formats/pdf/reader/pdf_reader.h"
#include "formats/pdf/pdf_texture.h"
#include "formats/pdf/writer/pdf_writer.h"

#include "nodes/html_label.h"
#include "nodes/latex_label.h"
#include "nodes/latex_label_3d.h"
#include "nodes/markdown_label.h"
#include "nodes/mermaid_label.h"
#include "formats/mermaid/mermaid_render.h"
#include "formats/latex/tex_render.h"

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
		GDREGISTER_CLASS(HTMLLayout);
		GDREGISTER_CLASS(HTMLDocument);
		GDREGISTER_INTERNAL_CLASS(HTMLLabelCanvas);
		GDREGISTER_CLASS(HTMLLabel);
		GDREGISTER_CLASS(TeXRender);
		GDREGISTER_CLASS(LatexLabel);
		GDREGISTER_CLASS(LatexLabel3D);
		GDREGISTER_CLASS(MermaidRender);
		GDREGISTER_CLASS(MermaidLabel);

		tex_loader.instantiate();
		ResourceLoader::get_singleton()->add_resource_format_loader(tex_loader, true);
	}

	else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR && Engine::get_singleton()->is_editor_hint()) {
		GDREGISTER_ABSTRACT_CLASS(ScriptEditorPreviewPlugin);
		GDREGISTER_INTERNAL_CLASS(HTMLPreviewEditorPlugin);
		GDREGISTER_INTERNAL_CLASS(MarkdownPreviewEditorPlugin);
		GDREGISTER_INTERNAL_CLASS(MermaidPreviewEditorPlugin);
		EditorPlugins::add_by_type<HTMLPreviewEditorPlugin>();
		EditorPlugins::add_by_type<MarkdownPreviewEditorPlugin>();
		EditorPlugins::add_by_type<MermaidPreviewEditorPlugin>();
		GDREGISTER_INTERNAL_CLASS(LatexDocumentImporter);
		GDREGISTER_INTERNAL_CLASS(HTMLDocumentImporter);
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
		EditorPlugins::remove_by_type<MermaidPreviewEditorPlugin>();
		EditorPlugins::remove_by_type<MarkdownPreviewEditorPlugin>();
		EditorPlugins::remove_by_type<HTMLPreviewEditorPlugin>();
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
