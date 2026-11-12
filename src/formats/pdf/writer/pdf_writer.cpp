#include "pdf_writer_internal.h"

#include "formats/pdf/reader/pdf_reader_internal.h"

#ifdef DOCRENDER_PDFIUM_ENABLED
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/char_string.hpp>

#include <atomic>
#include <cstring>
#endif

namespace godot {
	PDFWriter::PDFWriter() : state(std::make_unique<State>()) {}
	PDFWriter::~PDFWriter() = default;

	Error PDFWriter::State::fail(Error p_error, const String& p_message) {
		error = p_error;
		message = p_message;
		return error;
	}

	Error PDFWriter::State::unavailable() {
		return fail(ERR_UNAVAILABLE, "PDFium is unavailable in this build.");
	}

	void PDFWriter::State::clear_error() {
		error = OK;
		message = String();
	}

	bool PDFWriter::is_available() {
#ifdef DOCRENDER_PDFIUM_ENABLED
		String message;
		const auto api = pdfium::acquire(message);
		return api && api->has(pdfium::Group::writer_document) && api->has(pdfium::Group::writer_save);
#else
		return false;
#endif
	}

	PackedStringArray PDFWriter::get_capabilities() {
#ifdef DOCRENDER_PDFIUM_ENABLED
		String message;
		if (const auto api = pdfium::acquire(message)) {
			return api->capabilities(true);
		}
#endif
		return {};
	}

	Error PDFWriter::get_last_error() const {
		std::lock_guard guard(pdfium::mutex());
		return state->error;
	}

	String PDFWriter::get_last_error_message() const {
		std::lock_guard guard(pdfium::mutex());
		return state->message;
	}

	bool PDFWriter::is_open() const {
		std::lock_guard guard(pdfium::mutex());
#ifdef DOCRENDER_PDFIUM_ENABLED
		return state->document != nullptr;
#else
		return false;
#endif
	}

	void PDFWriter::close() {
		std::lock_guard guard(pdfium::mutex());
#ifdef DOCRENDER_PDFIUM_ENABLED
		state->reset();
#endif
		state->clear_error();
	}

	Error PDFWriter::open_file(const String& p_path, const String& p_password) {
#ifdef DOCRENDER_PDFIUM_ENABLED
		PackedByteArray bytes;
		String message;
		const Error error = pdfium::read_file(p_path, bytes, message);
		if (error != OK) {
			std::lock_guard guard(pdfium::mutex());
			return state->fail(error, message);
		}
		return open_bytes(std::move(bytes), p_password, p_path);
#else
		std::lock_guard guard(pdfium::mutex());
		return state->unavailable();
#endif
	}

	Error PDFWriter::open_buffer(const PackedByteArray& p_bytes, const String& p_password) {
		return open_bytes(p_bytes, p_password, "the PDF buffer");
	}

	Error PDFWriter::open_bytes(PackedByteArray p_bytes, const String& p_password, const String& p_source) {
		std::lock_guard guard(pdfium::mutex());
#ifdef DOCRENDER_PDFIUM_ENABLED
		String message;
		const auto api = pdfium::acquire(message);
		if (!api) {
			return state->fail(ERR_UNAVAILABLE, message);
		}
		Error error = OK;
		FPDF_DOCUMENT document = pdfium::load_document(*api, p_bytes, p_password, p_source, error, message);
		if (document == nullptr) {
			return state->fail(error, message);
		}
		state->replace(api, document, std::move(p_bytes));
		return OK;
#else
		return state->unavailable();
#endif
	}

#ifdef DOCRENDER_PDFIUM_ENABLED
	using namespace pdfium;

	namespace {
		int64_t next_handle_id() {
			static int64_t id = 0;
			// All callers hold the shared PDFium lock.
			return ++id;
		}

		const char* capability_name(Group p_group) {
			switch (p_group) {
#define PDFIUM_WRITER_GROUP_NAME(group, symbols) case Group::group: return #group + 7;
				PDFIUM_WRITER_GROUPS(PDFIUM_WRITER_GROUP_NAME)
#undef PDFIUM_WRITER_GROUP_NAME
				default: return "core";
			}
		}

		struct BufferSink : FPDF_FILEWRITE {
			PackedByteArray data;
			Error error = OK;

			BufferSink() : FPDF_FILEWRITE{ 1, write } {}

