# MarkdownLabel Theme Reference

`MarkdownLabel` is a GDExtension control that uses Godot's theme system for all visual styling. Its theme items belong to the `MarkdownLabel` theme type.

## Set the Theme Type Variation

```gdscript
var label: MarkdownLabel = MarkdownLabel.new()
label.theme_type_variation = &"MarkdownLabel"
```

Then configure the theme items below in the editor's theme panel or in a script.

---

## Supported Theme Properties

### Fonts

| Property | Type | Description | Fallback |
|--------|------|------|------|
| `text_font` | Font | Default body font | — |
| `bold_font` | Font | Bold font | — |
| `bold_italic_font` | Font | Bold italic font (`***text***`) | `text_font` |
| `italic_font` | Font | Italic font (`*text*`) | `text_font` |
| `code_text_font` | Font | Inline code font | `text_font` |
| `code_block_font` | Font | Code block font | `code_text_font` |
| `list_marker_font` | Font | List marker font | `text_font` |
| `table_header_font` | Font | Table header font | `text_font` |
| `table_cell_font` | Font | Table cell font | `text_font` |
| `h1_font` … `h6_font` | Font | Fonts for heading levels 1–6 | `text_font` |
| `footnote_ref_font` | Font | Footnote reference marker font (`[^id]`) | `text_font` |
| `footnote_text_font` | Font | Body font for the footnote definitions | `text_font` |

### Font Sizes

| Property | Type | Default |
|--------|------|--------|
| `text_font_size` | int | 16 |
| `code_text_font_size` | int | 16 |
| `code_block_font_size` | int | 16 |
| `list_marker_font_size` | int | 16 |
| `table_header_font_size` | int | 16 |
| `table_cell_font_size` | int | 16 |
| `h1_font_size` … `h6_font_size` | int | 28, 24, 21, 19, 17, 15 |
| `footnote_ref_font_size` | int | `text_font_size` − 4 (about 12) |
| `footnote_text_font_size` | int | `text_font_size` (16) |

`MarkdownLabel.extra_font_size` is added to all font sizes. It defaults to 0.

### Colors

| Property | Type | Default |
|--------|------|--------|
| `text_font_color` | Color | `Color.WHITE` |
| `link_color` | Color | `Color(0.36, 0.62, 1.0)` |
| `selection_color` | Color | `Color(0.1, 0.1, 1.0, 0.8)` |
| `code_text_font_color` | Color | `Color.WHITE` |
| `code_block_font_color` | Color | `Color(0.86, 0.88, 0.91)` |
| `highlight_color` | Color | `Color(1.0, 0.92, 0.35, 0.5)` |
| `highlight_font_color` | Color | `text_font_color` |
| `strikethrough_color` | Color | `Color(0.7, 0.7, 0.7)` |
| `list_marker_color` | Color | `text_font_color` |
| `table_header_font_color` | Color | `text_font_color` |
| `table_cell_font_color` | Color | `text_font_color` |
| `h1_font_color` … `h6_font_color` | Color | `text_font_color` |
| `footnote_ref_font_color` | Color | `link_color` |
| `footnote_text_font_color` | Color | `text_font_color` |

### StyleBoxes

| Property | Type | Description |
|--------|------|------|
| `selection_stylebox` | StyleBox | Background of selected text |
| `inline_code` | StyleBox | Inline code background |
| `code_block_panel` | StyleBox | Code block background and border |
| `blockquote_panel` | StyleBox | Blockquote panel with a colored left border |
| `blockquote_nested` | StyleBox | Transparent panel for nested blockquotes |
| `search_result` | StyleBox | Search match background |
| `highlight` | StyleBox | Background of highlighted text |
| `separator` | StyleBox | Horizontal separator style |
| `table_panel` | StyleBox | Optional outer border of the table |
| `table_header_panel` | StyleBox | Table header cell background and border |
| `table_cell_panel` | StyleBox | Regular table cell background and border |
| `table_striped_panel` | StyleBox | Striped row cell background and border |
| `footnote_ref` | StyleBox | Optional superscript background for footnote reference markers |

Missing StyleBoxes use their built-in fallback styles where provided. Set `table_panel` to draw an outer border around the table.

### Constants

#### Basic Layout

| Property | Type | Default | Description |
|--------|------|--------|------|
| `line_separation` | int | 2 | Line spacing in pixels |
| `paragraph_separation` | int | 10 | Paragraph spacing in pixels |

#### Lists

| Property | Type | Default | Description |
|--------|------|--------|------|
| `list_indent` | int | 26 | List indentation |
| `list_marker_gap` | int | 6 | Gap between the marker and the text |

#### Blockquotes

| Property | Type | Default | Description |
|--------|------|--------|------|
| `blockquote_indent` | int | 25 | Blockquote indentation |
| `blockquote_line_gap` | int | 14 | Gap between lines within a blockquote |

#### Tables

| Property | Type | Default | Description |
|--------|------|--------|------|
| `table_striped` | int | 0 | Row striping interval. `0` disables striping. `2` alternates styles every two rows. |

#### Block Spacing

Each block type has its own spacing before and after it. The properties are named `<block_type>_space_before` and `<block_type>_space_after`.

