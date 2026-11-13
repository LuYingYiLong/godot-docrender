#pragma once

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <limits>
#include <mutex>

#ifdef DOCRENDER_PDFIUM_ENABLED
#include <fpdf_annot.h>
#include <fpdf_attachment.h>
#include <fpdf_catalog.h>
#include <fpdf_doc.h>
#include <fpdf_edit.h>
#include <fpdf_flatten.h>
#include <fpdf_formfill.h>
#include <fpdf_javascript.h>
#include <fpdf_ppo.h>
#include <fpdf_save.h>
#include <fpdf_signature.h>
#include <fpdf_structtree.h>
#include <fpdf_text.h>
#include <fpdf_thumbnail.h>
#include <fpdf_transformpage.h>
#include <fpdfview.h>

#include <array>
#include <type_traits>

#include "pdfium_writer_symbols.h"

#define PDFIUM_CORE(X) \
	X(FPDF_InitLibrary) \
	X(FPDF_DestroyLibrary) \
	X(FPDF_LoadMemDocument64) \
	X(FPDF_GetLastError) \
	X(FPDF_CloseDocument) \
	X(FPDF_GetPageCount) \
	X(FPDF_LoadPage) \
	X(FPDF_ClosePage) \
	X(FPDF_GetPageWidthF) \
	X(FPDF_GetPageHeightF) \
	X(FPDFBitmap_CreateEx) \
	X(FPDFBitmap_Destroy) \
	X(FPDFBitmap_FillRect) \
	X(FPDF_RenderPageBitmap) \
	X(FPDF_PageToDevice) \
	X(FPDF_DeviceToPage) \
	X(FPDFBitmap_GetWidth) \
	X(FPDFBitmap_GetHeight) \
	X(FPDFBitmap_GetStride) \
	X(FPDFBitmap_GetFormat) \
	X(FPDFBitmap_GetBuffer)

#define PDFIUM_DOCUMENT(X) \
	X(FPDF_GetFileVersion) \
	X(FPDF_DocumentHasValidCrossReferenceTable) \
	X(FPDF_GetTrailerEnds) \
	X(FPDF_GetDocPermissions) \
	X(FPDF_GetDocUserPermissions) \
	X(FPDF_GetSecurityHandlerRevision) \
	X(FPDF_GetFileIdentifier) \
	X(FPDF_GetMetaText) \
	X(FPDF_GetPageLabel) \
	X(FPDF_VIEWERREF_GetPrintScaling) \
	X(FPDF_VIEWERREF_GetNumCopies) \
	X(FPDF_VIEWERREF_GetPrintPageRange) \
	X(FPDF_VIEWERREF_GetPrintPageRangeCount) \
	X(FPDF_VIEWERREF_GetPrintPageRangeElement) \
	X(FPDF_VIEWERREF_GetDuplex) \
	X(FPDF_GetPageBoundingBox) \
	X(FPDFPage_GetRotation) \
	X(FPDFPage_GetMediaBox) \
	X(FPDFPage_GetCropBox) \
	X(FPDFPage_GetBleedBox) \
	X(FPDFPage_GetTrimBox) \
	X(FPDFPage_GetArtBox) \
	X(FPDFPage_HasTransparency) \
	X(FPDFCatalog_IsTagged) \
	X(FPDFCatalog_GetLanguage) \
	X(FPDF_GetFormType)

#define PDFIUM_THUMBNAIL(X) \
	X(FPDFPage_GetThumbnailAsBitmap)