			static int write(FPDF_FILEWRITE* p_sink, const void* p_data, unsigned long p_size) {
				auto& sink = *static_cast<BufferSink*>(p_sink);
				const int64_t offset = sink.data.size();
				if (p_size == 0) {
					return 1;
				}
				if (offset > std::numeric_limits<int64_t>::max() - p_size) {
					sink.error = ERR_OUT_OF_MEMORY;
					return 0;
				}
				sink.error = static_cast<Error>(sink.data.resize(offset + p_size));
				if (sink.error != OK) {
					return 0;
				}
				std::memcpy(sink.data.ptrw() + offset, p_data, p_size);
				return 1;
			}
		};

		struct FileSink : FPDF_FILEWRITE {
			HANDLE file = INVALID_HANDLE_VALUE;
			String temporary;
			String destination;
			DWORD last_error = ERROR_SUCCESS;

			FileSink() : FPDF_FILEWRITE{ 1, write } {}

			~FileSink() {
				if (file != INVALID_HANDLE_VALUE) {
					CloseHandle(file);
				}
				if (!temporary.is_empty()) {
					DeleteFileW(temporary.wide_string().get_data());
				}
			}

			bool open(const String& p_path) {
				static std::atomic<uint64_t> serial{ 0 };
				destination = ProjectSettings::get_singleton()->globalize_path(p_path).replace("/", "\\");
				const String parent = destination.get_base_dir();
				for (int attempt = 0; attempt < 16; ++attempt) {
					const String name = vformat(".docrender-pdf-%d-%d.tmp", int64_t(GetCurrentProcessId()), ++serial);
					const String candidate = parent.path_join(name).replace("/", "\\");
					file = CreateFileW(candidate.wide_string().get_data(), GENERIC_WRITE, 0, nullptr,
						CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
					if (file != INVALID_HANDLE_VALUE) {
						temporary = candidate;
						return true;
					}
					last_error = GetLastError();
					if (last_error != ERROR_FILE_EXISTS && last_error != ERROR_ALREADY_EXISTS) {
						return false;
					}
				}
				return false;
			}

			bool commit() {
				if (!FlushFileBuffers(file)) {
					last_error = GetLastError();
					return false;
				}
				const bool closed = CloseHandle(file);
				file = INVALID_HANDLE_VALUE;
				if (!closed || !MoveFileExW(temporary.wide_string().get_data(), destination.wide_string().get_data(),
					MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
					last_error = GetLastError();
					return false;
				}
				temporary = String();
				return true;
			}

			static int write(FPDF_FILEWRITE* p_sink, const void* p_data, unsigned long p_size) {
				auto& sink = *static_cast<FileSink*>(p_sink);
				const auto* data = static_cast<const uint8_t*>(p_data);
				while (p_size > 0) {
					DWORD written = 0;
					if (!WriteFile(sink.file, data, p_size, &written, nullptr)) {
						sink.last_error = GetLastError();
						return 0;
					}
					if (written == 0) {
						sink.last_error = ERROR_WRITE_FAULT;
						return 0;
					}
					p_size -= written;
					data += written;
				}
				return 1;
			}
		};
	} // namespace

	PDFWriter::State::~State() {
		std::lock_guard guard(pdfium::mutex());
		reset();
	}

	void PDFWriter::State::reset() {
		close_pages();
		for (const auto& [id, object] : objects) {
			if (object.is_detached()) {
				api->FPDFPageObj_Destroy(object.handle);
			}
		}
		objects.clear();
		object_ids.clear();
		for (const auto& [id, font] : fonts) {
			api->FPDFFont_Close(font);
		}
		fonts.clear();
		if (document != nullptr) {
			api->FPDF_CloseDocument(document);
			document = nullptr;
		}
		input.clear();
		api.reset();
		uncertain = false;
	}

	void PDFWriter::State::replace(std::shared_ptr<Api> p_api, FPDF_DOCUMENT p_document, PackedByteArray p_input) {
		reset();
		api = std::move(p_api);
		document = p_document;
		input = std::move(p_input);
		clear_error();
	}

	bool PDFWriter::State::capability(Group p_group) {
		if (!api->has(p_group)) {
			fail(ERR_UNAVAILABLE, vformat("This PDFium build does not support PDFWriter capability '%s'.", capability_name(p_group)));
			return false;
		}
		return true;
	}

