# LatexLabel 主题参考

示例主题使用深色背景、24 px 的浅色公式和蓝色焦点边框，可在 Godot 主题编辑器中修改。[`latex_label_theme.tres`](../example/latex_label_theme.tres)

## 应用主题

1. 在场景中添加 `LatexLabel` 节点，在检查器的 `Text` 中填写 LaTeX，例如 `\frac{1}{\sqrt{x^2+1}}`。
2. 将文件系统中的 `latex_label_theme.tres` 拖入节点检查器的 **Theme → Theme** 属性。多个公式共用一套样式时，也可以把主题赋给它们的父 `Control`。
3. 打开主题资源，在主题编辑器中选择 `LatexLabel` 类型，就可以修改公式颜色、字号和样式盒。

## 主题属性

| 类型 | 属性 | 示例主题 | 作用 |
| --- | --- | --- | --- |
| Font Size | `font_size` | `24` | 公式渲染的基准字号，单位为像素。 |
| Color | `font_color` | 浅色 | 公式的前景色。 |
| StyleBox | `normal` | 深色圆角背景 | 绘制常态背景。其内容边距参与最小尺寸和公式对齐计算。 |
| StyleBox | `focus` | 深色背景和蓝色边框 | 获得焦点时替换 `normal` 的背景绘制。公式布局沿用 `normal` 的内容边距。 |

如需显示焦点样式，将节点的 `Focus Mode` 设为 `All` 后用 Tab 键聚焦。公式字形由内置的数学字体绘制。`LabelSettings` 中的 `font_size` 和 `font_color` 优先于主题及节点的主题覆盖项。主题缺少某个样式项时，控件会查询 `Label` 的同名主题项。

## 覆盖单个节点的主题项

将下面的脚本挂在 `LatexLabel` 上，可以加载示例主题，并单独设置这个节点的字号和颜色。

```gdscript
extends LatexLabel

const EXAMPLE_THEME: Theme = preload("uid://fuqrhbfft60e")


func _ready() -> void:
	theme = EXAMPLE_THEME
	text = r"\frac{1}{\sqrt{x^2+1}}"
	add_theme_font_size_override(&"font_size", 30)
	add_theme_color_override(&"font_color", Color(0.95, 0.97, 1.0))
```