#define PDFIUM_TEXT(X) \
	X(FPDFText_LoadPage) \
	X(FPDFText_ClosePage) \
	X(FPDFText_CountChars) \
	X(FPDFText_GetUnicode) \
	X(FPDFText_IsGenerated) \
	X(FPDFText_IsHyphen) \
	X(FPDFText_HasUnicodeMapError) \
	X(FPDFText_GetFontSize) \
	X(FPDFText_GetFontInfo) \
	X(FPDFText_GetFontWeight) \
	X(FPDFText_GetFillColor) \
	X(FPDFText_GetStrokeColor) \
	X(FPDFText_GetCharAngle) \
	X(FPDFText_GetCharBox) \
	X(FPDFText_GetLooseCharBox) \
	X(FPDFText_GetMatrix) \
	X(FPDFText_GetCharOrigin) \
	X(FPDFText_GetCharIndexAtPos) \
	X(FPDFText_GetText) \
	X(FPDFText_CountRects) \
	X(FPDFText_GetRect) \
	X(FPDFText_GetBoundedText) \
	X(FPDFText_FindStart) \
	X(FPDFText_FindNext) \
	X(FPDFText_GetSchResultIndex) \
	X(FPDFText_GetSchCount) \
	X(FPDFText_FindClose)

#define PDFIUM_NAVIGATION(X) \
	X(FPDFBookmark_GetFirstChild) \
	X(FPDFBookmark_GetNextSibling) \
	X(FPDFBookmark_GetTitle) \
	X(FPDFBookmark_GetCount) \
	X(FPDFBookmark_GetColor) \
	X(FPDFBookmark_GetStyle) \
	X(FPDFBookmark_GetDest) \
	X(FPDFBookmark_GetAction) \
	X(FPDFAction_GetType) \
	X(FPDFAction_GetDest) \
	X(FPDFAction_GetFilePath) \
	X(FPDFAction_GetURIPath) \
	X(FPDFDest_GetDestPageIndex) \
	X(FPDFDest_GetView) \
	X(FPDFDest_GetLocationInPage) \
	X(FPDF_CountNamedDests) \
	X(FPDF_GetNamedDest) \
	X(FPDFLink_Enumerate) \
	X(FPDFLink_GetAnnotRect) \
	X(FPDFLink_CountQuadPoints) \
	X(FPDFLink_GetQuadPoints) \
	X(FPDFLink_GetDest) \
	X(FPDFLink_GetAction)

#define PDFIUM_WEB_LINKS(X) \
	X(FPDFLink_LoadWebLinks) \
	X(FPDFLink_CloseWebLinks) \
	X(FPDFLink_CountWebLinks) \
	X(FPDFLink_GetURL) \
	X(FPDFLink_CountRects) \
	X(FPDFLink_GetRect) \
	X(FPDFLink_GetTextRange)

#define PDFIUM_ANNOTATIONS(X) \
	X(FPDFPage_GetAnnotCount) \
	X(FPDFPage_GetAnnot) \
	X(FPDFPage_CloseAnnot) \
	X(FPDFAnnot_GetSubtype) \
	X(FPDFAnnot_GetFlags) \
	X(FPDFAnnot_GetRect) \
	X(FPDFAnnot_GetStringValue) \
	X(FPDFAnnot_GetColor) \
	X(FPDFAnnot_CountAttachmentPoints) \
	X(FPDFAnnot_GetAttachmentPoints) \
	X(FPDFAnnot_GetVertices) \
	X(FPDFAnnot_GetInkListCount) \
	X(FPDFAnnot_GetInkListPath) \
	X(FPDFAnnot_GetLine) \
	X(FPDFAnnot_GetBorder)

#define PDFIUM_FORMS(X) \
	X(FPDFDOC_InitFormFillEnvironment) \
	X(FPDFDOC_ExitFormFillEnvironment) \
	X(FPDF_FFLDraw) \
	X(FPDFAnnot_GetFormFieldFlags) \
	X(FPDFAnnot_GetFormFieldName) \
	X(FPDFAnnot_GetFormFieldAlternateName) \
	X(FPDFAnnot_GetFormFieldType) \
	X(FPDFAnnot_GetFormFieldValue) \
	X(FPDFAnnot_GetOptionCount) \
	X(FPDFAnnot_GetOptionLabel) \
	X(FPDFAnnot_IsOptionSelected) \
	X(FPDFAnnot_IsChecked) \
	X(FPDFAnnot_GetFormFieldExportValue) \
	X(FORM_OnAfterLoadPage) \
	X(FORM_OnBeforeClosePage)