	bool PDFWriter::State::require(Group p_group) {
		if (document == nullptr) {
			String message;
			if (!pdfium::acquire(message)) {
				fail(ERR_UNAVAILABLE, message);
			} else {
				fail(ERR_UNCONFIGURED, "No PDF document is open.");
			}
			return false;
		}
		if (uncertain) {
			fail(ERR_INVALID_DATA, "The PDF page structure is uncertain after a native editing failure. Open a document again.");
			return false;
		}
		clear_error();
		return capability(p_group);
	}

	bool PDFWriter::State::invalid(const String& p_message) {
		fail(ERR_INVALID_PARAMETER, p_message);
		return false;
	}

	Error PDFWriter::State::checked(bool p_result, const String& p_message) {
		return p_result ? OK : fail(FAILED, p_message);
	}

	FPDF_DOCUMENT PDFWriter::State::source_document(const Ref<PDFReader>& p_reader) {
		if (p_reader.is_null() || p_reader->state->document == nullptr) {
			invalid("A PDFReader with an open source document is required.");
			return nullptr;
		}
		return p_reader->state->document;
	}

	int PDFWriter::State::page_count() const {
		return api->FPDF_GetPageCount(document);
	}

	bool PDFWriter::State::has_page(int64_t p_page) {
		return (p_page >= 0 && p_page < page_count()) || invalid("PDF page index is out of bounds.");
	}

	bool PDFWriter::State::resolve_insert(int64_t& r_at) {
		const int count = page_count();
		if (r_at == -1) {
			r_at = count;
		}
		return (r_at >= 0 && r_at <= count) || invalid("PDF page insertion index is out of bounds.");
	}

	FPDF_PAGE PDFWriter::State::page(int64_t p_index) {
		if (!has_page(p_index)) {
			return nullptr;
		}
		const int index = int(p_index);
		if (const auto it = pages.find(index); it != pages.end()) {
			return it->second.handle;
		}
		FPDF_PAGE handle = api->FPDF_LoadPage(document, index);
		if (handle != nullptr) {
			pages.emplace(index, EditablePage{ handle });
		} else {
			fail(ERR_FILE_CORRUPT, vformat("Could not load PDF page %d.", index));
		}
		return handle;
	}

	void PDFWriter::State::dirty(int p_page) {
		if (p_page >= 0) {
			pages.at(p_page).dirty = true;
		}
	}

	bool PDFWriter::State::flush_page(EditablePage& p_page) {
		if (p_page.dirty && !api->FPDFPage_GenerateContent(p_page.handle)) {
			fail(ERR_CANT_CREATE, "Could not generate the edited PDF page content.");
			return false;
		}
		p_page.dirty = false;
		return true;
	}

	bool PDFWriter::State::flush_pages() {
		for (auto& [index, page] : pages) {
			if (!flush_page(page)) {
				return false;
			}
		}
		close_pages();
		return true;
	}

	bool PDFWriter::State::flush_and_close_page(int p_page) {
		if (const auto it = pages.find(p_page); it != pages.end() && !flush_page(it->second)) {
			return false;
		}
		close_page(p_page);
		return true;
	}

	void PDFWriter::State::close_pages() {
		for (auto it = objects.begin(); it != objects.end();) {
			if (it->second.page >= 0) {
				object_ids.erase(it->second.handle);
				it = objects.erase(it);
			} else {
				++it;
			}
		}
		for (const auto& [id, annotation] : annotations) {
			api->FPDFPage_CloseAnnot(annotation.handle);
		}
		annotations.clear();
		for (const auto& [index, page] : pages) {
			api->FPDF_ClosePage(page.handle);
		}
		pages.clear();
	}

	void PDFWriter::State::close_page(int p_page) {
		const auto page = pages.find(p_page);
		if (page == pages.end()) {
			return;
		}
		for (auto it = objects.begin(); it != objects.end();) {
			if (it->second.page == p_page) {
				object_ids.erase(it->second.handle);
				it = objects.erase(it);
			} else {
				++it;
			}
		}
		for (auto it = annotations.begin(); it != annotations.end();) {
			if (it->second.page == p_page) {
				api->FPDFPage_CloseAnnot(it->second.handle);
				it = annotations.erase(it);
			} else {
				++it;
			}
		}
		api->FPDF_ClosePage(page->second.handle);
		pages.erase(page);
	}

