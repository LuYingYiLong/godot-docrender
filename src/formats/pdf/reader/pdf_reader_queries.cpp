#include "pdf_reader_internal.h"

#ifdef DOCRENDER_PDFIUM_ENABLED
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>

#include <unordered_set>
#include <vector>
#endif

namespace godot {
#ifdef DOCRENDER_PDFIUM_ENABLED
	using namespace pdfium;

#define PDF_TEXT_OR_RETURN(result) \
	PDF_PAGE_OR_RETURN(result) \
	const auto text_page = state->text_page(page.get()); \
	if (!text_page) { return result; }

	namespace {
		bool text_range(const Api& p_api, FPDF_TEXTPAGE p_page, int64_t p_start, int64_t p_count, int& r_count) {
			const int total = p_api.FPDFText_CountChars(p_page);
			if (total < 0 || p_start < 0 || p_start > total || p_count < -1 || p_count > total - p_start) {
				return false;
			}
			r_count = p_count == -1 ? total - int(p_start) : int(p_count);
			return true;
		}

		TypedArray<Rect2> text_rects(const Api& p_api, FPDF_TEXTPAGE p_page, int p_start, int p_count) {
			TypedArray<Rect2> result;
			const int count = p_api.FPDFText_CountRects(p_page, p_start, p_count);
			for (int i = 0; i < count; ++i) {
				double left, top, right, bottom;
				if (p_api.FPDFText_GetRect(p_page, i, &left, &top, &right, &bottom)) {
					result.append(rect(left, bottom, right, top));
				}
			}
			return result;
		}

		Dictionary destination(const Api& p_api, FPDF_DOCUMENT p_document, FPDF_DEST p_destination) {
			Dictionary result;
			if (p_destination == nullptr) {
				return result;
			}
			result["page_index"] = p_api.FPDFDest_GetDestPageIndex(p_document, p_destination);
			unsigned long count = 0;
			FS_FLOAT parameters[4]{};
			result["view"] = int64_t(p_api.FPDFDest_GetView(p_destination, &count, parameters));
			PackedFloat32Array values;
			for (unsigned long i = 0; i < count && i < std::size(parameters); ++i) {
				values.append(parameters[i]);
			}
			result["parameters"] = values;

			FPDF_BOOL has_x, has_y, has_zoom;
			FS_FLOAT x, y, zoom;
			if (p_api.FPDFDest_GetLocationInPage(p_destination, &has_x, &has_y, &has_zoom, &x, &y, &zoom)) {
				if (has_x) { result["x"] = x; }
				if (has_y) { result["y"] = y; }
				if (has_zoom) { result["zoom"] = zoom; }
			}
			return result;
		}

		Dictionary action(const Api& p_api, FPDF_DOCUMENT p_document, FPDF_ACTION p_action) {
			Dictionary result;
			if (p_action == nullptr) {
				return result;
			}
			const unsigned long type = p_api.FPDFAction_GetType(p_action);
			result["type"] = int64_t(type);
			if (type == PDFACTION_URI) {
				result["uri"] = utf8(read_buffer([&](void* p_buffer, unsigned long p_length) {
					return p_api.FPDFAction_GetURIPath(p_document, p_action, p_buffer, p_length);
				}));
			} else if (type == PDFACTION_REMOTEGOTO || type == PDFACTION_LAUNCH || type == PDFACTION_EMBEDDEDGOTO) {
				result["file_path"] = utf8(read_buffer([&](void* p_buffer, unsigned long p_length) {
					return p_api.FPDFAction_GetFilePath(p_action, p_buffer, p_length);
				}));
			}
			if (type == PDFACTION_GOTO || type == PDFACTION_REMOTEGOTO || type == PDFACTION_EMBEDDEDGOTO) {
				result["destination"] = destination(p_api, p_document, p_api.FPDFAction_GetDest(p_document, p_action));
			}
			return result;
		}
	}