#define PDFIUM_ATTACHMENTS(X) \
	X(FPDFDoc_GetAttachmentCount) \
	X(FPDFDoc_GetAttachment) \
	X(FPDFAttachment_GetName) \
	X(FPDFAttachment_GetFile) \
	X(FPDFAttachment_GetDescription) \
	X(FPDFAttachment_GetSubtype) \
	X(FPDFAttachment_GetAFRelationship) \
	X(FPDFAttachment_GetStringValue)

#define PDFIUM_SIGNATURES(X) \
	X(FPDF_GetSignatureCount) \
	X(FPDF_GetSignatureObject) \
	X(FPDFSignatureObj_GetContents) \
	X(FPDFSignatureObj_GetByteRange) \
	X(FPDFSignatureObj_GetSubFilter) \
	X(FPDFSignatureObj_GetReason) \
	X(FPDFSignatureObj_GetTime) \
	X(FPDFSignatureObj_GetDocMDPPermission)

#define PDFIUM_STRUCTURE(X) \
	X(FPDF_StructTree_GetForPage) \
	X(FPDF_StructTree_Close) \
	X(FPDF_StructTree_CountChildren) \
	X(FPDF_StructTree_GetChildAtIndex) \
	X(FPDF_StructElement_GetType) \
	X(FPDF_StructElement_GetTitle) \
	X(FPDF_StructElement_GetAltText) \
	X(FPDF_StructElement_GetActualText) \
	X(FPDF_StructElement_GetID) \
	X(FPDF_StructElement_GetLang) \
	X(FPDF_StructElement_GetExpansion) \
	X(FPDF_StructElement_GetMarkedContentID) \
	X(FPDF_StructElement_CountChildren) \
	X(FPDF_StructElement_GetChildAtIndex) \
	X(FPDF_StructElement_GetChildMarkedContentID) \
	X(FPDF_StructElement_GetMarkedContentIdCount) \
	X(FPDF_StructElement_GetMarkedContentIdAtIndex) \
	X(FPDF_StructElement_GetAttributeCount) \
	X(FPDF_StructElement_GetAttributeAtIndex) \
	X(FPDF_StructElement_Attr_GetCount) \
	X(FPDF_StructElement_Attr_GetName) \
	X(FPDF_StructElement_Attr_GetValue) \
	X(FPDF_StructElement_Attr_GetType) \
	X(FPDF_StructElement_Attr_GetBooleanValue) \
	X(FPDF_StructElement_Attr_GetNumberValue) \
	X(FPDF_StructElement_Attr_GetStringValue) \
	X(FPDF_StructElement_Attr_GetBlobValue) \
	X(FPDF_StructElement_Attr_CountChildren) \
	X(FPDF_StructElement_Attr_GetChildAtIndex)

