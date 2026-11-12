#include "pdf_reader_internal.h"

#ifdef DOCRENDER_PDFIUM_ENABLED
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>

#include <cmath>
#include <unordered_set>
#include <vector>
#endif

namespace godot {
#ifdef DOCRENDER_PDFIUM_ENABLED
	using namespace pdfium;

	namespace {
		template <typename Length = unsigned long, typename Read>
		PackedByteArray read_sized(Read p_read) {
			return read_buffer([&](uint8_t* p_buffer, Length p_length) {
				Length size = 0;
				return p_read(p_buffer, p_length, &size) ? size : Length(0);
			});
		}

		template <typename Read>
		PackedVector2Array read_points(Read p_read) {
			const PackedByteArray buffer = read_buffer<FS_POINTF>(p_read);
			const auto* points = reinterpret_cast<const FS_POINTF*>(buffer.ptr());
			PackedVector2Array result;
			for (int64_t i = 0; i < buffer.size() / int64_t(sizeof(FS_POINTF)); ++i) {
				result.append(Vector2(points[i].x, points[i].y));
			}
			return result;
		}

		String annotation_string(const Api& p_api, FPDF_ANNOTATION p_annotation, const char* p_key) {
			return utf16(read_buffer([&](uint8_t* p_buffer, unsigned long p_length) {
				return p_api.FPDFAnnot_GetStringValue(p_annotation, p_key, reinterpret_cast<FPDF_WCHAR*>(p_buffer), p_length);
			}));
		}
	}

	TypedArray<Dictionary> PDFReader::get_page_annotations(int64_t p_page) {
		PDF_QUERY_OR_RETURN(annotations, {});
		PDF_PAGE_OR_RETURN({})
		TypedArray<Dictionary> result;
		const int count = api.FPDFPage_GetAnnotCount(page.get());
		for (int i = 0; i < count; ++i) {
			const auto annotation = scoped(api.FPDFPage_GetAnnot(page.get(), i), api.FPDFPage_CloseAnnot);
			if (!annotation) {
				continue;
			}
			const auto handle = annotation.get();
			Dictionary row;
			row["index"] = i;
			row["subtype"] = api.FPDFAnnot_GetSubtype(handle);
			row["flags"] = api.FPDFAnnot_GetFlags(handle);
			row["author"] = annotation_string(api, handle, "T");
			row["contents"] = annotation_string(api, handle, "Contents");
			row["subject"] = annotation_string(api, handle, "Subj");
			row["modified"] = annotation_string(api, handle, "M");
			FS_RECTF bounds;
			if (api.FPDFAnnot_GetRect(handle, &bounds)) {
				row["rect"] = rect(bounds);
			}
			unsigned int r, g, b, a;
			if (api.FPDFAnnot_GetColor(handle, FPDFANNOT_COLORTYPE_Color, &r, &g, &b, &a)) {
				row["color"] = Color::from_rgba8(r, g, b, a);
			}
			if (api.FPDFAnnot_GetColor(handle, FPDFANNOT_COLORTYPE_InteriorColor, &r, &g, &b, &a)) {
				row["interior_color"] = Color::from_rgba8(r, g, b, a);
			}
			TypedArray<PackedVector2Array> quads;
			const size_t quad_count = api.FPDFAnnot_CountAttachmentPoints(handle);
			for (size_t j = 0; j < quad_count; ++j) {
				FS_QUADPOINTSF points;
				if (api.FPDFAnnot_GetAttachmentPoints(handle, j, &points)) {
					quads.append(quad(points));
				}
			}
			row["quads"] = quads;
			row["vertices"] = read_points([&](FS_POINTF* p_buffer, unsigned long p_length) {
				return api.FPDFAnnot_GetVertices(handle, p_buffer, p_length);
			});
			TypedArray<PackedVector2Array> ink_paths;
			const unsigned long path_count = api.FPDFAnnot_GetInkListCount(handle);
			for (unsigned long j = 0; j < path_count; ++j) {
				ink_paths.append(read_points([&](FS_POINTF* p_buffer, unsigned long p_length) {
					return api.FPDFAnnot_GetInkListPath(handle, j, p_buffer, p_length);
				}));
			}
			row["ink_paths"] = ink_paths;
			FS_POINTF start, end;
			if (api.FPDFAnnot_GetLine(handle, &start, &end)) {
				PackedVector2Array line;
				line.append(Vector2(start.x, start.y));
				line.append(Vector2(end.x, end.y));
				row["line"] = line;
			}
			float horizontal, vertical, width;
			if (api.FPDFAnnot_GetBorder(handle, &horizontal, &vertical, &width)) {
				row["border"] = Vector3(horizontal, vertical, width);
			}
			result.append(row);
		}
		return result;
	}