	Dictionary PDFReader::get_document_info() {
		PDF_QUERY_OR_RETURN(document, {});
		const FPDF_DOCUMENT document = state->document;
		Dictionary result;
		int version = 0;
		if (api.FPDF_GetFileVersion(document, &version)) {
			result["version"] = version;
		}
		result["page_count"] = api.FPDF_GetPageCount(document);
		result["valid_cross_reference_table"] = bool(api.FPDF_DocumentHasValidCrossReferenceTable(document));
		result["permissions"] = int64_t(api.FPDF_GetDocPermissions(document));
		result["user_permissions"] = int64_t(api.FPDF_GetDocUserPermissions(document));
		result["security_handler_revision"] = api.FPDF_GetSecurityHandlerRevision(document);
		result["form_type"] = api.FPDF_GetFormType(document);
		result["tagged"] = bool(api.FPDFCatalog_IsTagged(document));
		result["language"] = utf16(read_buffer<FPDF_WCHAR>([&](FPDF_WCHAR* p_buffer, unsigned long p_length) {
			return api.FPDFCatalog_GetLanguage(document, p_buffer, p_length);
		}));
		result["permanent_id"] = read_buffer([&](void* p_buffer, unsigned long p_length) {
			return api.FPDF_GetFileIdentifier(document, FILEIDTYPE_PERMANENT, p_buffer, p_length);
		});
		result["changing_id"] = read_buffer([&](void* p_buffer, unsigned long p_length) {
			return api.FPDF_GetFileIdentifier(document, FILEIDTYPE_CHANGING, p_buffer, p_length);
		});

		const auto trailers = read_buffer<unsigned int>([&](unsigned int* p_buffer, unsigned long p_length) {
			return api.FPDF_GetTrailerEnds(document, p_buffer, p_length);
		});
		PackedInt64Array offsets;
		const auto* trailer_offsets = reinterpret_cast<const unsigned int*>(trailers.ptr());
		for (int64_t i = 0; i < trailers.size() / int64_t(sizeof(unsigned int)); ++i) {
			offsets.append(trailer_offsets[i]);
		}
		result["trailer_ends"] = offsets;

		Dictionary preferences;
		preferences["print_scaling"] = bool(api.FPDF_VIEWERREF_GetPrintScaling(document));
		preferences["copies"] = api.FPDF_VIEWERREF_GetNumCopies(document);
		preferences["duplex"] = int(api.FPDF_VIEWERREF_GetDuplex(document));
		PackedInt32Array page_range;
		if (const FPDF_PAGERANGE range = api.FPDF_VIEWERREF_GetPrintPageRange(document)) {
			for (size_t i = 0, count = api.FPDF_VIEWERREF_GetPrintPageRangeCount(range); i < count; ++i) {
				page_range.append(api.FPDF_VIEWERREF_GetPrintPageRangeElement(range, i));
			}
		}
		preferences["print_page_range"] = page_range;
		result["viewer_preferences"] = preferences;
		return result;
	}


	Dictionary PDFReader::get_metadata() {
		PDF_QUERY_OR_RETURN(document, {});
		constexpr const char* tags[] = { "Title", "Author", "Subject", "Keywords", "Creator", "Producer", "CreationDate", "ModDate" };
		Dictionary result;
		for (const char* tag : tags) {
			const auto value = utf16(read_buffer([&](void* p_buffer, unsigned long p_length) {
				return api.FPDF_GetMetaText(state->document, tag, p_buffer, p_length);
			}));
			if (!value.is_empty()) {
				result[tag] = value;
			}
		}
		return result;
	}