#define PDFIUM_PAGE_OBJECTS(X) \
	X(FPDFPage_CountObjects) \
	X(FPDFPage_GetObject) \
	X(FPDFPageObj_GetType) \
	X(FPDFPageObj_GetIsActive) \
	X(FPDFPageObj_GetBounds) \
	X(FPDFPageObj_GetRotatedBounds) \
	X(FPDFPageObj_GetMatrix) \
	X(FPDFPageObj_GetMarkedContentID) \
	X(FPDFPageObj_GetFillColor) \
	X(FPDFPageObj_GetStrokeColor) \
	X(FPDFPageObj_GetStrokeWidth) \
	X(FPDFPageObj_GetLineJoin) \
	X(FPDFPageObj_GetLineCap) \
	X(FPDFPageObj_GetDashPhase) \
	X(FPDFPageObj_GetDashCount) \
	X(FPDFPageObj_GetDashArray) \
	X(FPDFPageObj_HasTransparency) \
	X(FPDFFormObj_CountObjects) \
	X(FPDFFormObj_GetObject) \
	X(FPDFPath_CountSegments) \
	X(FPDFPath_GetPathSegment) \
	X(FPDFPathSegment_GetPoint) \
	X(FPDFPathSegment_GetType) \
	X(FPDFPathSegment_GetClose) \
	X(FPDFPath_GetDrawMode) \
	X(FPDFTextObj_GetText) \
	X(FPDFTextObj_GetFont) \
	X(FPDFTextObj_GetFontSize) \
	X(FPDFTextObj_GetTextRenderMode) \
	X(FPDFFont_GetBaseFontName) \
	X(FPDFFont_GetFamilyName) \
	X(FPDFFont_GetIsEmbedded) \
	X(FPDFFont_GetFlags) \
	X(FPDFFont_GetWeight) \
	X(FPDFFont_GetFontData) \
	X(FPDFImageObj_GetImageMetadata) \
	X(FPDFImageObj_GetImagePixelSize) \
	X(FPDFImageObj_GetImageFilterCount) \
	X(FPDFImageObj_GetImageFilter) \
	X(FPDFImageObj_GetBitmap) \
	X(FPDFImageObj_GetRenderedBitmap) \
	X(FPDFImageObj_GetImageDataRaw) \
	X(FPDFImageObj_GetImageDataDecoded)

#define PDFIUM_JAVASCRIPT(X) \
	X(FPDFDoc_GetJavaScriptActionCount) \
	X(FPDFDoc_GetJavaScriptAction) \
	X(FPDFDoc_CloseJavaScriptAction) \
	X(FPDFJavaScriptAction_GetName) \
	X(FPDFJavaScriptAction_GetScript)

#define PDFIUM_XFA(X) \
	X(FPDF_GetXFAPacketCount) \
	X(FPDF_GetXFAPacketName) \
	X(FPDF_GetXFAPacketContent)

#define PDFIUM_GROUPS(X) \
	X(core, PDFIUM_CORE) \
	X(document, PDFIUM_DOCUMENT) \
	X(thumbnail, PDFIUM_THUMBNAIL) \
	X(text, PDFIUM_TEXT) \
	X(navigation, PDFIUM_NAVIGATION) \
	X(web_links, PDFIUM_WEB_LINKS) \
	X(annotations, PDFIUM_ANNOTATIONS) \
	X(forms, PDFIUM_FORMS) \
	X(attachments, PDFIUM_ATTACHMENTS) \
	X(signatures, PDFIUM_SIGNATURES) \
	X(structure, PDFIUM_STRUCTURE) \
	X(page_objects, PDFIUM_PAGE_OBJECTS) \
	X(javascript, PDFIUM_JAVASCRIPT) \
	X(xfa, PDFIUM_XFA)
#endif

namespace godot::pdfium {
	// 互斥锁也涵盖初始化和本地句柄销毁
	std::recursive_mutex& mutex();													// 返回用于串行化 PDFium 调用的递归锁
	Error read_file(
		const String& p_path,
		PackedByteArray& r_bytes,
		String& r_message
	);																				// 读取 PDF 文件字节并在失败时写出诊断信息

#ifdef DOCRENDER_PDFIUM_ENABLED
	enum class Group {
#define PDFIUM_GROUP_ENUM(group, symbols) group,
		PDFIUM_GROUPS(PDFIUM_GROUP_ENUM)
		PDFIUM_WRITER_GROUPS(PDFIUM_GROUP_ENUM)
#undef PDFIUM_GROUP_ENUM
		count,
	};

	class Api {
	public:
#define PDFIUM_DECLARE(symbol) decltype(&::symbol) symbol = nullptr;
#define PDFIUM_DECLARE_GROUP(group, symbols) symbols(PDFIUM_DECLARE)
		PDFIUM_GROUPS(PDFIUM_DECLARE_GROUP)
			PDFIUM_WRITER_SYMBOLS(PDFIUM_DECLARE)
#undef PDFIUM_DECLARE_GROUP
#undef PDFIUM_DECLARE