| Property | Type | Default | Block Type |
|--------|------|--------|------------|
| `heading_space_before` | int | 0 | Headings of all levels share this property |
| `heading_space_after` | int | `paragraph_separation` | Headings |
| `code_block_space_before` | int | 0 | Code blocks |
| `code_block_space_after` | int | `paragraph_separation` | Code blocks |
| `table_space_before` | int | 0 | Tables |
| `table_space_after` | int | `paragraph_separation` | Tables |
| `blockquote_space_before` | int | 0 | Blockquotes |
| `blockquote_space_after` | int | `paragraph_separation` | Blockquotes |
| `list_space_before` | int | 0 | Ordered, unordered, and task lists |
| `list_space_after` | int | `paragraph_separation` | Lists |
| `text_space_before` | int | 0 | Regular paragraphs |
| `text_space_after` | int | `paragraph_separation` | Regular paragraphs |
| `footnotes_space_before` | int | 0 | Footnote definitions |
| `footnotes_space_after` | int | `paragraph_separation` | Footnote definitions |

### Icons

| Property | Type | Description |
|--------|------|------|
| `task_checked` | Texture2D | Checkbox icon for completed tasks |
| `task_unchecked` | Texture2D | Checkbox icon for incomplete tasks |
| `file_broken` | Texture2D | Placeholder for images that fail to load or use an unsupported format |

Unset icons fall back to the addon's built-in icons.

---

## Inline Font Selection

For each parsed inline span, the control selects a font in this order.

1. Footnote references (`[^id]`) use `footnote_ref_font` and `footnote_ref_font_size`, with a superscript offset.
2. Inline code (`` `code` ``) uses `code_text_font`.
3. Bold italic text (`***text***`) uses `bold_italic_font`.
4. Bold text (`**text**`) uses `bold_font`.
5. Italic text (`*text*`) uses `italic_font`.
6. Other spans use the current block's default font, such as `text_font` for body text or `h1_font` for a level-one heading.

> Footnote reference markers use `footnote_ref_font_color` and are drawn as superscripts with a vertical offset of `footnote_ref_font_size × 0.35`. Set the `footnote_ref` StyleBox to add a background. The footnote definitions at the end of the document (the `[^id]: text` list) use `footnote_text_font`, `footnote_text_font_size`, and `footnote_text_font_color`.

---

## Example Theme Configuration

This example configures a custom theme for a `MarkdownLabel`. You can use it in the `_ready()` method of a `Control` node.

```gdscript
func _ready() -> void:
	var label: MarkdownLabel = %MarkdownLabel
	label.theme_type_variation = &"MarkdownLabel"

	# Fonts
	var default_font: FontFile = load("res://fonts/NotoSansSC-Regular.ttf")
	var bold_font: FontFile = load("res://fonts/NotoSansSC-Bold.ttf")
	var italic_font: FontFile = load("res://fonts/NotoSansSC-Italic.ttf")
	var bold_italic_font: FontFile = load("res://fonts/NotoSansSC-BoldItalic.ttf")
	var mono_font: FontFile = load("res://fonts/JetBrainsMono-Regular.ttf")

	label.add_theme_font_override("text_font", default_font)
	label.add_theme_font_override("bold_font", bold_font)
	label.add_theme_font_override("italic_font", italic_font)
	label.add_theme_font_override("bold_italic_font", bold_italic_font)
	label.add_theme_font_override("code_text_font", mono_font)
	label.add_theme_font_override("code_block_font", mono_font)

	# Font sizes
	label.add_theme_font_size_override("text_font_size", 15)
	label.add_theme_font_size_override("code_text_font_size", 14)
	label.add_theme_font_size_override("code_block_font_size", 14)
	label.add_theme_font_size_override("footnote_ref_font_size", 11)
	label.add_theme_font_size_override("footnote_text_font_size", 13)

	# Colors
	label.add_theme_color_override("text_font_color", Color(0.9, 0.9, 0.9))
	label.add_theme_color_override("link_color", Color(0.3, 0.6, 1.0))
	label.add_theme_color_override("code_text_font_color", Color(0.9, 0.88, 0.5))
	label.add_theme_color_override("code_block_font_color", Color(0.85, 0.86, 0.9))
	label.add_theme_color_override("footnote_ref_font_color", Color(0.4, 0.65, 1.0))
	label.add_theme_color_override("footnote_text_font_color", Color(0.75, 0.75, 0.75))

	# Spacing
	label.add_theme_constant_override("line_separation", 2)
	label.add_theme_constant_override("paragraph_separation", 12)
	label.add_theme_constant_override("blockquote_border_width", 4)
	label.add_theme_constant_override("list_indent", 24)
```

---

## Notes

- Set `theme_type_variation` to `"MarkdownLabel"` for theme lookups. The control uses its built-in defaults when the variation is unset.
- Unset fonts and colors fall back to the defaults shown in the tables.
- Font sizes and colors are set by block type. Inline formatting such as bold, italic, bold italic, and code switches fonts while sharing the current block's font size.
- Configure GDExtension theme items in the editor's theme panel or at runtime with `add_theme_*_override()`.