	Dictionary PDFReader::get_page_info(int64_t p_page) {
		PDF_QUERY_OR_RETURN(document, {});
		PDF_PAGE_OR_RETURN({})
		Dictionary result;
		result["page_index"] = p_page;
		result["size_pt"] = Vector2(api.FPDF_GetPageWidthF(page.get()), api.FPDF_GetPageHeightF(page.get()));
		result["rotation"] = api.FPDFPage_GetRotation(page.get()) * 90;
		result["has_transparency"] = bool(api.FPDFPage_HasTransparency(page.get()));
		result["label"] = utf16(read_buffer([&](void* p_buffer, unsigned long p_length) {
			return api.FPDF_GetPageLabel(state->document, int(p_page), p_buffer, p_length);
		}));

		Dictionary boxes;
		const struct Box {
			const char* name;
			decltype(&::FPDFPage_GetMediaBox) get;
		} definitions[] = {
			{ "media", api.FPDFPage_GetMediaBox },
			{ "crop", api.FPDFPage_GetCropBox },
			{ "bleed", api.FPDFPage_GetBleedBox },
			{ "trim", api.FPDFPage_GetTrimBox },
			{ "art", api.FPDFPage_GetArtBox },
		};
		for (const Box& box : definitions) {
			float left, bottom, right, top;
			if (box.get(page.get(), &left, &bottom, &right, &top)) {
				boxes[box.name] = rect(left, bottom, right, top);
			}
		}
		FS_RECTF bounds;
		if (api.FPDF_GetPageBoundingBox(page.get(), &bounds)) {
			boxes["bounds"] = rect(bounds);
		}
		result["boxes"] = boxes;
		return result;
	}

	int64_t PDFReader::get_page_char_count(int64_t p_page) {
		PDF_QUERY_OR_RETURN(text, 0);
		PDF_TEXT_OR_RETURN(0)
		const int count = api.FPDFText_CountChars(text_page.get());
		if (count < 0) {
			state->fail(ERR_FILE_CORRUPT, "Could not count PDF text characters.");
			return 0;
		}
		return count;
	}

	String PDFReader::get_page_text(int64_t p_page, int64_t p_start, int64_t p_count) {
		PDF_QUERY_OR_RETURN(text, {});
		PDF_TEXT_OR_RETURN({})
		int count;
		if (!text_range(api, text_page.get(), p_start, p_count, count)) {
			state->fail(ERR_INVALID_PARAMETER, "PDF text range is out of bounds.");
			return {};
		}
		if (count == 0) {
			return {};
		}
		return utf16(read_buffer<FPDF_WCHAR>([&](FPDF_WCHAR* p_buffer, int64_t) -> int64_t {
			return p_buffer == nullptr ? int64_t(count) + 1 : api.FPDFText_GetText(text_page.get(), int(p_start), count, p_buffer);
		}));
	}

	String PDFReader::get_page_text_in_rect(int64_t p_page, const Rect2& p_rect) {
		PDF_QUERY_OR_RETURN(text, {});
		PDF_TEXT_OR_RETURN({})
		if (!p_rect.position.is_finite() || !p_rect.get_end().is_finite() || p_rect.size.x < 0 || p_rect.size.y < 0) {
			state->fail(ERR_INVALID_PARAMETER, "Invalid PDF text rectangle.");
			return {};
		}
		const Vector2 end = p_rect.get_end();
		return utf16(read_buffer<FPDF_WCHAR>([&](FPDF_WCHAR* p_buffer, int64_t p_length) -> int64_t {
			if (p_length > std::numeric_limits<int>::max()) {
				return 0;
			}
			const int count = api.FPDFText_GetBoundedText(text_page.get(), p_rect.position.x, end.y, end.x, p_rect.position.y,
				p_buffer, int(p_length));
			return p_buffer == nullptr && count > 0 ? int64_t(count) + 1 : count;
		}));
	}

