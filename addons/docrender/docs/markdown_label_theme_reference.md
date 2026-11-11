# MarkdownLabel 主题参考

`MarkdownLabel` 是一个 GDExtension 控件，通过 Godot 主题系统统一管理所有视觉样式。所有主题属性均使用 `MarkdownLabel` 作为主题类型名。

## 设置主题类型变体

```gdscript
var label: MarkdownLabel = MarkdownLabel.new()
label.theme_type_variation = &"MarkdownLabel"
```

之后即可通过编辑器主题面板或脚本设置以下主题属性。

---

## 支持的主题属性

### 字体 Fonts

| 属性名 | 类型 | 说明 | 回退 |
|--------|------|------|------|
| `text_font` | Font | 正文默认字体 | — |
| `bold_font` | Font | 粗体字体 | — |
| `bold_italic_font` | Font | 粗斜体字体（`***text***`） | `text_font` |
| `italic_font` | Font | 斜体字体（`*text*`） | `text_font` |
| `code_text_font` | Font | 行内代码字体 | `text_font` |
| `code_block_font` | Font | 代码块字体 | `code_text_font` |
| `list_marker_font` | Font | 列表标记字体 | `text_font` |
| `table_header_font` | Font | 表格表头字体 | `text_font` |
| `table_cell_font` | Font | 表格单元格字体 | `text_font` |
| `h1_font` … `h6_font` | Font | 标题 1–6 级字体 | `text_font` |
| `footnote_ref_font` | Font | 脚注引用标记字体（`[^id]`） | `text_font` |
| `footnote_text_font` | Font | 脚注定义区域正文字体 | `text_font` |

### 字体大小 Font Sizes

| 属性名 | 类型 | 默认值 |
|--------|------|--------|
| `text_font_size` | int | 16 |
| `code_text_font_size` | int | 16 |
| `code_block_font_size` | int | 16 |
| `list_marker_font_size` | int | 16 |
| `table_header_font_size` | int | 16 |
| `table_cell_font_size` | int | 16 |
| `h1_font_size` … `h6_font_size` | int | 28, 24, 21, 19, 17, 15 |
| `footnote_ref_font_size` | int | `text_font_size` − 4（约 12） |
| `footnote_text_font_size` | int | `text_font_size`（16） |

所有字号会额外加上 `MarkdownLabel.extra_font_size` 的值（默认为 0）。

### 颜色 Colors

| 属性名 | 类型 | 默认值 |
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

### 样式盒 StyleBoxes

| 属性名 | 类型 | 说明 |
|--------|------|------|
| `selection_stylebox` | StyleBox | 文本选区背景 |
| `inline_code` | StyleBox | 行内代码背景 |
| `code_block_panel` | StyleBox | 代码块整体背景和边框 |
| `blockquote_panel` | StyleBox | 引用块面板，左侧有彩色边框 |
| `blockquote_nested` | StyleBox | 嵌套引用面板，无额外背景色 |
| `search_result` | StyleBox | 搜索高亮背景 |
| `highlight` | StyleBox | 高亮文本的背景 |
| `separator` | StyleBox | 水平分隔线样式 |
| `table_panel` | StyleBox | 表格整体边框（无默认值） |
| `table_header_panel` | StyleBox | 表头单元格背景和边框 |
| `table_cell_panel` | StyleBox | 普通单元格背景和边框 |
| `table_striped_panel` | StyleBox | 条纹行单元格背景和边框 |
| `footnote_ref` | StyleBox | 脚注引用标记的上标背景（未设置时不绘制背景） |

以上 StyleBox 如果未设置主题值，会使用代码内置的默认样式，确保开箱即用。`table_panel` 例外 — 未设置时不绘制表格外边框。

### 常量 Constants

#### 基础布局

| 属性名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `line_separation` | int | 2 | 行间距（像素） |
| `paragraph_separation` | int | 10 | 段落间距（像素） |

#### 列表

| 属性名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `list_indent` | int | 26 | 列表整体缩进 |
| `list_marker_gap` | int | 6 | 标记与文本之间的间距 |

#### 引用块

| 属性名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `blockquote_indent` | int | 25 | 引用块缩进 |
| `blockquote_line_gap` | int | 14 | 引用块内各行之间的间距 |

#### 表格

| 属性名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `table_striped` | int | 0 | 斑马条纹间隔（0 = 不启用。设为 2 表示每 2 行交替一次） |

#### 块间距

每个块类型支持独立的段前 / 段后间距，命名规则为 `<块类型>_space_before` 和 `<块类型>_space_after`：