			~Api();
		bool load(String& r_message);												// 加载 PDFium 动态库、解析符号并初始化后端
		bool has(Group p_group) const;												// 检查指定能力组所需的符号是否齐全
		PackedStringArray capabilities(bool p_writer = false) const;				// 列出可用的读取或写入能力标识

	private:
		HMODULE library = nullptr;
		std::array<bool, static_cast<size_t>(Group::count)> available{};
	};

	struct FormInfo : FPDF_FORMFILLINFO {
		FPDF_PAGE page = nullptr;
		int index = -1;

		FormInfo();
	};

	FPDF_DOCUMENT load_document(
		const Api& p_api,
		const PackedByteArray& p_bytes,
		const String& p_password,
		const String& p_source,
		Error& r_error,
		String& r_message
	);																				// 从内存加载 PDF 并写出错误码和诊断信息
	std::shared_ptr<Api> acquire(String& r_message);								// 按需加载并返回共享的 PDFium 后端
	void shutdown();																// 释放后端缓存持有的 PDFium 实例引用

	// 用指定关闭函数将原生句柄包装为 RAII 对象
	template <typename Handle, typename Deleter>
	auto scoped(Handle p_handle, Deleter p_deleter) {
		return std::unique_ptr<std::remove_pointer_t<Handle>, Deleter>(p_handle, p_deleter);
	}

	using Page = std::unique_ptr<std::remove_pointer_t<FPDF_PAGE>, decltype(&::FPDF_ClosePage)>;
	using TextPage = std::unique_ptr<std::remove_pointer_t<FPDF_TEXTPAGE>, decltype(&::FPDFText_ClosePage)>;

	// PDFium 的两遍 API 要么计数字节，要么计数元素
	// 把计数和元素类型放在一起，这样字符和几何缓冲区就能保持对齐
	template <typename Element = uint8_t, typename Read>
	PackedByteArray read_buffer(Read p_read) { // 先查询元素数量，再按元素大小分配并读取 PDFium 输出
		const uint64_t count = p_read(static_cast<Element*>(nullptr), 0);
		if (count == 0 || count > uint64_t(std::numeric_limits<int64_t>::max()) / sizeof(Element)) {
			return {};
		}
		PackedByteArray buffer;
		if (buffer.resize(int64_t(count * sizeof(Element))) != OK) {
			return {};
		}
		const uint64_t copied = p_read(reinterpret_cast<Element*>(buffer.ptrw()), count);
		if (copied == 0 || copied > count) {
			return {};
		}
		if (copied != count) {
			buffer.resize(int64_t(copied * sizeof(Element)));
		}
		return buffer;
	}

	String utf16(const PackedByteArray& p_buffer);									// 将 PDFium 的 UTF-16 缓冲解码为字符串并去掉末尾空字符
	String utf8(const PackedByteArray& p_buffer);									// 将 PDFium 的 UTF-8 缓冲解码为字符串并去掉末尾空字符
	Rect2 rect(double p_left, double p_bottom, double p_right, double p_top);		// 按 PDF 的左右上下边界构造非负尺寸的矩形
	Rect2 rect(const FS_RECTF& p_rect);												// 将 PDFium 矩形转换为 Godot 矩形，保留 PDF 点坐标
	Transform2D transform(const FS_MATRIX& p_matrix);								// 将 PDFium 矩阵转换为 Godot 二维变换
	PackedVector2Array quad(const FS_QUADPOINTSF& p_quad);							// 按 PDFium 的顶点顺序返回四边形的四个点
	Ref<Image> bitmap_to_image(const Api& p_api, FPDF_BITMAP p_bitmap);				// 将尺寸合规的 PDFium 位图转换为 RGBA8 图像
	inline constexpr int64_t MAX_BITMAP_EDGE = 8192;
	bool fits_bitmap(int64_t p_width, int64_t p_height);							// 检查位图是否满足 8192 像素单边和 64 MiB 输出限制
#endif
} // namespace godot::pdfium