	Dictionary PDFReader::get_char_info(int64_t p_page, int64_t p_index) {
		PDF_QUERY_OR_RETURN(text, {});
		PDF_TEXT_OR_RETURN({})
		const auto text = text_page.get();
		if (p_index < 0 || p_index >= api.FPDFText_CountChars(text)) {
			state->fail(ERR_INVALID_PARAMETER, "PDF character index is out of bounds.");
			return {};
		}
		const int index = int(p_index);
		Dictionary result;
		result["index"] = index;
		result["unicode"] = int64_t(api.FPDFText_GetUnicode(text, index));
		result["generated"] = api.FPDFText_IsGenerated(text, index) == 1;
		result["hyphen"] = api.FPDFText_IsHyphen(text, index) == 1;
		result["unicode_map_error"] = api.FPDFText_HasUnicodeMapError(text, index) == 1;
		result["angle"] = api.FPDFText_GetCharAngle(text, index);
		result["font_size"] = api.FPDFText_GetFontSize(text, index);
		result["font_weight"] = api.FPDFText_GetFontWeight(text, index);
		int font_flags = 0;
		result["font_name"] = utf8(read_buffer([&](void* p_buffer, unsigned long p_length) {
			return api.FPDFText_GetFontInfo(text, index, p_buffer, p_length, &font_flags);
		}));
		result["font_flags"] = font_flags;
		double left, bottom, right, top, x, y;
		FS_RECTF loose_bounds;
		FS_MATRIX matrix;
		if (api.FPDFText_GetCharBox(text, index, &left, &right, &bottom, &top)) {
			result["bounds"] = rect(left, bottom, right, top);
		}
		if (api.FPDFText_GetLooseCharBox(text, index, &loose_bounds)) {
			result["loose_bounds"] = rect(loose_bounds);
		}
		if (api.FPDFText_GetCharOrigin(text, index, &x, &y)) {
			result["origin"] = Vector2(x, y);
		}
		if (api.FPDFText_GetMatrix(text, index, &matrix)) {
			result["transform"] = transform(matrix);
		}
		unsigned int r, g, b, a;
		if (api.FPDFText_GetFillColor(text, index, &r, &g, &b, &a)) {
			result["fill_color"] = Color::from_rgba8(r, g, b, a);
		}
		if (api.FPDFText_GetStrokeColor(text, index, &r, &g, &b, &a)) {
			result["stroke_color"] = Color::from_rgba8(r, g, b, a);
		}
		return result;
	}

	TypedArray<Rect2> PDFReader::get_text_rects(int64_t p_page, int64_t p_start, int64_t p_count) {
		PDF_QUERY_OR_RETURN(text, {});
		PDF_TEXT_OR_RETURN({})
		int count;
		if (!text_range(api, text_page.get(), p_start, p_count, count)) {
			state->fail(ERR_INVALID_PARAMETER, "PDF text range is out of bounds.");
			return {};
		}
		return text_rects(api, text_page.get(), int(p_start), count);
	}

	int64_t PDFReader::hit_test_text(int64_t p_page, const Vector2& p_position, const Vector2& p_tolerance) {
		PDF_QUERY_OR_RETURN(text, -1);
		PDF_TEXT_OR_RETURN(-1)
		if (!p_position.is_finite() || !p_tolerance.is_finite() || p_tolerance.x < 0 || p_tolerance.y < 0) {
			state->fail(ERR_INVALID_PARAMETER, "Invalid PDF text hit-test position or tolerance.");
			return -1;
		}
		const int index = api.FPDFText_GetCharIndexAtPos(text_page.get(), p_position.x, p_position.y, p_tolerance.x, p_tolerance.y);
		if (index < -1) {
			state->fail(ERR_CANT_ACQUIRE_RESOURCE, "Could not locate the PDF character.");
			return -1;
		}
		return index;
	}