	String PDFReader::get_annotation_string(int64_t p_page, int64_t p_index, const String& p_key) {
		PDF_QUERY_OR_RETURN(annotations, {});
		PDF_PAGE_OR_RETURN({})
		if (p_index < 0 || p_index >= api.FPDFPage_GetAnnotCount(page.get()) || p_key.is_empty()) {
			state->fail(ERR_INVALID_PARAMETER, "Invalid PDF annotation index or key.");
			return {};
		}
		const auto annotation = scoped(api.FPDFPage_GetAnnot(page.get(), int(p_index)), api.FPDFPage_CloseAnnot);
		if (!annotation) {
			state->fail(ERR_FILE_CORRUPT, "Could not read the PDF annotation.");
			return {};
		}
		const CharString key = p_key.utf8();
		return annotation_string(api, annotation.get(), key.get_data());
	}

	TypedArray<Dictionary> PDFReader::get_form_fields(int64_t p_page) {
		PDF_QUERY_OR_RETURN(annotations, {});
		PDF_PAGE_OR_RETURN({})
		if (!state->ensure_form()) {
			return {};
		}
		const State::FormPage form_page(*state, page.get(), int(p_page));
		const FPDF_FORMHANDLE form = state->form;
		TypedArray<Dictionary> result;
		const int count = api.FPDFPage_GetAnnotCount(page.get());
		for (int i = 0; i < count; ++i) {
			const auto annotation = scoped(api.FPDFPage_GetAnnot(page.get(), i), api.FPDFPage_CloseAnnot);
			if (!annotation || api.FPDFAnnot_GetSubtype(annotation.get()) != FPDF_ANNOT_WIDGET) {
				continue;
			}
			const auto handle = annotation.get();
			const auto read_field = [&](auto p_read) {
				return utf16(read_buffer([&](uint8_t* p_buffer, unsigned long p_length) {
					return p_read(form, handle, reinterpret_cast<FPDF_WCHAR*>(p_buffer), p_length);
				}));
			};
			Dictionary row;
			row["annotation_index"] = i;
			row["name"] = read_field(api.FPDFAnnot_GetFormFieldName);
			row["alternate_name"] = read_field(api.FPDFAnnot_GetFormFieldAlternateName);
			row["value"] = read_field(api.FPDFAnnot_GetFormFieldValue);
			row["export_value"] = read_field(api.FPDFAnnot_GetFormFieldExportValue);
			row["type"] = api.FPDFAnnot_GetFormFieldType(form, handle);
			row["flags"] = api.FPDFAnnot_GetFormFieldFlags(form, handle);
			row["checked"] = bool(api.FPDFAnnot_IsChecked(form, handle));
			FS_RECTF bounds;
			if (api.FPDFAnnot_GetRect(handle, &bounds)) {
				row["rect"] = rect(bounds);
			}
			TypedArray<Dictionary> options;
			const int option_count = api.FPDFAnnot_GetOptionCount(form, handle);
			for (int j = 0; j < option_count; ++j) {
				Dictionary option;
				option["index"] = j;
				option["label"] = utf16(read_buffer([&](uint8_t* p_buffer, unsigned long p_length) {
					return api.FPDFAnnot_GetOptionLabel(form, handle, j, reinterpret_cast<FPDF_WCHAR*>(p_buffer), p_length);
				}));
				option["selected"] = bool(api.FPDFAnnot_IsOptionSelected(form, handle, j));
				options.append(option);
			}
			row["options"] = options;
			result.append(row);
		}
		return result;
	}

	TypedArray<Dictionary> PDFReader::get_attachments() {
		PDF_QUERY_OR_RETURN(attachments, {});
		TypedArray<Dictionary> result;
		const int count = api.FPDFDoc_GetAttachmentCount(state->document);
		for (int i = 0; i < count; ++i) {
			const FPDF_ATTACHMENT attachment = api.FPDFDoc_GetAttachment(state->document, i);
			if (attachment == nullptr) {
				continue;
			}
			const auto read = [&](auto p_read) {
				return utf16(read_buffer([&](uint8_t* p_buffer, unsigned long p_length) {
					return p_read(attachment, reinterpret_cast<FPDF_WCHAR*>(p_buffer), p_length);
				}));
			};
			Dictionary row;
			row["index"] = i;
			row["name"] = read(api.FPDFAttachment_GetName);
			row["description"] = read(api.FPDFAttachment_GetDescription);
			row["mime_type"] = read(api.FPDFAttachment_GetSubtype);
			row["relationship"] = read(api.FPDFAttachment_GetAFRelationship);
			constexpr const char* keys[] = { "CreationDate", "ModDate", "CheckSum" };
			for (const char* key : keys) {
				const String value = utf16(read_buffer([&](uint8_t* p_buffer, unsigned long p_length) {
					return api.FPDFAttachment_GetStringValue(attachment, key, reinterpret_cast<FPDF_WCHAR*>(p_buffer), p_length);
				}));
				if (!value.is_empty()) {
					row[key] = value;
				}
			}
			unsigned long size = 0;
			if (api.FPDFAttachment_GetFile(attachment, nullptr, 0, &size)) {
				row["size"] = int64_t(size);
			}
			result.append(row);
		}
		return result;
	}