| 属性名 | 类型 | 默认值 | 适用块类型 |
|--------|------|--------|------------|
| `heading_space_before` | int | 0 | 标题（所有级别共用同一属性） |
| `heading_space_after` | int | `paragraph_separation` | 标题 |
| `code_block_space_before` | int | 0 | 代码块 |
| `code_block_space_after` | int | `paragraph_separation` | 代码块 |
| `table_space_before` | int | 0 | 表格 |
| `table_space_after` | int | `paragraph_separation` | 表格 |
| `blockquote_space_before` | int | 0 | 引用块 |
| `blockquote_space_after` | int | `paragraph_separation` | 引用块 |
| `list_space_before` | int | 0 | 列表（含有序、无序、任务列表） |
| `list_space_after` | int | `paragraph_separation` | 列表 |
| `text_space_before` | int | 0 | 普通段落 |
| `text_space_after` | int | `paragraph_separation` | 普通段落 |
| `footnotes_space_before` | int | 0 | 脚注定义区域 |
| `footnotes_space_after` | int | `paragraph_separation` | 脚注定义区域 |

### 图标 Icons

| 属性名 | 类型 | 说明 |
|--------|------|------|
| `task_checked` | Texture2D | 已完成任务复选框图标 |
| `task_unchecked` | Texture2D | 未完成任务复选框图标 |
| `file_broken` | Texture2D | 图片加载失败或不支持时显示的占位图标 |

未设置时使用插件内置的默认图标。

---

## 行内格式字体选择规则

对于解析后的行内 span，字体按以下优先级选择：

1. 如果 span 是脚注引用（`[^id]`）→ 使用 `footnote_ref_font`，且字号使用 `footnote_ref_font_size`，并以上标偏移绘制
2. 如果 span 是行内代码（`` `code` ``）→ 使用 `code_text_font`
3. 如果 span 同时是粗体和斜体（`***text***`）→ 使用 `bold_italic_font`
4. 如果 span 是粗体（`**text**`）→ 使用 `bold_font`
5. 如果 span 是斜体（`*text*`）→ 使用 `italic_font`
6. 否则 → 使用当前块类型的默认字体（如正文用 `text_font`，标题用 `h1_font` 等）

> 脚注引用标记会以上标形式渲染（垂直偏移 = `footnote_ref_font_size × 0.35`），颜色使用 `footnote_ref_font_color`，并可选择通过 `footnote_ref` StyleBox 添加背景。脚注定义区域（文档末尾的 `[^id]: text` 列表）使用 `footnote_text_font`、`footnote_text_font_size` 和 `footnote_text_font_color`。

---

## 主题配置示例

以下示例展示了如何在脚本中为 `MarkdownLabel` 设置完整的自定义主题。此代码可放在任意 Control 节点的 `_ready()` 中执行。

```gdscript
func _ready() -> void:
    var label: MarkdownLabel = %MarkdownLabel
    label.theme_type_variation = &"MarkdownLabel"

    # 字体
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

    # 字号
    label.add_theme_font_size_override("text_font_size", 15)
    label.add_theme_font_size_override("code_text_font_size", 14)
    label.add_theme_font_size_override("code_block_font_size", 14)
    label.add_theme_font_size_override("footnote_ref_font_size", 11)
    label.add_theme_font_size_override("footnote_text_font_size", 13)

    # 颜色
    label.add_theme_color_override("text_font_color", Color(0.9, 0.9, 0.9))
    label.add_theme_color_override("link_color", Color(0.3, 0.6, 1.0))
    label.add_theme_color_override("code_text_font_color", Color(0.9, 0.88, 0.5))
    label.add_theme_color_override("code_block_font_color", Color(0.85, 0.86, 0.9))
    label.add_theme_color_override("footnote_ref_font_color", Color(0.4, 0.65, 1.0))
    label.add_theme_color_override("footnote_text_font_color", Color(0.75, 0.75, 0.75))

    # 间距
    label.add_theme_constant_override("line_separation", 2)
    label.add_theme_constant_override("paragraph_separation", 12)
    label.add_theme_constant_override("blockquote_border_width", 4)
    label.add_theme_constant_override("list_indent", 24)
```

---

## 注意事项

- 所有主题属性均绑定在 `"MarkdownLabel"` 类型上。如果未设置 `theme_type_variation`，主题查找会失败，控件将使用代码内置的默认值。
- 未显式设置的字体和颜色会自动回退到合理的默认值（参见各表"回退"列），无需逐项配置即可正常显示。
- 字体大小和颜色按块类型粒度控制，行内格式（粗体、斜体、粗斜体、代码）共用当前块的字体大小，仅切换字体。
- 由于 GDExtension 的限制，主题属性必须通过 `add_theme_*_override()` 在运行时设置，或通过编辑器主题面板配置。无法像 GDScript 插件那样通过 `Theme` 资源直接注册。