	int64_t PDFWriter::State::remember_object(FPDF_PAGEOBJECT p_handle, int p_page, int64_t p_parent, int64_t p_annotation) {
		if (p_handle == nullptr) {
			fail(ERR_CANT_ACQUIRE_RESOURCE, "Could not acquire a PDF page object.");
			return 0;
		}
		if (const auto it = object_ids.find(p_handle); it != object_ids.end()) {
			return it->second;
		}
		const int64_t id = next_handle_id();
		objects.emplace(id, Object{ p_handle, p_page, p_parent, p_annotation });
		object_ids.emplace(p_handle, id);
		return id;
	}

	PDFWriter::State::Object* PDFWriter::State::object(int64_t p_id, int p_type) {
		const auto it = objects.find(p_id);
		if (it == objects.end() || (p_type != 0 && api->FPDFPageObj_GetType(it->second.handle) != p_type)) {
			invalid("The PDF object ID is invalid, expired, or has the wrong type.");
			return nullptr;
		}
		return &it->second;
	}

	PDFWriter::State::Object* PDFWriter::State::editable_object(int64_t p_id, int p_type) {
		Object* result = object(p_id, p_type);
		// PDFium can regenerate a Form's stream after removing a direct child.
		// Object setters alone do not mark that stream for regeneration.
		if (result != nullptr && result->parent != 0) {
			fail(ERR_UNAVAILABLE, "PDFium cannot save in-place edits to a Form child. Detach the object before editing it.");
			return nullptr;
		}
		return result;
	}

	void PDFWriter::State::erase_object(int64_t p_id) {
		if (const auto it = objects.find(p_id); it != objects.end()) {
			object_ids.erase(it->second.handle);
			objects.erase(it);
		}
	}

	void PDFWriter::State::erase_descendants(int64_t p_id) {
		std::vector<int64_t> descendants = { p_id };
		for (size_t i = 0; i < descendants.size(); ++i) {
			for (const auto& [id, object] : objects) {
				if (object.parent == descendants[i]) {
					descendants.push_back(id);
				}
			}
		}
		for (size_t i = 1; i < descendants.size(); ++i) {
			erase_object(descendants[i]);
		}
	}

	Error PDFWriter::State::finish_object(Object& p_object, bool p_result) {
		if (!p_result) {
			return fail(FAILED, "PDFium could not edit the PDF object.");
		}
		if (p_object.annotation != 0) {
			return checked(api->FPDFAnnot_UpdateObject(annotations.at(p_object.annotation).handle, p_object.handle),
				"Could not update the PDF annotation appearance.");
		}
		dirty(p_object.page);
		return OK;
	}

	FPDF_PAGEOBJECTMARK PDFWriter::State::mark(Object& p_object, int64_t p_index) {
		if (p_index < 0 || p_index >= api->FPDFPageObj_CountMarks(p_object.handle)) {
			invalid("PDF content mark index is out of bounds.");
			return nullptr;
		}
		const auto handle = api->FPDFPageObj_GetMark(p_object.handle, unsigned(p_index));
		if (handle == nullptr) {
			fail(ERR_CANT_ACQUIRE_RESOURCE, "Could not read the PDF content mark.");
		}
		return handle;
	}

	int64_t PDFWriter::State::remember_font(FPDF_FONT p_font) {
		if (p_font == nullptr) {
			fail(ERR_CANT_CREATE, "Could not load the PDF font.");
			return 0;
		}
		const int64_t id = next_handle_id();
		fonts.emplace(id, p_font);
		return id;
	}

	int64_t PDFWriter::State::remember_annotation(FPDF_ANNOTATION p_handle, int p_page) {
		if (p_handle == nullptr) {
			fail(ERR_CANT_ACQUIRE_RESOURCE, "Could not acquire a PDF annotation.");
			return 0;
		}
		const int64_t id = next_handle_id();
		annotations.emplace(id, Annotation{ p_handle, p_page });
		return id;
	}

	PDFWriter::State::Annotation* PDFWriter::State::annotation(int64_t p_id) {
		const auto it = annotations.find(p_id);
		if (it == annotations.end()) {
			invalid("The PDF annotation ID is invalid or expired.");
			return nullptr;
		}
		return &it->second;
	}

