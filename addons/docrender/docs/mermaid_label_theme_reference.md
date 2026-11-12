# MermaidLabel Theme Reference

The example theme uses dark nodes, light text, and blue connectors. Open the resource in Godot to edit its four diagram colors. [Example theme](../example/mermaid_label_theme.tres)

## Apply the Theme

1. Add a `MermaidLabel` node and enter the Mermaid source in **Text** in the Inspector.
2. Drag `mermaid_label_theme.tres` to the node's **Theme → Theme** property. Several diagrams can inherit the theme from the same parent `Control`.
3. Open the theme resource and select **MermaidLabel → Colors** in the theme editor. The diagram redraws automatically when you change the colors.

## Theme Colors

| Theme Item | Diagram Elements |
| --- | --- |
| `node_background_color` | Fill color for elements such as flowchart nodes and sequence diagram participants |
| `node_border_color` | Solid outline color for diagram nodes |
| `node_text_color` | Node text and labels that use the general text color |
| `line_color` | Connectors, arrows, and message lines in sequence diagrams |

**Theme Mode** selects the default palette. You can override each of the four theme colors individually. **Auto** chooses a palette based on the luminance of `font_color`.

Elements with series colors, such as pie slices, use Mermaid's series palette. Use `style`, `classDef`, or `linkStyle` in the source to set colors for specific elements. [Mermaid theming](https://mermaid.js.org/config/theming.html) · [Flowchart styling and classes](https://mermaid.js.org/syntax/flowchart.html#styling-and-classes)

## Customize One Diagram

Use `add_theme_color_override()` to override a color on one node. You can attach this script to a `MermaidLabel` that already has a theme.

```gdscript
extends MermaidLabel


func _ready() -> void:
	text = "flowchart LR\nA[Start] --> B[Done]"
	add_theme_color_override(&"line_color", Color(0.35, 0.72, 0.92))
```