	PackedByteArray PDFReader::get_attachment_data(int64_t p_index) {
		PDF_QUERY_OR_RETURN(attachments, {});
		if (p_index < 0 || p_index >= api.FPDFDoc_GetAttachmentCount(state->document)) {
			state->fail(ERR_INVALID_PARAMETER, "PDF attachment index is out of bounds.");
			return {};
		}
		const FPDF_ATTACHMENT attachment = api.FPDFDoc_GetAttachment(state->document, int(p_index));
		if (attachment == nullptr) {
			state->fail(ERR_FILE_CORRUPT, "Could not read the PDF attachment.");
			return {};
		}
		unsigned long size = 0;
		if (!api.FPDFAttachment_GetFile(attachment, nullptr, 0, &size)) {
			state->fail(ERR_FILE_CANT_READ, "Could not read the PDF attachment stream.");
			return {};
		}
		auto data = read_sized([&](uint8_t* p_buffer, unsigned long p_length, unsigned long* r_size) {
			return api.FPDFAttachment_GetFile(attachment, p_buffer, p_length, r_size);
		});
		if (uint64_t(data.size()) != size) {
			state->fail(ERR_FILE_CANT_READ, "Could not read the complete PDF attachment.");
		}
		return data;
	}

	TypedArray<Dictionary> PDFReader::get_signatures() {
		PDF_QUERY_OR_RETURN(signatures, {});
		TypedArray<Dictionary> result;
		const int count = api.FPDF_GetSignatureCount(state->document);
		for (int i = 0; i < count; ++i) {
			const FPDF_SIGNATURE signature = api.FPDF_GetSignatureObject(state->document, i);
			if (signature == nullptr) {
				continue;
			}
			Dictionary row;
			row["index"] = i;
			row["subfilter"] = utf8(read_buffer<char>([&](char* p_buffer, unsigned long p_length) {
				return api.FPDFSignatureObj_GetSubFilter(signature, p_buffer, p_length);
			}));
			row["time"] = utf8(read_buffer<char>([&](char* p_buffer, unsigned long p_length) {
				return api.FPDFSignatureObj_GetTime(signature, p_buffer, p_length);
			}));
			row["reason"] = utf16(read_buffer([&](void* p_buffer, unsigned long p_length) {
				return api.FPDFSignatureObj_GetReason(signature, p_buffer, p_length);
			}));
			row["doc_mdp_permission"] = int64_t(api.FPDFSignatureObj_GetDocMDPPermission(signature));
			const auto bytes = read_buffer<int>([&](int* p_buffer, unsigned long p_length) {
				return api.FPDFSignatureObj_GetByteRange(signature, p_buffer, p_length);
			});
			PackedInt32Array range;
			const auto* values = reinterpret_cast<const int*>(bytes.ptr());
			for (int64_t j = 0; j < bytes.size() / int64_t(sizeof(int)); ++j) {
				range.append(values[j]);
			}
			row["byte_range"] = range;
			result.append(row);
		}
		return result;
	}

	PackedByteArray PDFReader::get_signature_contents(int64_t p_index) {
		PDF_QUERY_OR_RETURN(signatures, {});
		if (p_index < 0 || p_index >= api.FPDF_GetSignatureCount(state->document)) {
			state->fail(ERR_INVALID_PARAMETER, "PDF signature index is out of bounds.");
			return {};
		}
		const FPDF_SIGNATURE signature = api.FPDF_GetSignatureObject(state->document, int(p_index));
		if (signature == nullptr) {
			state->fail(ERR_FILE_CORRUPT, "Could not read the PDF signature.");
			return {};
		}
		return read_buffer([&](void* p_buffer, unsigned long p_length) {
			return api.FPDFSignatureObj_GetContents(signature, p_buffer, p_length);
		});
	}

