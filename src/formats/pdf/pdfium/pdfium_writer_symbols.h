#pragma once

#define PDFIUM_WRITER_SYMBOLS(X) \
	X(FPDF_CreateNewDocument) \
	X(FPDF_SaveAsCopy) \
	X(FPDF_SaveWithVersion) \
	X(FPDFPage_New) \
	X(FPDFPage_Delete) \
	X(FPDF_MovePages) \
	X(FPDF_ImportPagesByIndex) \
	X(FPDF_ImportPages) \
	X(FPDF_ImportNPagesToOne) \
	X(FPDF_NewXObjectFromPage) \
	X(FPDF_CloseXObject) \
	X(FPDF_NewFormObjectFromXObject) \
	X(FPDFPageObj_Destroy) \
	X(FPDFPage_SetRotation) \
	X(FPDFPage_SetMediaBox) \
	X(FPDFPage_SetCropBox) \
	X(FPDFPage_SetBleedBox) \
	X(FPDFPage_SetTrimBox) \
	X(FPDFPage_SetArtBox) \
	X(FPDFPage_TransFormWithClip) \
	X(FPDF_CreateClipPath) \
	X(FPDF_DestroyClipPath) \
	X(FPDFPage_InsertClipPath) \
	X(FPDFPage_TransformAnnots) \
	X(FPDFPage_GenerateContent) \
	X(FPDFPage_InsertObjectAtIndex) \
	X(FPDFPage_RemoveObject) \
	X(FPDFFormObj_RemoveObject) \
	X(FPDFPageObj_SetMatrix) \
	X(FPDFPageObj_TransformF) \
	X(FPDFPageObj_TransformClipPath) \
	X(FPDFPageObj_SetIsActive) \
	X(FPDFPageObj_SetBlendMode) \
	X(FPDFPageObj_SetFillColor) \
	X(FPDFPageObj_SetStrokeColor) \
	X(FPDFPageObj_SetStrokeWidth) \
	X(FPDFPageObj_SetLineCap) \
	X(FPDFPageObj_SetLineJoin) \
	X(FPDFPageObj_SetDashArray) \
	X(FPDFPageObj_CreateNewPath) \
	X(FPDFPageObj_CreateNewRect) \
	X(FPDFPath_MoveTo) \
	X(FPDFPath_LineTo) \
	X(FPDFPath_BezierTo) \
	X(FPDFPath_Close) \
	X(FPDFPath_SetDrawMode) \
	X(FPDFPageObj_CountMarks) \
	X(FPDFPageObj_GetMark) \
	X(FPDFPageObj_AddMark) \
	X(FPDFPageObj_AddExistingMark) \
	X(FPDFPageObj_RemoveMark) \
	X(FPDFPageObjMark_SetIntParam) \
	X(FPDFPageObjMark_SetFloatParam) \
	X(FPDFPageObjMark_SetStringParam) \
	X(FPDFPageObjMark_SetBlobParam) \
	X(FPDFPageObjMark_RemoveParam) \
	X(FPDFText_LoadStandardFont) \
	X(FPDFText_LoadFont) \
	X(FPDFText_LoadCidType2Font) \
	X(FPDFFont_Close) \
	X(FPDFPageObj_CreateTextObj) \
	X(FPDFText_SetText) \
	X(FPDFText_SetCharcodes) \
	X(FPDFText_SetPositions) \
	X(FPDFTextObj_SetFontSize) \
	X(FPDFTextObj_SetTextRenderMode) \
	X(FPDFPageObj_NewImageObj) \
	X(FPDFImageObj_SetBitmap) \
	X(FPDFImageObj_LoadJpegFileInline) \
	X(FPDFPage_GetAnnotIndex) \
	X(FPDFAnnot_IsSupportedSubtype) \
	X(FPDFPage_CreateAnnot) \
	X(FPDFPage_RemoveAnnot) \
	X(FPDFAnnot_SetRect) \
	X(FPDFAnnot_SetColor) \
	X(FPDFAnnot_SetBorder) \
	X(FPDFAnnot_SetFlags) \
	X(FPDFAnnot_SetStringValue) \
	X(FPDFAnnot_SetAttachmentPoints) \
	X(FPDFAnnot_AppendAttachmentPoints) \
	X(FPDFAnnot_AddInkStroke) \
	X(FPDFAnnot_RemoveInkList) \
	X(FPDFAnnot_SetURI) \
	X(FPDFAnnot_GetFileAttachment) \
	X(FPDFAnnot_AddFileAttachment) \
	X(FPDFAttachment_SetFile) \
	X(FPDFAnnot_SetAP) \
	X(FPDFAnnot_SetFontColor) \
	X(FPDFAnnot_IsObjectSupportedSubtype) \
	X(FPDFAnnot_GetObjectCount) \
	X(FPDFAnnot_GetObject) \
	X(FPDFAnnot_AppendObject) \
	X(FPDFAnnot_RemoveObject) \
	X(FPDFAnnot_UpdateObject) \
	X(FPDFAnnot_SetFormFieldFlags) \
	X(FORM_SetFocusedAnnot) \
	X(FORM_ForceToKillFocus) \
	X(FORM_SelectAllText) \
	X(FORM_ReplaceSelection) \
	X(FORM_SetIndexSelected) \
	X(FORM_OnChar) \
	X(FPDFPage_Flatten) \
	X(FPDFDoc_AddAttachment) \
	X(FPDFDoc_DeleteAttachment) \
	X(FPDFAttachment_SetDescription) \
	X(FPDFAttachment_SetStringValue) \
	X(FPDFCatalog_SetLanguage) \
	X(FPDF_CopyViewerPreferences)

