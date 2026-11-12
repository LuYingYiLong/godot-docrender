#include "latex_document_loader.h"

#include "formats/docrender_source.h"
#include "latex_document.h"

#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_uid.hpp>

using namespace godot;

namespace {
	// Godot's binary resource loader also recognizes .tex. Prefer DocRender's
	// imported artifact only when the sidecar identifies our importer.
	Ref<ConfigFile> get_tex_import(const String& p_path) {
		if (p_path.get_extension().nocasecmp_to("tex") != 0) {
			return {};
		}
		const String sidecar = p_path + String(".import");
		if (!FileAccess::file_exists(sidecar)) {
			return {};
		}

		Ref<ConfigFile> config;
		config.instantiate();
		if (config->load(sidecar) != OK) {
			return {};
		}
		const String importer = config->get_value("remap", "importer", "");
		if (importer != "docrender.latex_document" && importer != "docrender.source") {
			return {};
		}
		return config;
	}

	bool is_tex_source(const String& p_path) {
		if (p_path.get_extension().nocasecmp_to("tex") != 0) {
			return false;
		}
		if (FileAccess::file_exists(p_path + String(".import"))) {
			return get_tex_import(p_path).is_valid();
		}
		const Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
		if (file.is_null()) {
			return false;
		}

		// Let Godot handle native binary resources that use the same extension.
		const PackedByteArray header = file->get_buffer(4);
		return header.size() != 4 || header[0] != 'R' || header[1] != 'S' ||
			!((header[2] == 'R' || header[2] == 'C') && header[3] == 'C');
	}
} // namespace

bool DocRenderTexLoader::_recognize_path(const String& p_path, const StringName& p_type) const {
	return (p_type.is_empty() || p_type == Resource::get_class_static() || p_type == LatexDocument::get_class_static()) &&
		is_tex_source(p_path);
}

String DocRenderTexLoader::_get_resource_type(const String& p_path) const {
	return is_tex_source(p_path) ? String(LatexDocument::get_class_static()) : String();
}

int64_t DocRenderTexLoader::_get_resource_uid(const String& p_path) const {
	const Ref<ConfigFile> config = get_tex_import(p_path);
	if (config.is_null()) {
		return ResourceUID::INVALID_ID;
	}
	return ResourceUID::get_singleton()->text_to_id(config->get_value("remap", "uid", ""));
}

Variant DocRenderTexLoader::_load(const String& p_path, const String&, bool, int32_t) const {
	const Ref<ConfigFile> config = get_tex_import(p_path);
	if (config.is_valid() && StringName(config->get_value("remap", "type", "")) == LatexDocument::get_class_static()) {
		const String artifact = config->get_value("remap", "path", "");
		if (artifact.get_extension() == "res" && FileAccess::file_exists(artifact)) {
			// The outer ResourceLoader call owns caching under the original .tex path.
			const Ref<LatexDocument> document = ResourceLoader::get_singleton()->load(artifact,
				String(LatexDocument::get_class_static()), ResourceLoader::CACHE_MODE_IGNORE);
			return document.is_valid() ? Variant(document) : Variant(ERR_FILE_CANT_READ);
		}
	}

	String text;
	const Error error = docrender::read_document_text(p_path, text);
	if (error != OK) {
		return error;
	}
	Ref<LatexDocument> document;
	document.instantiate();
	document->set_text(text);
	return document;
}