	TypedArray<Dictionary> PDFReader::get_javascript_actions() {
		PDF_QUERY_OR_RETURN(javascript, {});
		TypedArray<Dictionary> result;
		const int count = api.FPDFDoc_GetJavaScriptActionCount(state->document);
		for (int i = 0; i < count; ++i) {
			const auto handle = scoped(api.FPDFDoc_GetJavaScriptAction(state->document, i), api.FPDFDoc_CloseJavaScriptAction);
			if (!handle) {
				continue;
			}
			Dictionary row;
			row["index"] = i;
			row["name"] = utf16(read_buffer([&](uint8_t* p_buffer, unsigned long p_length) {
				return api.FPDFJavaScriptAction_GetName(handle.get(), reinterpret_cast<FPDF_WCHAR*>(p_buffer), p_length);
			}));
			row["script"] = utf16(read_buffer([&](uint8_t* p_buffer, unsigned long p_length) {
				return api.FPDFJavaScriptAction_GetScript(handle.get(), reinterpret_cast<FPDF_WCHAR*>(p_buffer), p_length);
			}));
			result.append(row);
		}
		return result;
	}

	TypedArray<Dictionary> PDFReader::get_xfa_packets() {
		PDF_QUERY_OR_RETURN(xfa, {});
		TypedArray<Dictionary> result;
		const int count = api.FPDF_GetXFAPacketCount(state->document);
		for (int i = 0; i < count; ++i) {
			Dictionary row;
			row["index"] = i;
			row["name"] = utf8(read_buffer([&](void* p_buffer, unsigned long p_length) {
				return api.FPDF_GetXFAPacketName(state->document, i, p_buffer, p_length);
			}));
			row["data"] = read_sized([&](void* p_buffer, unsigned long p_length, unsigned long* r_length) {
				return api.FPDF_GetXFAPacketContent(state->document, i, p_buffer, p_length, r_length);
			});
			result.append(row);
		}
		return result;
	}

	namespace {
		Variant structure_attribute(const Api& p_api, FPDF_STRUCTELEMENT_ATTR_VALUE p_value) {
			// Attribute arrays may contain other arrays. Keep the traversal local
			// to this snapshot, and cut cycles in malformed documents.
			Variant result;
			struct Pending {
				FPDF_STRUCTELEMENT_ATTR_VALUE value;
				Array parent;
				int index;
			};
			std::vector<Pending> pending = { { p_value, Array(), -1 } };
			std::unordered_set<FPDF_STRUCTELEMENT_ATTR_VALUE> arrays;
			while (!pending.empty()) {
				const Pending item = pending.back();
				pending.pop_back();
				Variant value;
				if (item.value != nullptr) {
					switch (p_api.FPDF_StructElement_Attr_GetType(item.value)) {
						case FPDF_OBJECT_BOOLEAN: {
							FPDF_BOOL boolean = false;
							if (p_api.FPDF_StructElement_Attr_GetBooleanValue(item.value, &boolean)) {
								value = bool(boolean);
							}
						} break;
						case FPDF_OBJECT_NUMBER: {
							float number = 0.0f;
							if (p_api.FPDF_StructElement_Attr_GetNumberValue(item.value, &number)) {
								value = number;
							}
						} break;
						case FPDF_OBJECT_STRING:
						case FPDF_OBJECT_NAME:
							value = utf16(read_sized([&](void* p_buffer, unsigned long p_length, unsigned long* r_size) {
								return p_api.FPDF_StructElement_Attr_GetStringValue(item.value, p_buffer, p_length, r_size);
							}));
							break;
						case FPDF_OBJECT_STREAM:
							value = read_sized([&](void* p_buffer, unsigned long p_length, unsigned long* r_size) {
								return p_api.FPDF_StructElement_Attr_GetBlobValue(item.value, p_buffer, p_length, r_size);
							});
							break;
						case FPDF_OBJECT_ARRAY: {
							const int count = p_api.FPDF_StructElement_Attr_CountChildren(item.value);
							Array children;
							if (count > 0 && arrays.insert(item.value).second && children.resize(count) == OK) {
								for (int i = count - 1; i >= 0; --i) {
									pending.push_back({ p_api.FPDF_StructElement_Attr_GetChildAtIndex(item.value, i), children, i });
								}
							}
							value = children;
						} break;
						default: break;
					}
				}
				if (item.index < 0) {
					result = value;
				} else {
					Array parent = item.parent;
					parent[item.index] = value;
				}
			}
			return result;
		}