#define PDFIUM_WRITER_DOCUMENT(X) \
	X(FPDF_CreateNewDocument)

#define PDFIUM_WRITER_SAVE(X) \
	X(FPDF_SaveAsCopy) \
	X(FPDF_SaveWithVersion)

#define PDFIUM_WRITER_PAGES(X) \
	X(FPDFPage_New) \
	X(FPDFPage_Delete) \
	X(FPDF_MovePages)

#define PDFIUM_WRITER_PAGE_IMPORT(X) \
	X(FPDF_ImportPagesByIndex) \
	X(FPDF_ImportPages)

#define PDFIUM_WRITER_PAGE_TEMPLATES(X) \
	X(FPDF_ImportNPagesToOne) \
	X(FPDF_NewXObjectFromPage) \
	X(FPDF_CloseXObject) \
	X(FPDF_NewFormObjectFromXObject) \
	X(FPDFPageObj_Destroy)

#define PDFIUM_WRITER_PAGE_GEOMETRY(X) \
	X(FPDFPage_SetRotation) \
	X(FPDFPage_SetMediaBox) \
	X(FPDFPage_SetCropBox) \
	X(FPDFPage_SetBleedBox) \
	X(FPDFPage_SetTrimBox) \
	X(FPDFPage_SetArtBox) \
	X(FPDFPage_TransFormWithClip) \
	X(FPDF_CreateClipPath) \
	X(FPDF_DestroyClipPath) \
	X(FPDFPage_InsertClipPath) \
	X(FPDFPage_TransformAnnots)

#define PDFIUM_WRITER_OBJECTS(X) \
	X(FPDFPage_CountObjects) \
	X(FPDFPage_GetObject) \
	X(FPDFPage_GenerateContent) \
	X(FPDFPage_InsertObjectAtIndex) \
	X(FPDFPage_RemoveObject) \
	X(FPDFPageObj_Destroy) \
	X(FPDFFormObj_CountObjects) \
	X(FPDFFormObj_GetObject) \
	X(FPDFFormObj_RemoveObject) \
	X(FPDFPageObj_GetType) \
	X(FPDFPageObj_SetMatrix) \
	X(FPDFPageObj_TransformF) \
	X(FPDFPageObj_TransformClipPath) \
	X(FPDFPageObj_SetIsActive) \
	X(FPDFPageObj_SetBlendMode) \
	X(FPDFPageObj_SetFillColor) \
	X(FPDFPageObj_SetStrokeColor) \
	X(FPDFPageObj_SetStrokeWidth) \
	X(FPDFPageObj_SetLineCap) \
	X(FPDFPageObj_SetLineJoin) \
	X(FPDFPageObj_SetDashArray)

#define PDFIUM_WRITER_PATHS(X) \
	X(FPDFPageObj_Destroy) \
	X(FPDFPageObj_CreateNewPath) \
	X(FPDFPageObj_CreateNewRect) \
	X(FPDFPath_MoveTo) \
	X(FPDFPath_LineTo) \
	X(FPDFPath_BezierTo) \
	X(FPDFPath_Close) \
	X(FPDFPath_SetDrawMode)

#define PDFIUM_WRITER_CONTENT_MARKS(X) \
	X(FPDFPageObj_CountMarks) \
	X(FPDFPageObj_GetMark) \
	X(FPDFPageObj_AddMark) \
	X(FPDFPageObj_AddExistingMark) \
	X(FPDFPageObj_RemoveMark) \
	X(FPDFPageObjMark_SetIntParam) \
	X(FPDFPageObjMark_SetFloatParam) \
	X(FPDFPageObjMark_SetStringParam) \
	X(FPDFPageObjMark_SetBlobParam) \
	X(FPDFPageObjMark_RemoveParam)

#define PDFIUM_WRITER_FONTS(X) \
	X(FPDFText_LoadStandardFont) \
	X(FPDFText_LoadFont) \
	X(FPDFText_LoadCidType2Font) \
	X(FPDFFont_Close)

#define PDFIUM_WRITER_TEXT(X) \
	X(FPDFPageObj_Destroy) \
	X(FPDFPageObj_CreateTextObj) \
	X(FPDFText_SetText) \
	X(FPDFText_SetCharcodes) \
	X(FPDFText_SetPositions) \
	X(FPDFTextObj_SetFontSize) \
	X(FPDFTextObj_SetTextRenderMode)

#define PDFIUM_WRITER_IMAGES(X) \
	X(FPDFPageObj_Destroy) \
	X(FPDFPageObj_NewImageObj) \
	X(FPDFImageObj_SetBitmap) \
	X(FPDFImageObj_LoadJpegFileInline)