	TypedArray<Dictionary> PDFReader::find_text(int64_t p_page, const String& p_query, int64_t p_flags) {
		PDF_QUERY_OR_RETURN(text, {});
		PDF_TEXT_OR_RETURN({})
		if (p_query.is_empty() || p_flags < 0 || p_flags >= int64_t(FIND_CONSECUTIVE) * 2) {
			state->fail(ERR_INVALID_PARAMETER, "PDF text search requires a non-empty query and valid flags.");
			return {};
		}
		const Char16String query = p_query.utf16();
		const auto handle = scoped(api.FPDFText_FindStart(text_page.get(), reinterpret_cast<FPDF_WIDESTRING>(query.get_data()),
			static_cast<unsigned long>(p_flags), 0), api.FPDFText_FindClose);
		if (!handle) {
			state->fail(ERR_CANT_CREATE, "Could not start the PDF text search.");
			return {};
		}
		TypedArray<Dictionary> result;
		while (api.FPDFText_FindNext(handle.get())) {
			const int start = api.FPDFText_GetSchResultIndex(handle.get());
			const int count = api.FPDFText_GetSchCount(handle.get());
			Dictionary match;
			match["start"] = start;
			match["count"] = count;
			match["rects"] = text_rects(api, text_page.get(), start, count);
			result.append(match);
		}
		return result;
	}

	TypedArray<Dictionary> PDFReader::get_outline() {
		PDF_QUERY_OR_RETURN(navigation, {});
		const FPDF_DOCUMENT document = state->document;
		TypedArray<Dictionary> result;
		struct Level {
			FPDF_BOOKMARK next;
			TypedArray<Dictionary> items;
		};
		std::vector<Level> levels = { { api.FPDFBookmark_GetFirstChild(document, nullptr), result } };
		std::unordered_set<FPDF_BOOKMARK> visited;
		while (!levels.empty()) {
			Level& level = levels.back();
			const FPDF_BOOKMARK bookmark = level.next;
			if (bookmark == nullptr || !visited.insert(bookmark).second) {
				levels.pop_back();
				continue;
			}
			level.next = api.FPDFBookmark_GetNextSibling(document, bookmark);
			Dictionary row;
			row["title"] = utf16(read_buffer([&](void* p_buffer, unsigned long p_length) {
				return api.FPDFBookmark_GetTitle(bookmark, p_buffer, p_length);
			}));
			row["destination"] = destination(api, document, api.FPDFBookmark_GetDest(document, bookmark));
			row["action"] = action(api, document, api.FPDFBookmark_GetAction(bookmark));
			row["count"] = api.FPDFBookmark_GetCount(bookmark);
			row["style"] = api.FPDFBookmark_GetStyle(bookmark);
			float r, g, b;
			if (api.FPDFBookmark_GetColor(bookmark, &r, &g, &b)) {
				row["color"] = Color(r, g, b);
			}
			TypedArray<Dictionary> children;
			row["children"] = children;
			level.items.append(row);
			if (const FPDF_BOOKMARK child = api.FPDFBookmark_GetFirstChild(document, bookmark)) {
				levels.push_back({ child, children });
			}
		}
		return result;
	}

	TypedArray<Dictionary> PDFReader::get_named_destinations() {
		PDF_QUERY_OR_RETURN(navigation, {});
		const FPDF_DOCUMENT document = state->document;
		const uint64_t count = api.FPDF_CountNamedDests(document);
		TypedArray<Dictionary> result;
		for (int64_t i = 0; uint64_t(i) < count && i <= std::numeric_limits<int>::max(); ++i) {
			FPDF_DEST target = nullptr;
			const auto name = utf16(read_buffer([&](void* p_buffer, long p_length) -> long {
				target = api.FPDF_GetNamedDest(document, int(i), p_buffer, &p_length);
				return p_length > 0 ? p_length : 0;
			}));
			if (target != nullptr) {
				Dictionary row = destination(api, document, target);
				row["name"] = name;
				result.append(row);
			}
		}
		return result;
	}