		Dictionary describe_structure(const Api& p_api, FPDF_STRUCTELEMENT p_element) {
			const auto read = [&](auto p_read) {
				return utf16(read_buffer([&](void* p_buffer, unsigned long p_length) {
					return p_read(p_element, p_buffer, p_length);
				}));
			};
			Dictionary result;
			result["type"] = read(p_api.FPDF_StructElement_GetType);
			result["title"] = read(p_api.FPDF_StructElement_GetTitle);
			result["alt_text"] = read(p_api.FPDF_StructElement_GetAltText);
			result["actual_text"] = read(p_api.FPDF_StructElement_GetActualText);
			result["language"] = read(p_api.FPDF_StructElement_GetLang);
			result["id"] = read(p_api.FPDF_StructElement_GetID);
			result["expansion"] = read(p_api.FPDF_StructElement_GetExpansion);
			result["marked_content_id"] = p_api.FPDF_StructElement_GetMarkedContentID(p_element);
			PackedInt32Array ids;
			const int id_count = p_api.FPDF_StructElement_GetMarkedContentIdCount(p_element);
			for (int i = 0; i < id_count; ++i) {
				const int id = p_api.FPDF_StructElement_GetMarkedContentIdAtIndex(p_element, i);
				if (id >= 0) {
					ids.append(id);
				}
			}
			result["marked_content_ids"] = ids;

			TypedArray<Dictionary> attributes;
			const int attribute_count = p_api.FPDF_StructElement_GetAttributeCount(p_element);
			for (int i = 0; i < attribute_count; ++i) {
				const auto map = p_api.FPDF_StructElement_GetAttributeAtIndex(p_element, i);
				if (map == nullptr) {
					continue;
				}
				Dictionary entries;
				const int entry_count = p_api.FPDF_StructElement_Attr_GetCount(map);
				for (int j = 0; j < entry_count; ++j) {
					const auto key = read_sized([&](void* p_buffer, unsigned long p_length, unsigned long* r_size) {
						return p_api.FPDF_StructElement_Attr_GetName(map, j, p_buffer, p_length, r_size);
					});
					const String name = utf8(key);
					if (!name.is_empty()) {
						const CharString native_name = name.utf8();
						const auto handle = p_api.FPDF_StructElement_Attr_GetValue(map, native_name.get_data());
						if (handle != nullptr) {
							Dictionary entry;
							entry["type"] = p_api.FPDF_StructElement_Attr_GetType(handle);
							const Variant value = structure_attribute(p_api, handle);
							if (value.get_type() != Variant::NIL) {
								entry["value"] = value;
							}
							entries[name] = entry;
						}
					}
				}
				attributes.append(entries);
			}
			result["attributes"] = attributes;
			return result;
		}

		FPDF_PAGEOBJECT object_at_path(const Api& p_api, FPDF_PAGE p_page, const PackedInt32Array& p_path) {
			if (p_path.is_empty() || p_path[0] < 0 || p_path[0] >= p_api.FPDFPage_CountObjects(p_page)) {
				return nullptr;
			}
			FPDF_PAGEOBJECT object = p_api.FPDFPage_GetObject(p_page, p_path[0]);
			for (int64_t i = 1; i < p_path.size() && object != nullptr; ++i) {
				const int index = p_path[i];
				if (index < 0 || p_api.FPDFPageObj_GetType(object) != FPDF_PAGEOBJ_FORM || index >= p_api.FPDFFormObj_CountObjects(object)) {
					return nullptr;
				}
				object = p_api.FPDFFormObj_GetObject(object, static_cast<unsigned long>(index));
			}
			return object;
		}

		Dictionary describe_font(const Api& p_api, FPDF_FONT p_font) {
			Dictionary font;
			if (p_font == nullptr) {
				return font;
			}
			font["family"] = utf8(read_buffer<char>([&](char* p_buffer, size_t p_length) {
				return p_api.FPDFFont_GetFamilyName(p_font, p_buffer, p_length);
			}));
			font["base_name"] = utf8(read_buffer<char>([&](char* p_buffer, size_t p_length) {
				return p_api.FPDFFont_GetBaseFontName(p_font, p_buffer, p_length);
			}));
			font["embedded"] = p_api.FPDFFont_GetIsEmbedded(p_font) == 1;
			font["flags"] = p_api.FPDFFont_GetFlags(p_font);
			font["weight"] = p_api.FPDFFont_GetWeight(p_font);
			return font;
		}