#define PDFIUM_WRITER_ANNOTATIONS(X) \
	X(FPDFPage_GetAnnotCount) \
	X(FPDFPage_GetAnnot) \
	X(FPDFPage_GetAnnotIndex) \
	X(FPDFPage_CloseAnnot) \
	X(FPDFAnnot_GetSubtype) \
	X(FPDFAnnot_IsSupportedSubtype) \
	X(FPDFPage_CreateAnnot) \
	X(FPDFPage_RemoveAnnot) \
	X(FPDFAnnot_SetRect) \
	X(FPDFAnnot_SetColor) \
	X(FPDFAnnot_SetBorder) \
	X(FPDFAnnot_SetFlags) \
	X(FPDFAnnot_SetStringValue) \
	X(FPDFAnnot_SetAttachmentPoints) \
	X(FPDFAnnot_AppendAttachmentPoints) \
	X(FPDFAnnot_AddInkStroke) \
	X(FPDFAnnot_RemoveInkList) \
	X(FPDFAnnot_SetURI) \
	X(FPDFAnnot_GetFileAttachment) \
	X(FPDFAnnot_AddFileAttachment) \
	X(FPDFAttachment_SetFile)

#define PDFIUM_WRITER_ANNOTATION_APPEARANCE(X) \
	X(FPDFAnnot_SetAP) \
	X(FPDFAnnot_SetFontColor) \
	X(FPDFAnnot_IsObjectSupportedSubtype) \
	X(FPDFAnnot_GetObjectCount) \
	X(FPDFAnnot_GetObject) \
	X(FPDFAnnot_AppendObject) \
	X(FPDFAnnot_RemoveObject) \
	X(FPDFAnnot_UpdateObject)

#define PDFIUM_WRITER_FORMS(X) \
	X(FPDFDOC_InitFormFillEnvironment) \
	X(FPDFDOC_ExitFormFillEnvironment) \
	X(FORM_OnAfterLoadPage) \
	X(FORM_OnBeforeClosePage) \
	X(FPDF_GetFormType) \
	X(FPDFAnnot_GetFormFieldFlags) \
	X(FPDFAnnot_GetFormFieldType) \
	X(FPDFAnnot_GetFormFieldValue) \
	X(FPDFAnnot_GetOptionCount) \
	X(FPDFAnnot_IsOptionSelected) \
	X(FPDFAnnot_IsChecked) \
	X(FPDFAnnot_SetFormFieldFlags) \
	X(FORM_SetFocusedAnnot) \
	X(FORM_ForceToKillFocus) \
	X(FORM_SelectAllText) \
	X(FORM_ReplaceSelection) \
	X(FORM_SetIndexSelected) \
	X(FORM_OnChar)

#define PDFIUM_WRITER_FLATTEN(X) \
	X(FPDFPage_Flatten)

#define PDFIUM_WRITER_ATTACHMENTS(X) \
	X(FPDFDoc_GetAttachmentCount) \
	X(FPDFDoc_GetAttachment) \
	X(FPDFAttachment_GetName) \
	X(FPDFDoc_AddAttachment) \
	X(FPDFDoc_DeleteAttachment) \
	X(FPDFAttachment_SetFile) \
	X(FPDFAttachment_SetDescription) \
	X(FPDFAttachment_SetStringValue)

#define PDFIUM_WRITER_CATALOG(X) \
	X(FPDFCatalog_SetLanguage) \
	X(FPDF_CopyViewerPreferences)

#define PDFIUM_WRITER_GROUPS(X) \
	X(writer_document, PDFIUM_WRITER_DOCUMENT) \
	X(writer_save, PDFIUM_WRITER_SAVE) \
	X(writer_pages, PDFIUM_WRITER_PAGES) \
	X(writer_page_import, PDFIUM_WRITER_PAGE_IMPORT) \
	X(writer_page_templates, PDFIUM_WRITER_PAGE_TEMPLATES) \
	X(writer_page_geometry, PDFIUM_WRITER_PAGE_GEOMETRY) \
	X(writer_objects, PDFIUM_WRITER_OBJECTS) \
	X(writer_paths, PDFIUM_WRITER_PATHS) \
	X(writer_content_marks, PDFIUM_WRITER_CONTENT_MARKS) \
	X(writer_fonts, PDFIUM_WRITER_FONTS) \
	X(writer_text, PDFIUM_WRITER_TEXT) \
	X(writer_images, PDFIUM_WRITER_IMAGES) \
	X(writer_annotations, PDFIUM_WRITER_ANNOTATIONS) \
	X(writer_annotation_appearance, PDFIUM_WRITER_ANNOTATION_APPEARANCE) \
	X(writer_forms, PDFIUM_WRITER_FORMS) \
	X(writer_flatten, PDFIUM_WRITER_FLATTEN) \
	X(writer_attachments, PDFIUM_WRITER_ATTACHMENTS) \
	X(writer_catalog, PDFIUM_WRITER_CATALOG)