	TypedArray<Dictionary> PDFReader::get_page_links(int64_t p_page) {
		PDF_QUERY_OR_RETURN(navigation, {});
		PDF_PAGE_OR_RETURN({})
		TypedArray<Dictionary> result;
		int position = 0;
		FPDF_LINK link;
		while (api.FPDFLink_Enumerate(page.get(), &position, &link)) {
			Dictionary row;
			FS_RECTF bounds;
			if (api.FPDFLink_GetAnnotRect(link, &bounds)) {
				row["rect"] = rect(bounds);
			}
			TypedArray<PackedVector2Array> quads;
			const int count = api.FPDFLink_CountQuadPoints(link);
			for (int i = 0; i < count; ++i) {
				FS_QUADPOINTSF points;
				if (api.FPDFLink_GetQuadPoints(link, i, &points)) {
					quads.append(quad(points));
				}
			}
			row["quads"] = quads;
			row["destination"] = destination(api, state->document, api.FPDFLink_GetDest(state->document, link));
			row["action"] = action(api, state->document, api.FPDFLink_GetAction(link));
			result.append(row);
		}
		return result;
	}

	TypedArray<Dictionary> PDFReader::get_page_web_links(int64_t p_page) {
		PDF_QUERY_OR_RETURN(web_links, {});
		if (!api.has(Group::text)) {
			state->fail(ERR_UNAVAILABLE, "PDF web links require text support.");
			return {};
		}
		PDF_TEXT_OR_RETURN({})
		const auto links = scoped(api.FPDFLink_LoadWebLinks(text_page.get()), api.FPDFLink_CloseWebLinks);
		if (!links) {
			return {};
		}
		TypedArray<Dictionary> result;
		const int count = api.FPDFLink_CountWebLinks(links.get());
		for (int i = 0; i < count; ++i) {
			Dictionary row;
			row["uri"] = utf16(read_buffer<FPDF_WCHAR>([&](FPDF_WCHAR* p_buffer, int p_length) {
				return api.FPDFLink_GetURL(links.get(), i, p_buffer, p_length);
			}));
			TypedArray<Rect2> rects;
			const int rect_count = api.FPDFLink_CountRects(links.get(), i);
			for (int j = 0; j < rect_count; ++j) {
				double left, top, right, bottom;
				if (api.FPDFLink_GetRect(links.get(), i, j, &left, &top, &right, &bottom)) {
					rects.append(rect(left, bottom, right, top));
				}
			}
			row["rects"] = rects;
			int start, length;
			if (api.FPDFLink_GetTextRange(links.get(), i, &start, &length)) {
				row["start"] = start;
				row["count"] = length;
			}
			result.append(row);
		}
		return result;
	}

#undef PDF_TEXT_OR_RETURN
#else
	PDF_READER_UNAVAILABLE(Dictionary, get_document_info, (), {})
	PDF_READER_UNAVAILABLE(Dictionary, get_metadata, (), {})
	PDF_READER_UNAVAILABLE(Dictionary, get_page_info, (int64_t), {})
	PDF_READER_UNAVAILABLE(int64_t, get_page_char_count, (int64_t), {})
	PDF_READER_UNAVAILABLE(String, get_page_text, (int64_t, int64_t, int64_t), {})
	PDF_READER_UNAVAILABLE(String, get_page_text_in_rect, (int64_t, const Rect2&), {})
	PDF_READER_UNAVAILABLE(Dictionary, get_char_info, (int64_t, int64_t), {})
	PDF_READER_UNAVAILABLE(TypedArray<Rect2>, get_text_rects, (int64_t, int64_t, int64_t), {})
	PDF_READER_UNAVAILABLE(int64_t, hit_test_text, (int64_t, const Vector2&, const Vector2&), -1)
	PDF_READER_UNAVAILABLE(TypedArray<Dictionary>, find_text, (int64_t, const String&, int64_t), {})
	PDF_READER_UNAVAILABLE(TypedArray<Dictionary>, get_outline, (), {})
	PDF_READER_UNAVAILABLE(TypedArray<Dictionary>, get_named_destinations, (), {})
	PDF_READER_UNAVAILABLE(TypedArray<Dictionary>, get_page_links, (int64_t), {})
	PDF_READER_UNAVAILABLE(TypedArray<Dictionary>, get_page_web_links, (int64_t), {})
#endif
} // namespace godot