		Dictionary describe_object(const Api& p_api, FPDF_PAGE p_page, FPDF_PAGEOBJECT p_object, FPDF_TEXTPAGE p_text,
			const PackedInt32Array& p_path) {
			Dictionary result;
			const int type = p_api.FPDFPageObj_GetType(p_object);
			result["path"] = p_path;
			result["type"] = type;
			result["marked_content_id"] = p_api.FPDFPageObj_GetMarkedContentID(p_object);
			result["has_transparency"] = bool(p_api.FPDFPageObj_HasTransparency(p_object));
			FPDF_BOOL active;
			if (p_api.FPDFPageObj_GetIsActive(p_object, &active)) {
				result["active"] = bool(active);
			}
			float left, bottom, right, top;
			FS_QUADPOINTSF rotated;
			FS_MATRIX matrix;
			if (p_api.FPDFPageObj_GetBounds(p_object, &left, &bottom, &right, &top)) {
				result["bounds"] = rect(left, bottom, right, top);
			}
			if (p_api.FPDFPageObj_GetRotatedBounds(p_object, &rotated)) {
				result["rotated_bounds"] = quad(rotated);
			}
			if (p_api.FPDFPageObj_GetMatrix(p_object, &matrix)) {
				result["transform"] = transform(matrix);
			}
			unsigned int r, g, b, a;
			if (p_api.FPDFPageObj_GetFillColor(p_object, &r, &g, &b, &a)) {
				result["fill_color"] = Color::from_rgba8(r, g, b, a);
			}
			if (p_api.FPDFPageObj_GetStrokeColor(p_object, &r, &g, &b, &a)) {
				result["stroke_color"] = Color::from_rgba8(r, g, b, a);
			}
			float width, phase;
			if (p_api.FPDFPageObj_GetStrokeWidth(p_object, &width)) {
				result["stroke_width"] = width;
			}
			result["line_join"] = p_api.FPDFPageObj_GetLineJoin(p_object);
			result["line_cap"] = p_api.FPDFPageObj_GetLineCap(p_object);
			if (p_api.FPDFPageObj_GetDashPhase(p_object, &phase)) {
				result["dash_phase"] = phase;
			}
			const int dash_count = p_api.FPDFPageObj_GetDashCount(p_object);
			PackedFloat32Array dashes;
			if (dash_count > 0 && dashes.resize(dash_count) == OK && p_api.FPDFPageObj_GetDashArray(p_object, dashes.ptrw(), dash_count)) {
				result["dash_array"] = dashes;
			}

			switch (type) {
				case FPDF_PAGEOBJ_TEXT: {
					float size;
					if (p_api.FPDFTextObj_GetFontSize(p_object, &size)) {
						result["font_size"] = size;
					}
					result["text_render_mode"] = int(p_api.FPDFTextObj_GetTextRenderMode(p_object));
					result["font"] = describe_font(p_api, p_api.FPDFTextObj_GetFont(p_object));
					if (p_text != nullptr) {
						result["text"] = utf16(read_buffer([&](uint8_t* p_buffer, unsigned long p_length) {
							return p_api.FPDFTextObj_GetText(p_object, p_text, reinterpret_cast<FPDF_WCHAR*>(p_buffer), p_length);
						}));
					}
				} break;
				case FPDF_PAGEOBJ_PATH: {
					TypedArray<Dictionary> segments;
					const int count = p_api.FPDFPath_CountSegments(p_object);
					for (int i = 0; i < count; ++i) {
						const FPDF_PATHSEGMENT segment = p_api.FPDFPath_GetPathSegment(p_object, i);
						float x, y;
						if (segment == nullptr || !p_api.FPDFPathSegment_GetPoint(segment, &x, &y)) {
							continue;
						}
						Dictionary entry;
						entry["type"] = p_api.FPDFPathSegment_GetType(segment);
						entry["point"] = Vector2(x, y);
						entry["close"] = bool(p_api.FPDFPathSegment_GetClose(segment));
						segments.append(entry);
					}
					result["segments"] = segments;
					int mode;
					FPDF_BOOL stroke;
					if (p_api.FPDFPath_GetDrawMode(p_object, &mode, &stroke)) {
						result["fill_mode"] = mode;
						result["stroke"] = bool(stroke);
					}
				} break;
				case FPDF_PAGEOBJ_IMAGE: {
					FPDF_IMAGEOBJ_METADATA metadata;
					if (p_api.FPDFImageObj_GetImageMetadata(p_object, p_page, &metadata)) {
						result["pixel_size"] = Vector2i(metadata.width, metadata.height);
						result["dpi"] = Vector2(metadata.horizontal_dpi, metadata.vertical_dpi);
						result["bits_per_pixel"] = int64_t(metadata.bits_per_pixel);
						result["color_space"] = metadata.colorspace;
					}
					PackedStringArray filters;
					const int count = p_api.FPDFImageObj_GetImageFilterCount(p_object);
					for (int i = 0; i < count; ++i) {
						filters.append(utf8(read_buffer([&](void* p_buffer, unsigned long p_length) {
							return p_api.FPDFImageObj_GetImageFilter(p_object, i, p_buffer, p_length);
						})));
					}
					result["filters"] = filters;
				} break;
				default: break;
			}
			return result;
		}
	}