	void PDFWriter::State::forget_annotation_objects(int64_t p_id) {
		for (auto it = objects.begin(); it != objects.end();) {
			if (it->second.annotation == p_id) {
				object_ids.erase(it->second.handle);
				it = objects.erase(it);
			} else {
				++it;
			}
		}
	}

	FPDF_ATTACHMENT PDFWriter::State::attachment(int64_t p_index) {
		if (p_index < 0 || p_index >= api->FPDFDoc_GetAttachmentCount(document)) {
			invalid("PDF attachment index is out of bounds.");
			return nullptr;
		}
		FPDF_ATTACHMENT attachment = api->FPDFDoc_GetAttachment(document, int(p_index));
		if (attachment == nullptr) {
			fail(ERR_FILE_CORRUPT, "Could not read the PDF attachment.");
		}
		return attachment;
	}

	Error PDFWriter::create_document() {
		std::lock_guard guard(pdfium::mutex());
		String message;
		const auto api = pdfium::acquire(message);
		if (!api) {
			return state->fail(ERR_UNAVAILABLE, message);
		}
		if (!api->has(Group::writer_document)) {
			return state->fail(ERR_UNAVAILABLE, "This PDFium build cannot create documents.");
		}
		FPDF_DOCUMENT document = api->FPDF_CreateNewDocument();
		if (document == nullptr) {
			return state->fail(ERR_CANT_CREATE, "Could not create a PDF document.");
		}
		state->replace(api, document);
		return OK;
	}

	bool PDFWriter::State::prepare_save(int64_t p_flags, int64_t p_version) {
		constexpr int64_t flags = SAVE_INCREMENTAL | SAVE_REMOVE_SECURITY | SAVE_SUBSET_NEW_FONTS;
		if (p_flags < 0 || (p_flags & ~flags) || (p_version != 0 && (p_version < 10 || p_version > 17))) {
			return invalid("Invalid PDF save flags or PDF version.");
		}
		if ((p_flags & SAVE_INCREMENTAL) && (input.is_empty() || p_version != 0 || (p_flags & SAVE_REMOVE_SECURITY))) {
			return invalid("Incremental PDF saving requires an opened PDF and its original version and encryption.");
		}
		return flush_pages();
	}

	bool PDFWriter::State::save(FPDF_FILEWRITE& p_sink, int64_t p_flags, int64_t p_version) {
		const auto flags = static_cast<FPDF_DWORD>(p_flags);
		return p_version == 0 ? api->FPDF_SaveAsCopy(document, &p_sink, flags) :
			api->FPDF_SaveWithVersion(document, &p_sink, flags, int(p_version));
	}

	PackedByteArray PDFWriter::save_to_buffer(int64_t p_flags, int64_t p_version) {
		PDF_WRITE_OR_RETURN(save, {});
		if (!state->prepare_save(p_flags, p_version)) {
			return {};
		}
		BufferSink sink;
		if (!state->save(sink, p_flags, p_version) || sink.error != OK || sink.data.is_empty()) {
			state->fail(sink.error != OK ? sink.error : ERR_CANT_CREATE, "Could not save the PDF to a buffer.");
			return {};
		}
		return sink.data;
	}

	Error PDFWriter::save_to_file(const String& p_path, int64_t p_flags, int64_t p_version) {
		PDF_WRITE_OR_RETURN(save, state->error);
		if (p_path.is_empty() || p_path.ends_with("/") || p_path.ends_with("\\")) {
			return state->fail(ERR_INVALID_PARAMETER, "A PDF output file path is required.");
		}
		FileSink sink;
		if (!sink.open(p_path)) {
			return state->fail(ERR_CANT_CREATE, vformat("Could not create a temporary PDF beside %s (Windows error %d).",
				p_path, int64_t(sink.last_error)));
		}
		if (!state->prepare_save(p_flags, p_version)) {
			return state->error;
		}
		if (!state->save(sink, p_flags, p_version) || sink.last_error != ERROR_SUCCESS || !sink.commit()) {
			return state->fail(ERR_FILE_CANT_WRITE, vformat("Could not save %s (Windows error %d).",
				p_path, int64_t(sink.last_error)));
		}
		return OK;
	}
#endif
} // namespace godot
