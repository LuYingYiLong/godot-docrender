# MermaidLabel 主题参考

示例主题使用深色节点、浅色文字和蓝色连线。打开资源即可在 Godot 中编辑四项图表颜色。[示例主题](../example/mermaid_label_theme.tres)

## 应用主题

1. 添加 `MermaidLabel` 节点，在检查器的 **Text** 中填写 Mermaid 源码。
2. 将 `mermaid_label_theme.tres` 拖入节点的 **Theme → Theme** 属性。多个图表可从同一个父 `Control` 继承主题。
3. 打开主题资源，在主题编辑器中选择 **MermaidLabel → Colors**。调整颜色后，图表会自动重绘。

## 主题颜色

| 主题项 | 控制的图表元素 |
| --- | --- |
| `node_background_color` | 流程图节点、时序图参与者等元素的填充色 |
| `node_border_color` | 图表节点的纯色描边 |
| `node_text_color` | 节点文字以及采用通用文字色的标签 |
| `line_color` | 连线和箭头，以及时序图中的消息线 |

**Theme Mode** 选择默认色板，四项主题颜色可逐项覆盖。**Auto** 根据 `font_color` 的明暗选择色板。

饼图扇区等采用系列色的元素使用 Mermaid 的系列色。源码里的 `style`、`classDef` 和 `linkStyle` 可为个别元素指定颜色。[Mermaid 主题配置](https://mermaid.js.org/config/theming.html) · [流程图样式语法](https://mermaid.js.org/syntax/flowchart.html#styling-and-classes)

## 单独调整一个图表

在脚本中可以用 `add_theme_color_override()` 覆盖单个节点的颜色。下面的脚本可挂在已应用主题的 `MermaidLabel` 上。

```gdscript
extends MermaidLabel


func _ready() -> void:
	text = "flowchart LR\nA[开始] --> B[完成]"
	add_theme_color_override(&"line_color", Color(0.35, 0.72, 0.92))
```