	TypedArray<Dictionary> PDFReader::get_page_structure(int64_t p_page) {
		PDF_QUERY_OR_RETURN(structure, {});
		PDF_PAGE_OR_RETURN({})
		const auto tree = scoped(api.FPDF_StructTree_GetForPage(page.get()), api.FPDF_StructTree_Close);
		if (!tree) {
			return {};
		}
		TypedArray<Dictionary> result;
		struct Level {
			FPDF_STRUCTELEMENT parent;
			int index;
			int count;
			TypedArray<Dictionary> items;
		};
		std::vector<Level> levels = { { nullptr, 0, api.FPDF_StructTree_CountChildren(tree.get()), result } };
		std::unordered_set<FPDF_STRUCTELEMENT> visited;
		while (!levels.empty()) {
			Level& level = levels.back();
			if (level.index >= level.count) {
				levels.pop_back();
				continue;
			}
			const int index = level.index++;
			const auto element = level.parent ? api.FPDF_StructElement_GetChildAtIndex(level.parent, index) :
				api.FPDF_StructTree_GetChildAtIndex(tree.get(), index);
			if (element == nullptr) {
				if (level.parent != nullptr) {
					const int id = api.FPDF_StructElement_GetChildMarkedContentID(level.parent, index);
					if (id >= 0) {
						Dictionary content;
						content["marked_content_id"] = id;
						level.items.append(content);
					}
				}
				continue;
			}
			if (!visited.insert(element).second) {
				continue;
			}
			Dictionary row = describe_structure(api, element);
			TypedArray<Dictionary> children;
			row["children"] = children;
			level.items.append(row);
			levels.push_back({ element, 0, api.FPDF_StructElement_CountChildren(element), children });
		}
		return result;
	}

	TypedArray<Dictionary> PDFReader::get_page_objects(int64_t p_page) {
		PDF_QUERY_OR_RETURN(page_objects, {});
		PDF_PAGE_OR_RETURN({})
		const auto text_page = api.has(Group::text) ? state->text_page(page.get()) : TextPage(nullptr, nullptr);
		if (api.has(Group::text) && !text_page) {
			return {};
		}
		TypedArray<Dictionary> result;
		struct Level {
			FPDF_PAGEOBJECT form;
			int index;
			int count;
			PackedInt32Array path;
			TypedArray<Dictionary> items;
		};
		std::vector<Level> levels = { { nullptr, 0, api.FPDFPage_CountObjects(page.get()), {}, result } };
		std::unordered_set<FPDF_PAGEOBJECT> visited_forms;
		while (!levels.empty()) {
			Level& level = levels.back();
			if (level.index >= level.count) {
				levels.pop_back();
				continue;
			}
			const int index = level.index++;
			const auto object = level.form ? api.FPDFFormObj_GetObject(level.form, static_cast<unsigned long>(index)) :
				api.FPDFPage_GetObject(page.get(), index);
			if (object == nullptr) {
				continue;
			}
			PackedInt32Array path = level.path;
			path.append(index);
			Dictionary row = describe_object(api, page.get(), object, text_page.get(), path);
			const bool form = api.FPDFPageObj_GetType(object) == FPDF_PAGEOBJ_FORM;
			TypedArray<Dictionary> children;
			if (form) {
				row["children"] = children;
			}
			level.items.append(row);
			if (form && visited_forms.insert(object).second) {
				levels.push_back({ object, 0, api.FPDFFormObj_CountObjects(object), path, children });
			}
		}
		return result;
	}

