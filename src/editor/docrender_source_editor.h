#pragma once

#include "latex_document_importer.h"
#include "mermaid_document_importer.h"

#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/signal.hpp>

namespace godot {
	class ScriptEditorBase;

	class DocRenderSourceEditorPlugin : public EditorPlugin {
		GDCLASS(DocRenderSourceEditorPlugin, EditorPlugin)

	public:
		static void prepare_registration();
		void _enter_tree() override;
		void _exit_tree() override;
		void _set_window_layout(const Ref<ConfigFile>& p_configuration) override;

	protected:
		static void _bind_methods() {}

	private:
		struct NativeConnection {
			Signal signal;
			Callable callback;
		};

		Ref<LatexDocumentImporter> latex_importer;
		Ref<MermaidDocumentImporter> mermaid_importer;
		NativeConnection tree_activation;
		NativeConnection list_activation;
		NativeConnection file_selection;
		Callable open_text_result;
		uint64_t script_list_id = 0;
		uint64_t script_tabs_id = 0;
		HashMap<uint64_t, String> source_tabs;

		void install_importers();
		void import_existing_sources();
		void attach_source_editor();
		void detach_source_editor();
		void activate_tree_item();
		void activate_list_item(int32_t p_index);
		bool open_imported_source(const Variant& p_metadata);
		void open_source_text(const String& p_path);
		void sync_open_sources();
		void restore_open_source_paths(const Array& p_paths);
		void remember_source(ScriptEditorBase* p_tab, const String& p_path);
		void forget_source(uint64_t p_tab_id);
		void select_file(const String& p_path);
	};
} // namespace godot
