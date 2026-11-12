#include "docrender_source.h"

#include <godot_cpp/classes/file_access.hpp>

using namespace godot;

Error docrender::read_document_text(const String& p_path, String& r_text) {
	const Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		return FileAccess::get_open_error();
	}

	const uint64_t length = file->get_length();
	const PackedByteArray bytes = file->get_buffer(length);
	ERR_FAIL_COND_V_MSG(uint64_t(bytes.size()) != length, ERR_FILE_CANT_READ, "Could not read " + p_path);
	String text;
	if (!bytes.is_empty()) {
		const bool invalid = bytes.has(0) || text.parse_utf8(reinterpret_cast<const char*>(bytes.ptr()), bytes.size()) != OK;
		ERR_FAIL_COND_V_MSG(invalid, ERR_FILE_CORRUPT, "Expected UTF-8 text in " + p_path);
	}

	r_text = text;
	return OK;
}