	Ref<Image> PDFReader::get_page_object_image(int64_t p_page, const PackedInt32Array& p_path, bool p_rendered) {
		PDF_QUERY_OR_RETURN(page_objects, {});
		PDF_PAGE_OR_RETURN({})
		const FPDF_PAGEOBJECT object = object_at_path(api, page.get(), p_path);
		if (object == nullptr || api.FPDFPageObj_GetType(object) != FPDF_PAGEOBJ_IMAGE) {
			state->fail(ERR_INVALID_PARAMETER, "The PDF object path must identify an image.");
			return {};
		}
		unsigned int width, height;
		if (!api.FPDFImageObj_GetImagePixelSize(object, &width, &height) || !fits_bitmap(width, height)) {
			state->fail(ERR_INVALID_PARAMETER, "The PDF image object has invalid dimensions or exceeds the bitmap limit.");
			return {};
		}
		if (p_rendered) {
			float left, bottom, right, top;
			if (!api.FPDFPageObj_GetBounds(object, &left, &bottom, &right, &top)) {
				state->fail(ERR_CANT_ACQUIRE_RESOURCE, "Could not read the rendered PDF image bounds.");
				return {};
			}
			const double rendered_width = std::ceil(double(right)) - std::floor(double(left));
			const double rendered_height = std::ceil(double(top)) - std::floor(double(bottom));
			if (!(rendered_width > 0 && rendered_width <= MAX_BITMAP_EDGE && rendered_height > 0 && rendered_height <= MAX_BITMAP_EDGE) ||
				!fits_bitmap(int64_t(rendered_width), int64_t(rendered_height))) {
				state->fail(ERR_INVALID_PARAMETER, "The rendered PDF image object exceeds the bitmap limit.");
				return {};
			}
		}
		const auto bitmap = scoped(p_rendered ? api.FPDFImageObj_GetRenderedBitmap(state->document, page.get(), object) :
			api.FPDFImageObj_GetBitmap(object), api.FPDFBitmap_Destroy);
		const auto image = bitmap ? bitmap_to_image(api, bitmap.get()) : Ref<Image>();
		if (image.is_null()) {
			state->fail(ERR_CANT_CREATE, "Could not create the PDF image object bitmap.");
		}
		return image;
	}

	PackedByteArray PDFReader::get_page_object_image_data(int64_t p_page, const PackedInt32Array& p_path, bool p_decoded) {
		PDF_QUERY_OR_RETURN(page_objects, {});
		PDF_PAGE_OR_RETURN({})
		const FPDF_PAGEOBJECT object = object_at_path(api, page.get(), p_path);
		if (object == nullptr || api.FPDFPageObj_GetType(object) != FPDF_PAGEOBJ_IMAGE) {
			state->fail(ERR_INVALID_PARAMETER, "The PDF object path must identify an image.");
			return {};
		}
		const auto read = p_decoded ? api.FPDFImageObj_GetImageDataDecoded : api.FPDFImageObj_GetImageDataRaw;
		return read_buffer([&](void* p_buffer, unsigned long p_length) { return read(object, p_buffer, p_length); });
	}

	PackedByteArray PDFReader::get_page_object_font_data(int64_t p_page, const PackedInt32Array& p_path) {
		PDF_QUERY_OR_RETURN(page_objects, {});
		PDF_PAGE_OR_RETURN({})
		const FPDF_PAGEOBJECT object = object_at_path(api, page.get(), p_path);
		if (object == nullptr || api.FPDFPageObj_GetType(object) != FPDF_PAGEOBJ_TEXT) {
			state->fail(ERR_INVALID_PARAMETER, "The PDF object path must identify a text object.");
			return {};
		}
		const FPDF_FONT font = api.FPDFTextObj_GetFont(object);
		if (font == nullptr) {
			return {};
		}
		return read_sized<size_t>([&](uint8_t* p_buffer, size_t p_length, size_t* r_length) {
			return api.FPDFFont_GetFontData(font, p_buffer, p_length, r_length);
		});
	}
#else
	PDF_READER_UNAVAILABLE(TypedArray<Dictionary>, get_page_annotations, (int64_t), {})
	PDF_READER_UNAVAILABLE(TypedArray<Dictionary>, get_form_fields, (int64_t), {})
	PDF_READER_UNAVAILABLE(String, get_annotation_string, (int64_t, int64_t, const String&), {})
	PDF_READER_UNAVAILABLE(TypedArray<Dictionary>, get_attachments, (), {})
	PDF_READER_UNAVAILABLE(PackedByteArray, get_attachment_data, (int64_t), {})
	PDF_READER_UNAVAILABLE(TypedArray<Dictionary>, get_signatures, (), {})
	PDF_READER_UNAVAILABLE(PackedByteArray, get_signature_contents, (int64_t), {})
	PDF_READER_UNAVAILABLE(TypedArray<Dictionary>, get_page_structure, (int64_t), {})
	PDF_READER_UNAVAILABLE(TypedArray<Dictionary>, get_page_objects, (int64_t), {})
	PDF_READER_UNAVAILABLE(Ref<Image>, get_page_object_image, (int64_t, const PackedInt32Array&, bool), {})
	PDF_READER_UNAVAILABLE(PackedByteArray, get_page_object_image_data, (int64_t, const PackedInt32Array&, bool), {})
	PDF_READER_UNAVAILABLE(PackedByteArray, get_page_object_font_data, (int64_t, const PackedInt32Array&), {})
	PDF_READER_UNAVAILABLE(TypedArray<Dictionary>, get_javascript_actions, (), {})
	PDF_READER_UNAVAILABLE(TypedArray<Dictionary>, get_xfa_packets, (), {})
#endif
} // namespace godot
